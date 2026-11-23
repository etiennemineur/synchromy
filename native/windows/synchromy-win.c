/* Synchromy — version Windows autonome : application (Synchromy.exe) et économiseur d'écran (Synchromy.scr).
   Le même moteur en C que l'économiseur macOS et l'app Apple TV (synchromy.c) calcule chaque image en pixels et la musique ;
   cette coque pose l'image à l'écran, agrandie sans lissage, et envoie le son à la sortie audio par WASAPI, en stéréo,
   en 5.1 ou en 7.1 selon la disposition des enceintes déclarée par Windows.

   Arguments (convention des économiseurs Windows) :
     /s           économiseur, plein écran ; le moindre geste l'arrête
     /p <fenêtre> aperçu dans la petite fenêtre du panneau de configuration (sans son)
     /c           réglages (musique, volume)
     sans argument : réglages si le fichier s'appelle *.scr, sinon l'application en plein écran.
   Application : Échap quitte, F ou F11 bascule plein écran / fenêtre, M coupe ou rend le son, N change de tableau. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <dwmapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "synchromy.h"

/* les identifiants COM du son, déclarés ici pour ne dépendre d'aucune bibliothèque d'identifiants */
static const GUID kCLSID_Enum   = { 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
static const GUID kIID_Enum     = { 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };
static const GUID kIID_Client   = { 0x1CB9AD4C, 0xDBFA, 0x4C32, { 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2 } };
static const GUID kIID_Render   = { 0xF294ACFC, 0x3146, 0x4483, { 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2 } };
static const GUID kSUB_FLOAT    = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };
static const GUID kSUB_PCM      = { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };

enum { MODE_APP, MODE_ECONOMISEUR, MODE_APERCU, MODE_REGLAGES };
static int mode = MODE_APP;
static Syn *moteur;
static HWND fenetre;
static double tailleW, tailleH;
static LARGE_INTEGER freqHorloge, depart;
static BITMAPINFO bmi;
static uint32_t *tampon; static int tamponN;
static POINT sourisDepart; static int sourisConnue;
static int pleinEcran = 1; static RECT rectFenetre;
static DWORD dernierGeste;

/* ---------- réglages : HKCU\Software\Synchromy ---------- */
static DWORD lireReglage(const char *nom, DWORD defaut){
  HKEY k; DWORD v = defaut, t = sizeof v;
  if(RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Synchromy", 0, KEY_READ, &k) == ERROR_SUCCESS){ RegQueryValueExA(k, nom, NULL, NULL, (BYTE *)&v, &t); RegCloseKey(k); }
  return v;
}
static void ecrireReglage(const char *nom, DWORD v){
  HKEY k; if(RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Synchromy", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL) == ERROR_SUCCESS){ RegSetValueExA(k, nom, 0, REG_DWORD, (BYTE *)&v, sizeof v); RegCloseKey(k); }
}

/* ---------- le son : WASAPI, mode partagé, réveillé par événement ---------- */
static IAudioClient *client; static IAudioRenderClient *rendu; static HANDLE evenement, filAudio; static volatile LONG enMarche;
static int canaux, flottant, bits; static UINT32 nTampon; static float matrice[8*6]; static float *tamponAudio;
/* une enceinte de Windows, dans la langue du moteur. Avec des enceintes latérales (7.1), l'arrière devient l'arrière ;
   sans elles (5.1 « back »), l'arrière tient le rôle des surrounds. */
static int etiquettes(DWORD masque, int n, int *etiq){
  int lat = (masque & (SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT)) != 0, i = 0, surround = 0;
  for(int b = 0; b < 32 && i < n; b++){ DWORD bit = 1u << b; if(!(masque & bit)) continue; int e = SYN_AUTRE;
    switch(bit){
      case SPEAKER_FRONT_LEFT: e = SYN_L; break; case SPEAKER_FRONT_RIGHT: e = SYN_R; break;
      case SPEAKER_FRONT_CENTER: e = SYN_C; break; case SPEAKER_LOW_FREQUENCY: e = SYN_LFE; break;
      case SPEAKER_BACK_LEFT: e = lat ? SYN_LB : SYN_LS; break; case SPEAKER_BACK_RIGHT: e = lat ? SYN_RB : SYN_RS; break;
      case SPEAKER_SIDE_LEFT: e = SYN_LS; break; case SPEAKER_SIDE_RIGHT: e = SYN_RS; break;
      case SPEAKER_BACK_CENTER: e = SYN_CB; break;
      case SPEAKER_FRONT_LEFT_OF_CENTER: e = SYN_L; break; case SPEAKER_FRONT_RIGHT_OF_CENTER: e = SYN_R; break;
    }
    etiq[i++] = e; if(e >= SYN_C && e <= SYN_CB) surround = 1; }
  for(; i < n; i++) etiq[i] = SYN_AUTRE;
  return surround;
}
static DWORD WINAPI boucleAudio(LPVOID x){
  (void)x; CoInitializeEx(NULL, COINIT_MULTITHREADED);
  DWORD tache = 0; HANDLE h = NULL; HMODULE av = LoadLibraryA("avrt.dll");   /* priorité « audio » du planificateur, si elle existe */
  if(av){ HANDLE (WINAPI *f)(LPCSTR, LPDWORD) = (void *)GetProcAddress(av, "AvSetMmThreadCharacteristicsA"); if(f) h = f("Pro Audio", &tache); }
  while(enMarche){
    if(WaitForSingleObject(evenement, 200) != WAIT_OBJECT_0) continue;
    UINT32 att = 0; if(FAILED(IAudioClient_GetCurrentPadding(client, &att))) break;
    UINT32 n = nTampon - att; if(n == 0) continue;
    BYTE *d = NULL; if(FAILED(IAudioRenderClient_GetBuffer(rendu, n, &d))) break;
    if(flottant) syn_audio_canaux(moteur, (float *)d, (int)n, canaux, matrice);
    else { syn_audio_canaux(moteur, tamponAudio, (int)n, canaux, matrice);
      if(bits == 16){ int16_t *o = (int16_t *)d; for(UINT32 i = 0; i < n*canaux; i++){ float v = tamponAudio[i]; o[i] = (int16_t)lrintf((v > 1 ? 1 : v < -1 ? -1 : v)*32767); } }
      else { int32_t *o = (int32_t *)d; for(UINT32 i = 0; i < n*canaux; i++){ float v = tamponAudio[i]; o[i] = (int32_t)llrint((double)(v > 1 ? 1 : v < -1 ? -1 : v)*2147483647.0); } } }
    IAudioRenderClient_ReleaseBuffer(rendu, n, 0);
  }
  if(av){ BOOL (WINAPI *g)(HANDLE) = (void *)GetProcAddress(av, "AvRevertMmThreadCharacteristics"); if(g && h) g(h); }
  CoUninitialize(); return 0;
}
static void couperSon(void){
  if(filAudio){ InterlockedExchange(&enMarche, 0); WaitForSingleObject(filAudio, 2000); CloseHandle(filAudio); filAudio = NULL; }
  if(client){ IAudioClient_Stop(client); }
  if(rendu){ IAudioRenderClient_Release(rendu); rendu = NULL; }
  if(client){ IAudioClient_Release(client); client = NULL; }
  if(evenement){ CloseHandle(evenement); evenement = NULL; }
  free(tamponAudio); tamponAudio = NULL;
}
static void lancerSon(void){
  if(client || !moteur || mode == MODE_APERCU || !lireReglage("son", 1)) return;
  IMMDeviceEnumerator *en = NULL; IMMDevice *dev = NULL; WAVEFORMATEX *wf = NULL;
  if(FAILED(CoCreateInstance(&kCLSID_Enum, NULL, CLSCTX_ALL, &kIID_Enum, (void **)&en))) return;
  if(FAILED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(en, eRender, eConsole, &dev))){ IMMDeviceEnumerator_Release(en); return; }
  if(FAILED(IMMDevice_Activate(dev, &kIID_Client, CLSCTX_ALL, NULL, (void **)&client))) goto fin;
  if(FAILED(IAudioClient_GetMixFormat(client, &wf))) goto echec;
  canaux = wf->nChannels; bits = wf->wBitsPerSample; flottant = wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT; DWORD masque = canaux >= 2 ? (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT) : SPEAKER_FRONT_CENTER;
  if(wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE){ WAVEFORMATEXTENSIBLE *x = (WAVEFORMATEXTENSIBLE *)wf; masque = x->dwChannelMask;
    flottant = IsEqualGUID(&x->SubFormat, &kSUB_FLOAT); if(!flottant && !IsEqualGUID(&x->SubFormat, &kSUB_PCM)) goto echec; }
  if(!flottant && bits != 16 && bits != 32) goto echec;
  if(canaux < 1 || canaux > 8) goto echec;
  /* stéréo, ou au-delà : la disposition des enceintes déclarée par Windows décide (5.1, 7.1 par HDMI ou carte son) */
  int etiq[8]; int surround = etiquettes(masque, canaux, etiq), cMoteur = canaux >= 3 && surround ? canaux : 2;
  syn_activer_son_canaux(moteur, wf->nSamplesPerSec, lireReglage("volume", 60)/100.0, cMoteur);
  syn_sortie(moteur, cMoteur);
  memset(matrice, 0, sizeof matrice);
  if(cMoteur > 2) syn_matrice(canaux, etiq, matrice);
  else { /* le moteur en stéréo : gauche et droite, le reste du 5.1 replié dessus ; une sortie mono reçoit la somme */
    for(int c = 0; c < canaux; c++){ float *m = matrice + 6*c;
      if(canaux == 1){ m[0] = m[1] = .5f; }
      else if(etiq[c] == SYN_L || (c == 0 && etiq[c] == SYN_AUTRE)){ m[0] = 1; m[2] = .7f; m[4] = .7f; }
      else if(etiq[c] == SYN_R || (c == 1 && etiq[c] == SYN_AUTRE)){ m[1] = 1; m[2] = .7f; m[5] = .7f; } } }
  REFERENCE_TIME duree = 400000;   /* 40 ms */
  if(FAILED(IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, duree, 0, wf, NULL))) goto echec;
  evenement = CreateEventA(NULL, FALSE, FALSE, NULL); IAudioClient_SetEventHandle(client, evenement);
  IAudioClient_GetBufferSize(client, &nTampon);
  if(FAILED(IAudioClient_GetService(client, &kIID_Render, (void **)&rendu))) goto echec;
  tamponAudio = malloc(sizeof(float)*nTampon*canaux);
  { REFERENCE_TIME lat = 0; IAudioClient_GetStreamLatency(client, &lat);   /* l'image attend le son */
    syn_latence(moteur, (double)nTampon/wf->nSamplesPerSec + lat/1e7 + .02); }
  { BYTE *d; if(SUCCEEDED(IAudioRenderClient_GetBuffer(rendu, nTampon, &d))){ memset(d, 0, (size_t)nTampon*wf->nBlockAlign); IAudioRenderClient_ReleaseBuffer(rendu, nTampon, AUDCLNT_BUFFERFLAGS_SILENT); } }
  InterlockedExchange(&enMarche, 1); filAudio = CreateThread(NULL, 0, boucleAudio, NULL, 0, NULL);
  IAudioClient_Start(client);
  goto fin;
echec:
  if(client){ IAudioClient_Release(client); client = NULL; }
fin:
  if(wf) CoTaskMemFree(wf);
  if(dev) IMMDevice_Release(dev);
  IMMDeviceEnumerator_Release(en);
}

/* ---------- l'image ---------- */
static double maintenant(void){ LARGE_INTEGER t; QueryPerformanceCounter(&t); return (double)(t.QuadPart - depart.QuadPart)*1000.0/freqHorloge.QuadPart; }
/* la taille en points, comme les pixels CSS de la page ; une petite fenêtre est calculée comme un écran de 1280 points */
static void tailleVirtuelle(HWND w, double *tw, double *th){
  RECT r; GetClientRect(w, &r); double W = r.right - r.left, H = r.bottom - r.top;
  UINT dpi = 96; HMODULE u = GetModuleHandleA("user32.dll"); UINT (WINAPI *g)(HWND) = u ? (void *)GetProcAddress(u, "GetDpiForWindow") : NULL; if(g) dpi = g(w);
  double k = dpi/96.0; W /= k; H /= k;
  if(W < 1 || H < 1){ W = 1440; H = 900; }
  if(W < 1000){ double q = 1280/W; W = 1280; H *= q; }
  *tw = W; *th = H;
}
static void preparer(void){
  double W, H; tailleVirtuelle(fenetre, &W, &H);
  if(moteur && fabs(W - tailleW) < 1 && fabs(H - tailleH) < 1) return;
  int avaitSon = client != NULL; couperSon(); syn_detruire(moteur);
  LARGE_INTEGER t; QueryPerformanceCounter(&t);
  uint64_t graine = ((uint64_t)GetTickCount() << 32) ^ (uint64_t)t.QuadPart ^ (uint64_t)GetCurrentProcessId()*2654435761u;
  moteur = syn_creer(W, H, graine); tailleW = W; tailleH = H; QueryPerformanceCounter(&depart);
  if(avaitSon || mode != MODE_APERCU) lancerSon();
}
static void dessiner(HDC dc){
  if(!moteur) return;
  int gw, gh, px; const uint32_t *p = syn_pixels(moteur, &gw, &gh, &px);
  if(gw*gh > tamponN){ free(tampon); tamponN = gw*gh; tampon = malloc(sizeof(uint32_t)*tamponN); }
  /* le moteur range R, G, B ; Windows attend B, G, R */
  for(int i = 0; i < gw*gh; i++){ uint32_t v = p[i]; tampon[i] = (v & 0xFF00FF00u) | ((v & 0xFFu) << 16) | ((v >> 16) & 0xFFu); }
  memset(&bmi, 0, sizeof bmi); bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bmi.bmiHeader.biWidth = gw; bmi.bmiHeader.biHeight = -gh;
  bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32; bmi.bmiHeader.biCompression = BI_RGB;
  RECT r; GetClientRect(fenetre, &r);
  SetStretchBltMode(dc, COLORONCOLOR);   /* de vrais gros pixels, comme dans la page */
  StretchDIBits(dc, 0, 0, r.right, r.bottom, 0, 0, gw, gh, tampon, &bmi, DIB_RGB_COLORS, SRCCOPY);
}

/* ---------- plein écran ---------- */
static void basculerPleinEcran(void){
  LONG st = GetWindowLongA(fenetre, GWL_STYLE);
  if(pleinEcran){ SetWindowLongA(fenetre, GWL_STYLE, (st & ~WS_POPUP) | WS_OVERLAPPEDWINDOW);
    SetWindowPos(fenetre, NULL, rectFenetre.left, rectFenetre.top, rectFenetre.right - rectFenetre.left, rectFenetre.bottom - rectFenetre.top, SWP_NOZORDER | SWP_FRAMECHANGED);
    pleinEcran = 0; ShowCursor(TRUE); }
  else { GetWindowRect(fenetre, &rectFenetre); MONITORINFO mi = { sizeof mi }; GetMonitorInfoA(MonitorFromWindow(fenetre, MONITOR_DEFAULTTONEAREST), &mi);
    SetWindowLongA(fenetre, GWL_STYLE, (st & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
    SetWindowPos(fenetre, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED);
    pleinEcran = 1; }
}

static LRESULT CALLBACK procNoire(HWND w, UINT m, WPARAM wp, LPARAM lp){   /* les autres écrans, en noir pendant l'économiseur */
  switch(m){ case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: PostMessageA(fenetre, WM_CLOSE, 0, 0); return 0;
    case WM_SETCURSOR: SetCursor(NULL); return TRUE; }
  return DefWindowProcA(w, m, wp, lp);
}
static LRESULT CALLBACK proc(HWND w, UINT m, WPARAM wp, LPARAM lp){
  int eco = mode == MODE_ECONOMISEUR;
  switch(m){
    case WM_SIZE: if(moteur && wp != SIZE_MINIMIZED) preparer(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(w, &ps); dessiner(dc); EndPaint(w, &ps); return 0; }
    case WM_SETCURSOR: if(LOWORD(lp) == HTCLIENT && (eco || (pleinEcran && GetTickCount() - dernierGeste > 2000))){ SetCursor(NULL); return TRUE; } break;
    case WM_MOUSEMOVE: {
      POINT p = { (short)LOWORD(lp), (short)HIWORD(lp) };
      if(eco){ if(!sourisConnue){ sourisDepart = p; sourisConnue = 1; } else if(abs(p.x - sourisDepart.x) > 6 || abs(p.y - sourisDepart.y) > 6) PostMessageA(w, WM_CLOSE, 0, 0); }
      else { dernierGeste = GetTickCount(); SetCursor(LoadCursor(NULL, IDC_ARROW)); }
      break; }
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: if(eco) PostMessageA(w, WM_CLOSE, 0, 0); break;
    case WM_ACTIVATEAPP: if(eco && !wp) PostMessageA(w, WM_CLOSE, 0, 0); break;
    case WM_SYSCOMMAND: if(eco && ((wp & 0xFFF0) == SC_SCREENSAVE || (wp & 0xFFF0) == SC_CLOSE)) return 0; break;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
      if(eco){ PostMessageA(w, WM_CLOSE, 0, 0); return 0; }
      if(mode == MODE_APP){
        if(wp == VK_ESCAPE){ PostMessageA(w, WM_CLOSE, 0, 0); return 0; }
        if(wp == VK_F11 || wp == 'F'){ basculerPleinEcran(); return 0; }
        if(wp == 'M'){ if(client){ couperSon(); ecrireReglage("son", 0); } else { ecrireReglage("son", 1); lancerSon(); } return 0; }
        if(wp == 'N'){ if(moteur) syn_nouveau_tableau(moteur); return 0; }
      }
      break;
    case WM_CLOSE: couperSon(); DestroyWindow(w); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
  }
  return DefWindowProcA(w, m, wp, lp);
}

/* ---------- réglages ---------- */
static HWND caseSon, curseur;
static LRESULT CALLBACK procReglages(HWND w, UINT m, WPARAM wp, LPARAM lp){
  switch(m){
    case WM_COMMAND:
      if(LOWORD(wp) == IDOK){ ecrireReglage("son", SendMessageA(caseSon, BM_GETCHECK, 0, 0) == BST_CHECKED); ecrireReglage("volume", (DWORD)SendMessageA(curseur, TBM_GETPOS, 0, 0)); DestroyWindow(w); }
      else if(LOWORD(wp) == IDCANCEL) DestroyWindow(w);
      return 0;
    case WM_CLOSE: DestroyWindow(w); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
  }
  return DefWindowProcA(w, m, wp, lp);
}
static int reglages(HINSTANCE inst){
  INITCOMMONCONTROLSEX ic = { sizeof ic, ICC_BAR_CLASSES }; InitCommonControlsEx(&ic);
  WNDCLASSA c = { 0 }; c.lpfnWndProc = procReglages; c.hInstance = inst; c.lpszClassName = "SynchromyReglages"; c.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); c.hCursor = LoadCursor(NULL, IDC_ARROW);
  c.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(1)); RegisterClassA(&c);
  HWND w = CreateWindowExA(WS_EX_DLGMODALFRAME, c.lpszClassName, "Synchromy", WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 380, 220, NULL, NULL, inst, NULL);
  HFONT f = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  const int fr = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_FRENCH;   /* la langue de Windows : français, sinon anglais */
  HWND t = CreateWindowA("STATIC", fr ? "Dessin et musique génératifs, en hommage à Norman McLaren. Conçu par Etienne Mineur, etienne.design."
                                      : "Generative drawing and music, a tribute to Norman McLaren. Designed by Etienne Mineur, etienne.design.", WS_CHILD | WS_VISIBLE, 16, 14, 340, 34, w, NULL, inst, NULL);
  caseSon = CreateWindowA("BUTTON", fr ? "Musique" : "Music", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 16, 62, 90, 22, w, NULL, inst, NULL);
  HWND tv = CreateWindowA("STATIC", "Volume", WS_CHILD | WS_VISIBLE, 16, 98, 60, 20, w, NULL, inst, NULL);
  curseur = CreateWindowA(TRACKBAR_CLASSA, "", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, 80, 94, 270, 28, w, NULL, inst, NULL);
  HWND ok = CreateWindowA("BUTTON", "OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, 176, 140, 84, 28, w, (HMENU)IDOK, inst, NULL);
  HWND an = CreateWindowA("BUTTON", fr ? "Annuler" : "Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 268, 140, 84, 28, w, (HMENU)IDCANCEL, inst, NULL);
  HWND tous[6] = { t, caseSon, tv, curseur, ok, an }; for(int i = 0; i < 6; i++) SendMessageA(tous[i], WM_SETFONT, (WPARAM)f, TRUE);
  SendMessageA(caseSon, BM_SETCHECK, lireReglage("son", 1) ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageA(curseur, TBM_SETRANGE, TRUE, MAKELPARAM(5, 100)); SendMessageA(curseur, TBM_SETPOS, TRUE, lireReglage("volume", 60));
  ShowWindow(w, SW_SHOW);
  MSG msg; while(GetMessageA(&msg, NULL, 0, 0) > 0){ if(!IsDialogMessageA(w, &msg)){ TranslateMessage(&msg); DispatchMessageA(&msg); } }
  return 0;
}

/* les autres écrans, couverts de noir pendant l'économiseur */
static BOOL CALLBACK chaqueEcran(HMONITOR m, HDC d, LPRECT r, LPARAM lp){
  (void)d; MONITORINFO mi = { sizeof mi }; GetMonitorInfoA(m, &mi); if(mi.dwFlags & MONITORINFOF_PRIMARY) return TRUE;
  CreateWindowExA(WS_EX_TOPMOST | WS_EX_NOACTIVATE, "SynchromyNoir", "", WS_POPUP | WS_VISIBLE, r->left, r->top, r->right - r->left, r->bottom - r->top, NULL, NULL, (HINSTANCE)lp, NULL);
  return TRUE;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prec, LPSTR ligne, int montrer){
  (void)prec; (void)montrer;
  { HMODULE u = GetModuleHandleA("user32.dll"); BOOL (WINAPI *f)(HANDLE) = u ? (void *)GetProcAddress(u, "SetProcessDpiAwarenessContext") : NULL;
    if(!f || !f((HANDLE)-4)) SetProcessDPIAware(); }   /* chaque pixel de l'écran compte */
  HWND parent = NULL;
  { char nom[MAX_PATH]; GetModuleFileNameA(NULL, nom, MAX_PATH); size_t l = strlen(nom); int scr = l > 4 && _stricmp(nom + l - 4, ".scr") == 0;
    const char *a = ligne; while(*a == ' ') a++;
    if(*a == '/' || *a == '-'){ char o = (char)tolower((unsigned char)a[1]); a += 2;
      if(o == 's') mode = MODE_ECONOMISEUR;
      else if(o == 'p' || o == 'l'){ mode = MODE_APERCU; while(*a == ' ' || *a == ':') a++; parent = (HWND)(intptr_t)_strtoui64(a, NULL, 10); }
      else if(o == 'c') mode = MODE_REGLAGES;
      else if(o == 'a') return 0; }   /* mot de passe (Windows 95) : rien à faire */
    else mode = scr ? MODE_REGLAGES : MODE_APP; }
  if(mode == MODE_REGLAGES) return reglages(inst);
  CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  QueryPerformanceFrequency(&freqHorloge); QueryPerformanceCounter(&depart);
  timeBeginPeriod(1);
  WNDCLASSA c = { 0 }; c.style = CS_HREDRAW | CS_VREDRAW; c.lpfnWndProc = proc; c.hInstance = inst; c.lpszClassName = "Synchromy"; c.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  c.hCursor = LoadCursor(NULL, IDC_ARROW); c.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(1)); RegisterClassA(&c);
  if(mode == MODE_APERCU){
    if(!parent || !IsWindow(parent)) return 0;
    RECT r; GetClientRect(parent, &r);
    fenetre = CreateWindowExA(0, "Synchromy", "Synchromy", WS_CHILD | WS_VISIBLE, 0, 0, r.right, r.bottom, parent, NULL, inst, NULL);
  }else{
    MONITORINFO mi = { sizeof mi }; POINT o = { 0, 0 }; GetMonitorInfoA(MonitorFromPoint(o, MONITOR_DEFAULTTOPRIMARY), &mi); RECT m = mi.rcMonitor;
    fenetre = CreateWindowExA(mode == MODE_ECONOMISEUR ? WS_EX_TOPMOST : 0, "Synchromy", "Synchromy", WS_POPUP | WS_VISIBLE, m.left, m.top, m.right - m.left, m.bottom - m.top, NULL, NULL, inst, NULL);
    int lw = (m.right - m.left)*2/3, lh = (m.bottom - m.top)*2/3; rectFenetre = (RECT){ m.left + lw/4, m.top + lh/4, m.left + lw/4 + lw, m.top + lh/4 + lh };
    if(mode == MODE_ECONOMISEUR){ WNDCLASSA n = { 0 }; n.lpfnWndProc = procNoire; n.hInstance = inst; n.lpszClassName = "SynchromyNoir"; n.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); RegisterClassA(&n);
      EnumDisplayMonitors(NULL, NULL, chaqueEcran, (LPARAM)inst); SetForegroundWindow(fenetre); }
    dernierGeste = GetTickCount();
  }
  if(!fenetre) return 1;
  preparer();
  /* la boucle : les messages, puis une image, calée sur le rafraîchissement de l'écran */
  MSG msg; int fini = 0;
  while(!fini){
    while(PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)){ if(msg.message == WM_QUIT){ fini = 1; break; } TranslateMessage(&msg); DispatchMessageA(&msg); }
    if(fini) break;
    if(mode == MODE_APERCU && !IsWindow(parent)) break;
    if(IsIconic(fenetre)){ Sleep(50); continue; }
    if(mode == MODE_APP && pleinEcran && GetTickCount() - dernierGeste > 2000 && GetForegroundWindow() == fenetre) SetCursor(NULL);   /* la souris s'efface */
    if(moteur && syn_image(moteur, maintenant())){ HDC dc = GetDC(fenetre); dessiner(dc); ReleaseDC(fenetre, dc); }
    if(FAILED(DwmFlush())) Sleep(15);
  }
  couperSon(); syn_detruire(moteur); moteur = NULL;
  timeEndPeriod(1); CoUninitialize();
  return 0;
}
