/* Synchromy — moteur natif. Port du moteur de dessin de la version web, fonction par fonction ;
   les noms suivent ceux du code d'origine pour qu'on puisse les rapprocher. */
#include "synchromy.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#ifdef _WIN32
#define localtime_r(t, r) localtime_s((r), (t))   /* Windows : le même appel, arguments inversés */
#endif

#define PI 3.14159265358979323846

/* Bandes de lignes calculées en parallèle. Chaque bande n'écrit que dans ses propres lignes : pas de verrou.
   Sur Apple, Grand Central Dispatch ; ailleurs OpenMP s'il est là ; sinon, en série. En dessous d'une certaine
   quantité de travail, le découpage coûterait plus qu'il ne rapporte : on reste en série. */
#if defined(__APPLE__) && defined(__BLOCKS__)
#include <dispatch/dispatch.h>
#define PAR_N(n, b, ...) dispatch_apply((size_t)(n), dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^(size_t b##_){ int b = (int)b##_; __VA_ARGS__ })
#elif defined(_OPENMP)
#define PAR_N(n, b, ...) _Pragma("omp parallel for schedule(static)") for(int b = 0; b < (n); b++){ __VA_ARGS__ }
#else
#define PAR_N(n, b, ...) for(int b = 0; b < (n); b++){ __VA_ARGS__ }
#endif
#define PAR_LIGNES(debut, fin, pas_, travail, VAR, ...) do{ \
    const int d__ = (debut), p__ = (pas_), n__ = (fin) < d__ ? 0 : ((fin) - d__)/p__ + 1; \
    const int nb__ = (travail) < 40000 ? 1 : (n__ < 12 ? n__ : 12); \
    if(nb__ <= 1){ for(int k__ = 0; k__ < n__; k__++){ const int VAR = d__ + k__*p__; __VA_ARGS__ } } \
    else PAR_N(nb__, b__, { const int a__ = n__*b__/nb__, z__ = n__*(b__ + 1)/nb__; for(int k__ = a__; k__ < z__; k__++){ const int VAR = d__ + k__*p__; __VA_ARGS__ } }); \
  }while(0)
static inline double borne(double v, double a, double b){ return v < a ? a : v > b ? b : v; }
static inline double mx2(double a, double b){ return a > b ? a : b; }
static inline double mn2(double a, double b){ return a < b ? a : b; }
static inline int imx(int a, int b){ return a > b ? a : b; }
static inline int imn(int a, int b){ return a < b ? a : b; }

/* ================= couleurs ================= */
#define RGB(r,g,b) ((uint32_t)((255u<<24) | ((uint32_t)(b)<<16) | ((uint32_t)(g)<<8) | (uint32_t)(r)))
static uint32_t hexc(uint32_t n){ return RGB((n>>16)&255, (n>>8)&255, n&255); }
#define BLANC RGB(255,255,255)
#define NOIR  RGB(0,0,0)
#define SPECTRE 1u
static uint32_t VERT;

enum { PAL_ACIDE, PAL_VAGUE, PAL_CHARTE, PAL_ARC, PAL_ASCII, PAL_BIT, NPAL };
typedef struct { int n, base, arc, ascii, bit; uint32_t u[24]; } Palette;
static Palette PALS[NPAL];
static uint32_t ARC[24];
static uint32_t TEINTES[64]; static int NT = 0;
static int AB, AN, BB, BN;

/* ASCII : police 5 × 7 */
static uint8_t POLICE[23][7]; static int NPOL = 0; static int RANG_CAR[128];
static int CARS[4][8], NCARS[4], PLUIE[12], NPLUIE;
static float BAYER[64];

static void police(char c, const char *r){
  RANG_CAR[(int)c] = NPOL;
  for(int l = 0; l < 7; l++){ int v = 0; for(int b = 0; b < 5; b++) v = (v << 1) | (r[l*6 + b] == '1'); POLICE[NPOL][l] = (uint8_t)v; }
  NPOL++;
}
static void jeu(int *dst, int *n, const char *t){ *n = 0; for(; *t; t++) dst[(*n)++] = RANG_CAR[(int)*t]; }

static int pret = 0;
static void preparerTables(void){
  if(pret) return; pret = 1;
  VERT = hexc(0x39ff14);
  static const uint32_t acide[] = {0xff3ea5,0xffe21f,0x2330ff,0x52e03a,0x1f6b78};
  static const uint32_t vague[] = {0xe3a1f8,0xe8321f,0x1c1ce0,0x6bff3c,0x4aa8f0,0x5b35f5};
  static const uint32_t charte[] = {0x000000,0x39ff14};
  static const uint32_t ascii[] = {0x39ff14,0x000000,0xffffff};
  static const uint32_t bit[] = {0x000000,0x333333,0x6e6e6e,0xa8a8a8,0xffffff};
  for(int i = 0; i < 24; i++){
    double h = i/24.0*6; int x = (int)lround(255*(1 - fabs(fmod(h, 2) - 1))), s6 = (int)h, r, g, b;
    if(s6 == 0){ r = 255; g = x; b = 0; } else if(s6 == 1){ r = x; g = 255; b = 0; } else if(s6 == 2){ r = 0; g = 255; b = x; }
    else if(s6 == 3){ r = 0; g = x; b = 255; } else if(s6 == 4){ r = x; g = 0; b = 255; } else { r = 255; g = 0; b = x; }
    ARC[i] = RGB(r, g, b);
  }
  #define REMPLIR(P, T) do{ PALS[P].n = (int)(sizeof(T)/sizeof(T[0])); for(int i = 0; i < PALS[P].n; i++) PALS[P].u[i] = hexc(T[i]); }while(0)
  REMPLIR(PAL_ACIDE, acide); REMPLIR(PAL_VAGUE, vague); REMPLIR(PAL_CHARTE, charte); REMPLIR(PAL_ASCII, ascii); REMPLIR(PAL_BIT, bit);
  PALS[PAL_ARC].n = 24; memcpy(PALS[PAL_ARC].u, ARC, sizeof ARC); PALS[PAL_ARC].arc = 1;
  PALS[PAL_ASCII].ascii = 1; PALS[PAL_BIT].bit = 1;
  for(int p = 0; p < NPAL; p++){ PALS[p].base = NT; for(int i = 0; i < PALS[p].n; i++) TEINTES[NT++] = PALS[p].u[i]; }
  AB = PALS[PAL_ASCII].base; AN = PALS[PAL_ASCII].n; BB = PALS[PAL_BIT].base; BN = PALS[PAL_BIT].n;
  police('.', "00000 00000 00000 00000 00000 00110 00110"); police(':', "00000 00110 00110 00000 00110 00110 00000");
  police('-', "00000 00000 00000 01110 00000 00000 00000"); police('\'', "00100 00100 00000 00000 00000 00000 00000");
  police(',', "00000 00000 00000 00000 00110 00100 01000"); police('+', "00000 00100 00100 11111 00100 00100 00000");
  police('=', "00000 00000 11111 00000 11111 00000 00000"); police('/', "00001 00010 00010 00100 01000 01000 10000");
  police('\\', "10000 01000 01000 00100 00010 00010 00001"); police('<', "00010 00100 01000 10000 01000 00100 00010");
  police('>', "01000 00100 00010 00001 00010 00100 01000"); police('*', "00000 10101 01110 11111 01110 10101 00000");
  police('x', "00000 10001 01010 00100 01010 10001 00000"); police('o', "00000 01110 10001 10001 10001 01110 00000");
  police('0', "01110 10001 10011 10101 11001 10001 01110"); police('1', "00100 01100 00100 00100 00100 00100 01110");
  police('#', "01010 01010 11111 01010 11111 01010 01010"); police('%', "11001 11010 00010 00100 01000 01011 10011");
  police('@', "01110 10001 10111 10101 10111 10000 01110"); police('M', "10001 11011 10101 10101 10001 10001 10001");
  police('W', "10001 10001 10001 10101 10101 11011 10001"); police('8', "01110 10001 10001 01110 10001 10001 01110");
  police('$', "00100 01111 10100 01110 00101 11110 00100");
  jeu(CARS[0], &NCARS[0], ".:-',"); jeu(CARS[1], &NCARS[1], "+=/\\<>"); jeu(CARS[2], &NCARS[2], "*xo01"); jeu(CARS[3], &NCARS[3], "#%@MW8$");
  jeu(PLUIE, &NPLUIE, "01+*#:x=/$");
  for(int y = 0; y < 8; y++) for(int x = 0; x < 8; x++){
    int v = 0; for(int i = 0; i < 3; i++) v = (v << 2) | ((((x ^ y) >> i) & 1) << 1) | ((y >> i) & 1);
    BAYER[y*8 + x] = (float)((v + .5)/64);
  }
}

/* ================= motifs ================= */
enum { M_DAMIER, M_RAYURES, M_POIS, M_CHEVRONS, M_CERCLES, M_CIBLE, M_RAYONS, M_BULLE, M_TUNNEL, M_HELICE, M_ONDES,
       M_ZEBRE, M_LABYRINTHE, M_ECAILLES, M_HEXAGONES, M_POLAIRE, M_ETOILE, M_BRIQUES, M_CROIX, M_POINTES, M_PASTILLES,
       M_LOBES, M_BANDES, M_APLAT, M_SPIRALE, M_MOIRE, M_CONTOURS,
       M_MEANDRE, M_VERTIGE, M_SPHERES, M_ROSACE, M_CORAIL, M_SOLEILS, M_DECALAGE, M_FLEURS,
       M_TOURBILLON, M_ARCHES, M_PRISMES, M_SOMMETS, M_FAILLES, M_ABIME, M_CADRES, M_LOSANGES, M_FRISE,   /* les neuf motifs d'op art */
       NMOTIFS, M_LIQUIDE = 99 };
/* tirage du mode Auto : motif, poids au tour du noir et blanc, poids au tour de la couleur */
static const int TIRAGE[][3] = { {M_DAMIER,10,5},{M_RAYURES,9,6},{M_POIS,6,5},{M_CHEVRONS,6,4},{M_CERCLES,7,5},{M_CIBLE,0,9},{M_RAYONS,8,4},{M_BULLE,7,4},
  {M_TUNNEL,6,7},{M_HELICE,7,4},{M_ONDES,6,4},{M_ZEBRE,7,4},{M_LABYRINTHE,7,4},{M_ECAILLES,6,4},{M_HEXAGONES,6,4},
  {M_POLAIRE,7,4},{M_ETOILE,6,4},{M_BRIQUES,6,4},{M_CROIX,6,4},{M_POINTES,7,6},{M_PASTILLES,3,7},{M_LOBES,3,6},{M_CONTOURS,7,9},{M_BANDES,0,8},{M_APLAT,0,4},{M_SPIRALE,5,3},{M_MOIRE,5,2},
  {M_MEANDRE,7,4},{M_VERTIGE,7,3},{M_SPHERES,6,3},{M_ROSACE,6,5},{M_CORAIL,7,3},{M_SOLEILS,3,6},{M_DECALAGE,0,7},{M_FLEURS,0,7},
  {M_TOURBILLON,11,6},{M_ARCHES,10,7},{M_PRISMES,10,6},{M_SOMMETS,9,9},{M_FAILLES,11,5},{M_ABIME,11,6},{M_CADRES,10,6},{M_LOSANGES,10,6},{M_FRISE,10,7} };
#define NTIRAGE ((int)(sizeof TIRAGE / sizeof TIRAGE[0]))

enum { FOND_BLANC, FOND_NOIR, FOND_TRAME, FOND_DIAG, FOND_CERCLES, FOND_RONDS };
#define FOND_ANIME(f) ((f) >= FOND_TRAME)
enum { PIN_MOLLE, PIN_LIQUIDE };

/* ================= structures ================= */
#define MAX 4800
#define PLAFOND 2000
#define LIQ_MAX 2600
#define NBMAX 56
#define SEUIL .5

typedef struct Forme {
  int id, liquide, nb, emis, px, motif, sens, o, n, arc, ascii, bit, parChamp, aBandes, deb, cpt;
  double h, mort, x0, y0, cx, cy, graine, ke, t, u, amp, lam, kn, arc0, pasArc, ue, g, vmax, vcoeur, sx, sy;
  uint32_t c0, c1, c2, c3, fg, bg, bandes[64];
  int rebond; double chocF, chocAvant, tChoc;   /* forme qui rebondit : force des chocs de l'image, date du dernier son */
  int rk, ton;   /* coups par mesure s'il est une percussion ; rang de sa couleur, qui donne sa note */
  int m; double kr, tw, na, kv, ns, Ro, dec, ox, oy, st, nr, kl, Aa, cc, kp, ee;   /* les réglages des motifs d'op art */
} Forme;

typedef struct { double taille, mollesse, viscosite, sechage, contact, vitesse, gravite, inclinaison, variation, cerneEp;
                 int pixel, motif, palette, fond, sol, pinceau, cerne, rebond; } Reglages;

typedef struct { int cs, cols, rows, n, cap; int *debut, *rempli; int ordre[MAX]; } Grille;

typedef struct Traceur {
  Forme *forme; double x, y, ex, ey, vx, vy, t, tm, k, g0, gras; int ci, cn;
  /* automate */
  int etat, n, zig, zs, trav; double a, v, kk, tour, horloge, gg;
  Reglages r;
} Traceur;
enum { E_REPOS, E_TACHE, E_TRAIT, E_GOMME };

typedef struct { int t, coin, d; double u; } Tuile;
typedef struct { double x, y, R; int n, d; } Soleil;

struct Son;
struct Syn {
  uint64_t hasard[2];
  struct Son *son;   /* moteur sonore, s'il est branché */
  Reglages P;
  double Wv, Hv;          /* taille de la vue, en points */
  double W, H; int gw, gh;
  /* gouttes */
  float X[MAX], Y[MAX], PXa[MAX], PYa[MAX], VX[MAX], VY[MAX], HH[MAX], GM[MAX], AGE[MAX], CT[MAX];
  int SID[MAX]; uint8_t FZ[MAX], LQ[MAX], GEN[MAX], RB[MAX]; uint32_t COL[MAX]; int N;
  int gen;   /* chaque scène a sa génération de gouttes ; pendant une transition, deux scènes vivent ensemble */
  Forme **formes; int nf, capf;
  Forme **parId; int capId; int uid;
  Forme *eau;
  int sale; int tourNB;
  /* physique */
  Grille GS, GL; int celDe[MAX]; int vJ[512]; float vA[512], vUx[512], vUy[512];
  int nbDeb[MAX], nbFin[MAX]; int *nbJ;
  /* toile */
  int PX0, compteGrille; int fk, fw, fh;
  uint32_t *pix, *fondPix, *sousCerne; uint8_t *peintCerne; float *champ; uint8_t *majorite; float *votes; size_t nvotes;
  int graineFond; int aTrame; int tnc, tnr; double ttw, tth; Tuile *tuiles; Soleil soleils[4]; int nsol; double tFond; int imagesFond;
  struct { int sorte, graine, quinconce, sens, moire, anneaux, k; double pf, w, S, ang, A, L, cx, cy, cx2, cy2; } opt;   /* fonds d'art optique */
  int trie[MAX]; int rangDe[64]; uint32_t teinteDuRang[64]; uint8_t modeDuRang[64];
  double tAscii;
  /* boucle */
  double dernier, reste, coutPas, cout, periode, qualite; int compteReglage, premiere;
  int pxMin, compteMachine;   /* plus petit pixel permis, réglé selon la puissance de la machine */
  Traceur automate;
  /* veille */
  double prochainTableau;
  /* transition : une grande forme grandit et laisse voir la nouvelle scène */
  int scene;   /* style de la scène, parmi les douze de l'économiseur ; les couleurs et l'orchestre suivent */
  int trans, genAncien; double tTrans0, dureeTrans, tcx, tcy, phase[3]; uint32_t *fondAncien, *pixAncien; uint32_t bordA, bordB; float rTable[1024];
  uint16_t *masque; int imTrans; float *chemin; int nChemin, nPoints, iChemin, pasChemin; double rPinceau, bandeT;
  double latence, partHC; int derniers[4], nbTemps; double tempsT;
  /* Hardcore : l'épisode en cours, l'effet du pas, les images gardées pour sauter en arrière */
  /* l'heure, en grosses lettres pixel : elle apparaît colonne après colonne, tient, puis tombe ou se dissout */
  struct { int actif, etat, mode, motif, k, x0, y0, n, nCols; double t0, tenue, prochaine; uint32_t ca, cb; uint32_t *fond; size_t nFond;
           struct { int gx, gy, col; double dy, vy, delai, mort; uint8_t vivant; } c[400]; } horloge;
  int hcEpisode, hcArcs, hcDernierPas, hcEffet, hcBarVue; double hcZ, hcZx, hcZy; uint32_t *hcSouv[6]; int hcNSouv, hcISouv; size_t hcTaille;
};

/* ================= hasard ================= */
static inline double alea01(Syn *s){
  uint64_t a = s->hasard[0], b = s->hasard[1]; s->hasard[0] = b;
  a ^= a << 23; a ^= a >> 17; a ^= b ^ (b >> 26); s->hasard[1] = a;
  return ((a + b) >> 11) * (1.0/9007199254740992.0);
}
#define R01() alea01(s)
static inline double alea(Syn *s, double a, double b){ return a + alea01(s)*(b - a); }
static inline double h1(double n){ n = sin(n*127.1)*43758.5453; return n - floor(n); }
static inline double bruit(double x){ double i = floor(x), f = x - i, u = f*f*(3 - 2*f); return h1(i)*(1 - u) + h1(i + 1)*u; }
static inline double bruit2(double x, double y){ double i = floor(x), j = floor(y), fx = x - i, fy = y - j, ux = fx*fx*(3 - 2*fx), uy = fy*fy*(3 - 2*fy);
  double a = h1(i*12.9898 + j*78.233), b = h1((i + 1)*12.9898 + j*78.233), c = h1(i*12.9898 + (j + 1)*78.233), d = h1((i + 1)*12.9898 + (j + 1)*78.233);
  return a + (b - a)*ux + (c - a)*uy + (a - b - c + d)*ux*uy; }
static uint32_t mulberryEtat;
static double mulberry(void){
  uint32_t a = mulberryEtat += 0x6D2B79F5u; uint32_t t = (a ^ (a >> 15)) * (1u | a);
  t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t; return ((t ^ (t >> 14))) / 4294967296.0;
}
static inline int pgcd(int a, int b){ while(b){ int t = a % b; a = b; b = t; } return a; }
static inline uint32_t imul32(int64_t a, uint32_t b){ return (uint32_t)a * b; }
static inline uint32_t hacheCase(int ix, int iy, int g){
  return imul32((int64_t)(uint32_t)((int64_t)ix*73856093) ^ (uint32_t)((int64_t)iy*19349663) ^ (uint32_t)((int64_t)g*83492791), 0x9E3779B1u) >> 8;
}
static inline int fl(double v){ return (int)floor(v); }
static inline double atan2Vite(double y, double x);

#include "son.inc"
/* les douze styles de l'économiseur, dans l'ordre où les tableaux les enchaînent */
static const int ORDRE_VEILLE[12] = { ST_MINIMAL, ST_DNB, ST_JUNGLE, ST_CHIP, ST_HOUSE, ST_DUB, ST_HIPHOP, ST_AFRO, ST_GAMELAN, ST_BOSSA, ST_AMBIENT, ST_CONCRET };

/* ================= formes ================= */
static void dimensionner(Syn *s);
static void construireFond(Syn *s);

static int indexDans(const Palette *p, uint32_t c){ for(int i = 0; i < p->n; i++) if(p->u[i] == c) return i; return -1; }

static void creerMotif(Syn *s, Forme *f){
  const Palette *pal = &PALS[s->P.palette];
  uint32_t vives[24]; int nv = 0;
  for(int i = 0; i < pal->n; i++) if(pal->u[i] != NOIR && pal->u[i] != BLANC) vives[nv++] = pal->u[i];
  #define COUL() (nv ? vives[(int)(R01()*nv)] : pal->u[(int)(R01()*pal->n)])
  int nb = s->tourNB; s->tourNB = !s->tourNB;
  int arc = pal->arc && !nb;
  f->ascii = pal->ascii; if(f->ascii){ f->fg = nb ? NOIR : VERT; f->bg = nb ? BLANC : NOIR; }
  f->bit = pal->bit; f->arc = 0;
  #define SPECTRE_F() do{ f->arc = 1; f->arc0 = R01()*24; f->pasArc = mx2(3, f->h*alea(s,.18,.4)); }while(0)
  int m = s->P.motif;
  if(m < 0){
    int col = nb ? 1 : 2; double tot = 0; for(int k = 0; k < NTIRAGE; k++) tot += TIRAGE[k][col];
    double r = R01()*tot;
    for(int k = 0; k < NTIRAGE; k++){ if(!TIRAGE[k][col]) continue; m = TIRAGE[k][0]; if((r -= TIRAGE[k][col]) < 0) break; }
  }
  f->motif = m; f->t = 0; f->sens = R01() < .5 ? -1 : 1; f->aBandes = 0;
  { static const int RK[NMOTIFS][2] = { [M_DAMIER]={2,4}, [M_RAYURES]={5,11}, [M_POIS]={3,6}, [M_CHEVRONS]={3,5}, [M_CERCLES]={2,4}, [M_CIBLE]={3,5}, [M_RAYONS]={4,8},
      [M_BULLE]={3,7}, [M_TUNNEL]={2,4}, [M_HELICE]={4,9}, [M_ONDES]={3,6}, [M_ZEBRE]={2,4}, [M_LABYRINTHE]={5,10}, [M_ECAILLES]={2,5}, [M_HEXAGONES]={2,4},
      [M_POLAIRE]={1,2}, [M_ETOILE]={3,6}, [M_BRIQUES]={3,6}, [M_CROIX]={2,4}, [M_POINTES]={3,6}, [M_PASTILLES]={3,6}, [M_LOBES]={1,2},
      [M_MEANDRE]={3,6}, [M_VERTIGE]={2,5}, [M_ROSACE]={2,4}, [M_CORAIL]={4,8}, [M_SOLEILS]={1,3}, [M_FLEURS]={2,5},
      [M_TOURBILLON]={2,4}, [M_ARCHES]={2,5}, [M_PRISMES]={3,6}, [M_SOMMETS]={3,6}, [M_FAILLES]={4,8}, [M_ABIME]={1,2}, [M_CADRES]={3,6}, [M_LOSANGES]={4,8}, [M_FRISE]={3,6} };
    f->rk = RK[m][1] ? RK[m][0] + (int)(R01()*(RK[m][1] - RK[m][0] + 1)) : 0; }
  #define DUO() do{ \
    if(nb){ f->c0 = NOIR; f->c1 = BLANC; } \
    else if(arc){ f->c0 = SPECTRE; f->c1 = R01() < .7 ? BLANC : NOIR; SPECTRE_F(); } \
    else { f->c0 = COUL(); f->c1 = R01() < .6 ? BLANC : pal->u[(indexDans(pal, f->c0) + 1 + (int)(R01()*(pal->n - 1))) % pal->n]; } }while(0)
  #define BANDES() do{ \
    f->aBandes = 1; int i = 0; \
    if(arc){ int h0 = (int)(R01()*24); while(i < 64){ uint32_t c = ARC[h0++ % 24]; int n = 1 + (int)(R01()*3); while(n-- > 0 && i < 64) f->bandes[i++] = c; } } \
    else { uint32_t avant = 0; while(i < 64){ uint32_t c = R01() < .05 ? BLANC : COUL(); \
      if(c == avant){ int ix = indexDans(pal, c); c = pal->u[(ix + 1) % pal->n]; } avant = c; \
      int n = 1 + (int)floor(R01()*R01()*7); while(n-- > 0 && i < 64) f->bandes[i++] = c; } } }while(0)
  #define TRIO() do{ \
    if(nb){ f->c0 = NOIR; f->c1 = BLANC; f->c2 = BLANC; f->c3 = NOIR; } \
    else { const uint32_t *src = arc ? ARC : (nv ? vives : pal->u); int ns = arc ? 24 : (nv ? nv : pal->n); \
      int i0 = (int)(R01()*ns), ec = arc ? 7 : 1; f->c0 = src[i0]; f->c2 = src[(i0 + ec) % ns]; f->c3 = src[(i0 + 2*ec) % ns]; \
      f->c1 = R01() < .5 ? BLANC : NOIR; if(f->c3 == f->c0) f->c3 = f->c1 == BLANC ? NOIR : BLANC; } }while(0)
  f->parChamp = 0;
  switch(m){
    case M_CONTOURS: f->kn = alea(s,7,12); f->parChamp = 1;
      if(nb){ f->aBandes = 1; int i = 0; uint32_t c = NOIR; while(i < 64){ int n = 1 + (int)(R01()*2); while(n-- > 0 && i < 64) f->bandes[i++] = c; c = c == NOIR ? BLANC : NOIR; } }
      else BANDES(); break;
    case M_POINTES: f->n = 16 + (int)(R01()*24); f->parChamp = 1; DUO(); f->c2 = nb ? NOIR : COUL();
      if(!nb && f->c2 == f->c0) f->c2 = pal->u[(indexDans(pal, f->c0) + 1) % pal->n]; break;
    case M_PASTILLES: f->u = f->h*alea(s,.5,.8); TRIO(); break;
    case M_LOBES: f->u = f->h*alea(s,.6,.95); TRIO(); break;
    case M_BANDES: f->u = mx2(3, f->h*alea(s,.09,.2)); BANDES(); break;
    case M_RAYURES: { static const int O[6] = {0,0,0,1,2,3}; f->o = O[(int)(R01()*6)]; f->u = f->h*alea(s,.22,.46); DUO(); break; }
    case M_RAYONS: f->n = 2*(8 + (int)(R01()*11)); DUO(); f->c2 = nb ? NOIR : COUL(); break;
    case M_CERCLES: f->u = f->h*alea(s,.2,.4); DUO(); break;
    case M_DAMIER: f->u = f->h*alea(s,.3,.6); DUO(); break;
    case M_SPIRALE: f->n = 2*(1 + (int)(R01()*4)); f->u = f->h*alea(s,.2,.42); DUO(); break;
    case M_HELICE: f->n = 2*(5 + (int)(R01()*8)); DUO(); break;
    case M_MOIRE: f->u = f->h*alea(s,.13,.22); DUO(); break;
    case M_TUNNEL: f->o = R01() < .4; f->u = f->h*alea(s,.16,.34); if(!nb && R01() < .6) BANDES(); else DUO(); break;
    case M_CIBLE: f->u = mx2(3, f->h*alea(s,.1,.22)); BANDES(); break;
    case M_ONDES: f->o = R01() < .3; f->u = f->h*alea(s,.2,.36); f->amp = f->u*alea(s,.8,1.9); f->lam = f->h*alea(s,.45,1.1); DUO(); break;
    case M_CHEVRONS: f->u = f->h*alea(s,.2,.36); DUO(); break;
    case M_BULLE: f->u = f->h*alea(s,.3,.5); DUO(); break;
    case M_POIS: f->u = f->h*alea(s,.22,.36); DUO(); break;
    case M_ZEBRE: f->u = f->h*alea(s,.22,.36); f->amp = f->u*alea(s,1.4,2.6); f->lam = f->h*alea(s,.5,1); DUO(); break;
    case M_LABYRINTHE: f->u = f->h*alea(s,.2,.3); DUO(); break;
    case M_ECAILLES: f->u = f->h*alea(s,.22,.34); DUO(); break;
    case M_HEXAGONES: f->u = f->h*alea(s,.24,.36); DUO(); break;
    case M_POLAIRE: f->n = 2*(4 + (int)(R01()*6)); DUO(); break;
    case M_ETOILE: f->n = 5 + (int)(R01()*3); f->u = f->h*alea(s,.5,.7); DUO(); break;
    case M_BRIQUES: f->u = f->h*alea(s,.3,.5); DUO(); break;
    case M_CROIX: f->u = f->h*alea(s,.22,.32); DUO(); break;
    case M_MEANDRE: f->n = 3 + (int)(R01()*4); f->u = f->h*alea(s,.16,.26); DUO(); break;
    case M_VERTIGE: f->n = 2*(5 + (int)(R01()*5)); f->amp = alea(s,2.6,4); DUO(); break;
    case M_SPHERES: f->n = 2*(9 + (int)(R01()*8)); f->u = f->h*alea(s,.18,.3); DUO(); break;
    case M_ROSACE: f->n = 8 + (int)(R01()*8); f->u = f->h*alea(s,.12,.2); DUO(); break;
    case M_CORAIL: f->u = f->h*alea(s,.2,.32); DUO(); break;
    case M_SOLEILS: f->n = 2*(7 + (int)(R01()*6)); f->u = f->h*alea(s,.3,.45); DUO(); f->c2 = nb ? NOIR : COUL(); break;
    case M_DECALAGE: f->u = mx2(3, f->h*alea(s,.1,.18)); f->lam = alea(s,.15,.35); BANDES(); break;
    case M_FLEURS: f->u = f->h*alea(s,.4,.6); TRIO(); break;
    /* les neuf motifs d'op art */
    case M_TOURBILLON: { static const int MM[5] = {8, 10, 12, 12, 16}; f->n = 2*(18 + (int)(R01()*12)); f->m = MM[(int)(R01()*5)]; f->kr = alea(s, 4, 5.5); f->tw = alea(s, .45, .9)*(R01() < .5 ? -1 : 1); f->u = f->h*alea(s, .2, .32); DUO(); break; }
    case M_ARCHES: f->u = mx2(2, f->h*alea(s, .08, .13)); f->na = 2 + (int)(R01()*3); f->kv = alea(s, .55, .8); DUO(); break;
    case M_PRISMES: f->u = f->h*alea(s, .45, .7); DUO(); break;
    case M_SOMMETS: f->u = mx2(2, f->h*alea(s, .08, .13)); f->ns = 4 + (int)(R01()*3); DUO(); break;
    case M_FAILLES: f->u = mx2(2, f->h*alea(s, .07, .12)); f->Ro = f->h*alea(s, .6, 1.2); f->dec = alea(s, .3, .7); f->ox = f->h*alea(s, -3, -1.5); f->oy = f->h*alea(s, 1.5, 3); DUO(); break;
    case M_ABIME: { static const int MM[4] = {4, 6, 6, 8}; f->m = MM[(int)(R01()*4)]; f->st = alea(s, .3, .5); f->kr = alea(s, 4.5, 6.5); f->u = f->h*alea(s, .12, .24); DUO(); break; }
    case M_CADRES: f->u = mx2(2, f->h*alea(s, .07, .11)); f->nr = 4 + (int)(R01()*3); DUO(); break;
    case M_LOSANGES: f->u = mx2(2, f->h*alea(s, .13, .19)); f->kl = 3 + (int)(R01()*3); DUO(); break;
    case M_FRISE: f->u = mx2(2, f->h*alea(s, .08, .12)); f->Aa = 4 + (int)(R01()*3); f->cc = alea(s, .25, .5); f->kp = (1 + 2*f->cc)*2*alea(s, .9, 1.1); f->ee = 1 + (int)(R01()*2); DUO(); break;
    default: if(arc){ f->c0 = SPECTRE; SPECTRE_F(); } else f->c0 = nb ? (s->P.fond == FOND_NOIR ? BLANC : NOIR) : COUL();
  }
  /* la couleur de la forme donne son ton : rang de sa teinte dans la palette, ou place dans le spectre */
  f->ton = 0;
  if(f->arc) f->ton = (int)floor(f->arc0/24*7) % 7;
  else if(!nb){ uint32_t c = f->aBandes ? f->bandes[0] : f->c0; int k = -1;
    if(pal->arc){ for(int i = 0; i < 24; i++) if(ARC[i] == c){ k = (int)floor(i/24.0*7); break; } } else k = indexDans(pal, c);
    f->ton = k > 0 ? k : 0; }
}

static void ranger(Syn *s, Forme *f){
  if(f->id >= s->capId){ int n = s->capId ? s->capId : 1024; while(n <= f->id) n *= 2;
    s->parId = realloc(s->parId, sizeof(Forme*)*n); memset(s->parId + s->capId, 0, sizeof(Forme*)*(n - s->capId)); s->capId = n; }
  s->parId[f->id] = f;
}
static inline Forme *parId(Syn *s, int id){ return id < s->capId ? s->parId[id] : NULL; }

static void adopter(Syn *s, int px){ if(px % s->PX0){ s->PX0 = pgcd(s->PX0, px); dimensionner(s); } }

static Forme *nouvelleForme(Syn *s, double x, double y, double h){
  Forme *f = calloc(1, sizeof(Forme));
  f->id = ++s->uid; f->h = h > 0 ? h : s->P.taille; f->x0 = f->cx = x; f->y0 = f->cy = y;
  f->graine = R01()*999; f->px = s->P.pixel; f->rebond = s->P.rebond; f->ke = .6 + .8*borne(y/s->H, 0, 1);
  creerMotif(s, f);
  adopter(s, f->px);
  if(s->nf + 1 > s->capf){ s->capf = s->capf ? s->capf*2 : 64; s->formes = realloc(s->formes, sizeof(Forme*)*s->capf); }
  s->formes[s->nf] = s->formes[s->nf - 1]; s->formes[s->nf - 1] = f; s->nf++;   /* sous le liquide, toujours dernier */
  ranger(s, f);
  return f;
}

static void goutte(Syn *s, Forme *f, double x, double y, double vx, double vy, uint32_t teinte){
  if(s->N >= MAX) return;
  int i = s->N++; s->sale = 1;
  s->X[i] = s->PXa[i] = (float)x; s->Y[i] = s->PYa[i] = (float)y; s->VX[i] = (float)vx; s->VY[i] = (float)vy;
  s->HH[i] = (float)f->h; s->SID[i] = f->id; s->AGE[i] = 0; s->CT[i] = 0; s->FZ[i] = 0; s->GEN[i] = (uint8_t)s->gen; s->RB[i] = (uint8_t)f->rebond;
  if(f->liquide){ s->LQ[i] = 1; s->COL[i] = teinte; s->GM[i] = 1; }
  else { s->LQ[i] = 0; s->COL[i] = 0; s->GM[i] = (float)(.05 + 2.7*pow(bruit(x/(1.9*f->h) + f->graine), 2.8)); }
  f->nb++; f->emis++;
}

static void retirer(Syn *s, int i){
  Forme *f = parId(s, s->SID[i]); if(f) f->nb--;
  s->sale = 1;
  int d = --s->N;
  if(i != d){
    s->X[i]=s->X[d]; s->Y[i]=s->Y[d]; s->PXa[i]=s->PXa[d]; s->PYa[i]=s->PYa[d]; s->VX[i]=s->VX[d]; s->VY[i]=s->VY[d];
    s->HH[i]=s->HH[d]; s->GM[i]=s->GM[d]; s->AGE[i]=s->AGE[d]; s->CT[i]=s->CT[d]; s->SID[i]=s->SID[d]; s->FZ[i]=s->FZ[d]; s->GEN[i]=s->GEN[d]; s->RB[i]=s->RB[d]; s->LQ[i]=s->LQ[d]; s->COL[i]=s->COL[d];
  }
}

/* ================= traceur ================= */
static double tailleAuHasard(Syn *s){
  double v = s->P.variation;
  return v > 0 ? borne(s->P.taille*pow(1.8, v*alea(s,-1,1)), 10, 100) : s->P.taille;
}
static double respirer(Syn *s, Traceur *tr){
  double v = s->P.variation; if(!(v > 0)) return 1;
  tr->k += .09;
  return pow(2.2, v*borne((2*bruit(tr->k + tr->g0) - 1)*1.7, -1, 1));
}
static void semer(Syn *s, Traceur *tr, double x, double y, double gras, int dir, double ux, double uy){
  tr->gras = gras;
  Forme *f = tr->forme; int n = imx(1, (int)lround(1 + 2.3*gras)); double larg = f->h*.42*gras;
  double nx = !dir ? 1 : -uy, ny = !dir ? 0 : ux;
  for(int k = 0; k < n; k++){
    double o = n == 1 ? 0 : ((double)k/(n-1) - .5)*2*larg;
    goutte(s, f, x + nx*o + alea(s,-2,2), y + ny*o + alea(s,-2,2), tr->vx*.14, tr->vy*.14, 0);
  }
}
static void tracer(Syn *s, Traceur *tr, double x, double y, double maintenant, int forcer){
  int repos = maintenant - tr->t > 320;
  if(forcer || !tr->forme || tr->forme->mort > 0 || repos || tr->forme->emis > 230){
    tr->forme = nouvelleForme(s, x, y, tailleAuHasard(s));
    tr->ex = x; tr->ey = y; tr->vx = tr->vy = 0;
    if(forcer) semer(s, tr, x, y, 1.2, 0, 0, 0);
  }else{
    double dt = mx2(4, maintenant - tr->t);
    tr->vx = (x - tr->x)/dt*16.6; tr->vy = (y - tr->y)/dt*16.6;
  }
  Forme *f = tr->forme; double pas = f->h*.45;
  double dx = x - tr->ex, dy = y - tr->ey, d = hypot(dx, dy); int garde = 0;
  while(d >= pas && garde++ < 60){
    tr->ex += dx/d*pas; tr->ey += dy/d*pas;
    semer(s, tr, tr->ex, tr->ey, mn2(2.6, borne(1.5 - hypot(tr->vx, tr->vy)/38, .45, 1.5)*respirer(s, tr)), 1, dx/d, dy/d);
    dx = x - tr->ex; dy = y - tr->ey; d = hypot(dx, dy);
  }
  if(hypot(x - tr->x, y - tr->y) > 1.5) tr->tm = maintenant;
  if(!forcer && maintenant - tr->t < 320) son_chemin(s, hypot(x - tr->x, y - tr->y));   /* le trait fait avancer le voyage */
  tr->x = x; tr->y = y; tr->t = maintenant;
  son_suivre(s, tr, PIN_MOLLE, tr->gras ? tr->gras : 1);
}
static void gonfler(Syn *s, Traceur *tr){
  if(!tr->forme || tr->forme->mort > 0) return;
  if(tr->forme->emis > 260) tr->forme = nouvelleForme(s, tr->x, tr->y, tailleAuHasard(s));
  Forme *f = tr->forme; double r = f->h*.3;
  for(int k = 0; k < 2; k++) goutte(s, f, tr->x + alea(s,-r,r), tr->y + alea(s,-r,r), 0, 0, 0);
  tr->vx *= .7; tr->vy *= .7; son_chemin(s, 1.5); son_suivre(s, tr, PIN_MOLLE, 1.4);
}
/* liquide */
static int debit(Syn *s){ return (int)lround(borne(lround(s->P.taille/14), 1, 5)*2); }
static uint32_t teinteDe(Syn *s, Traceur *tr){
  const Palette *pal = &PALS[s->P.palette];
  if(++tr->cn > (pal->arc ? 12 : 420)){ tr->cn = 0; tr->ci++; }
  return (uint32_t)(pal->base + tr->ci % pal->n);
}
static void verser(Syn *s, Traceur *tr, double x, double y, int n){
  double m = respirer(s, tr), r = mx2(3, s->P.taille*.3*m), vx = borne(tr->vx*.3, -8, 8), vy = borne(tr->vy*.3, -8, 8);
  n = imx(1, (int)lround(n*m));
  if(s->eau->px != s->P.pixel){ s->eau->px = s->P.pixel; adopter(s, s->eau->px); }
  son_verse(s, tr, n);
  for(int k = 0; k < n; k++) goutte(s, s->eau, x + alea(s,-r,r), y + alea(s,-r,r), vx + alea(s,-.15,.15), vy + alea(s,-.1,.25), teinteDe(s, tr));
}
static void tracerLiquide(Syn *s, Traceur *tr, double x, double y, double maintenant, int forcer){
  if(forcer || maintenant - tr->t > 320){
    tr->ci = (int)(R01()*12); tr->cn = 0; tr->ex = x; tr->ey = y; tr->vx = tr->vy = 0;
    if(forcer) verser(s, tr, x, y, debit(s) + 1);
  }else{
    double dt = mx2(4, maintenant - tr->t);
    tr->vx = (x - tr->x)/dt*16.6; tr->vy = (y - tr->y)/dt*16.6;
  }
  double pasL = s->eau->h*.5;
  double dx = x - tr->ex, dy = y - tr->ey, d = hypot(dx, dy); int garde = 0;
  while(d >= pasL && garde++ < 40){
    tr->ex += dx/d*pasL; tr->ey += dy/d*pasL;
    verser(s, tr, tr->ex, tr->ey, debit(s));
    dx = x - tr->ex; dy = y - tr->ey; d = hypot(dx, dy);
  }
  if(hypot(x - tr->x, y - tr->y) > 1.5) tr->tm = maintenant;
  if(!forcer && maintenant - tr->t < 320) son_chemin(s, hypot(x - tr->x, y - tr->y));
  tr->x = x; tr->y = y; tr->t = maintenant;
  son_suivre(s, tr, PIN_LIQUIDE, 1);
}
static void gommer(Syn *s, double x0, double y0, double x1, double y1){
  double dx = x1 - x0, dy = y1 - y0, l2 = dx*dx + dy*dy, R = mx2(10, s->P.taille*.9), R2 = R*R;
  for(int i = s->N-1; i >= 0; i--){
    double t = l2 ? borne(((s->X[i] - x0)*dx + (s->Y[i] - y0)*dy)/l2, 0, 1) : 0;
    double ex = s->X[i] - x0 - t*dx, ey = s->Y[i] - y0 - t*dy;
    if(ex*ex + ey*ey < R2) retirer(s, i);
  }
}
#define tracerP(s, tr, x, y, m, fo) ((s)->P.pinceau == PIN_LIQUIDE ? tracerLiquide(s, tr, x, y, m, fo) : tracer(s, tr, x, y, m, fo))
#define gonflerP(s, tr) ((s)->P.pinceau == PIN_LIQUIDE ? verser(s, tr, (tr)->x, (tr)->y, debit(s)) : gonfler(s, tr))

static void effacer(Syn *s){
  s->sale = 1;
  for(int k = 0; k < s->nf; k++) s->formes[k]->mort = .22;
  s->automate.forme = NULL; s->automate.etat = E_REPOS; s->automate.n = 30;
}

/* ================= automate ================= */
static const int PALETTES_AUTO[12] = {PAL_ACIDE,PAL_ACIDE,PAL_ACIDE,PAL_VAGUE,PAL_VAGUE,PAL_VAGUE,PAL_CHARTE,PAL_CHARTE,PAL_ARC,PAL_ARC,PAL_ASCII,PAL_BIT};
static void tirerAuto(Syn *s, Traceur *A){
  double u = R01();
  static const int MULT[8] = {1,1,1,1,2,2,3,4};
  A->r = s->P;
  A->r.pinceau = R01() < .10 ? PIN_LIQUIDE : PIN_MOLLE;   /* le liquide reste rare : un geste sur dix */
  A->r.rebond = A->r.pinceau == PIN_MOLLE && R01() < .2;   /* une forme sur cinq rebondit, et sonne à chaque choc */
  A->r.taille = lround(12 + 80*u*u*(.4 + .6*R01()) + 8*R01());   /* de 12 à 100 */
  A->r.variation = R01() < .45 ? 0 : alea(s, .25, 1);
  /* les couleurs suivent la musique de la scène */
  static const int THEMES[12][3] = { {PAL_CHARTE, PAL_CHARTE, PAL_BIT},    /* minimal : noir, vert, 1 bit */
                                     {PAL_ACIDE, PAL_ACIDE, PAL_ARC},      /* drum and bass : acide et arc-en-ciel */
                                     {PAL_VAGUE, PAL_VAGUE, PAL_ARC},      /* jungle : vague et arc-en-ciel */
                                     {PAL_ASCII, PAL_ASCII, PAL_BIT},      /* 8 bits : ASCII et 1 bit */
                                     {PAL_ACIDE, PAL_ARC, PAL_CHARTE},     /* house */
                                     {PAL_VAGUE, PAL_VAGUE, PAL_BIT},      /* dub */
                                     {PAL_CHARTE, PAL_ASCII, PAL_ACIDE},   /* hip-hop */
                                     {PAL_ARC, PAL_ARC, PAL_ACIDE},        /* afro */
                                     {PAL_ARC, PAL_VAGUE, PAL_CHARTE},     /* gamelan */
                                     {PAL_VAGUE, PAL_ARC, PAL_CHARTE},     /* bossa nova */
                                     {PAL_VAGUE, PAL_BIT, PAL_CHARTE},     /* ambient */
                                     {PAL_BIT, PAL_ASCII, PAL_CHARTE} };   /* concret */
  A->r.palette = THEMES[s->scene % 12][(int)(R01()*3)]; (void)PALETTES_AUTO;
  A->r.motif = -1;
  /* économiseur : chaque geste tire sa taille de pixel, de 2 à 20 (paire, pour que la grille commune reste à 2 au plus fin) */
  /* taille des pixels : six fois sur dix entre 1 et 6, sinon entre 8 et 24 ; le liquide, entre 1 et 4.
     Sur une machine qui peine, on monte au plus petit pixel permis (et à ses multiples). */
  { int px = A->r.pinceau == PIN_LIQUIDE ? 1 + (int)(R01()*4) : R01() < .6 ? 1 + (int)(R01()*6) : 8 + (int)(R01()*17);
    int m = s->pxMin; if(m > 1) px = imn(24, ((px + m - 1)/m)*m);
    A->r.pixel = px; } (void)MULT;
  double g = R01();
  A->etat = g < .3 ? E_TACHE : E_TRAIT;
  A->kk = .55; A->tour = 0; A->zig = 0; A->v = alea(s, 3, 11);
  if(g >= .3){
    if(g < .58) A->kk = alea(s, .4, .9);
    else if(g < .72) A->kk = .06;
    else if(g < .88){ A->kk = .12; A->tour = alea(s, .07, .2)*(R01() < .5 ? -1 : 1); }
    else { A->kk = .1; A->zig = (int)alea(s, 6, 14); A->zs = 1; A->v = alea(s, 6, 12); }
  }
}
/* l'automate trace avec ses réglages le temps d'un appel */
#define EN_AUTO(s, A, code) do{ Reglages garde_ = (s)->P; \
    (s)->P.pinceau = (A)->r.pinceau; (s)->P.taille = (A)->r.taille; (s)->P.variation = (A)->r.variation; \
    (s)->P.palette = (A)->r.palette; (s)->P.motif = (A)->r.motif; (s)->P.pixel = (A)->r.pixel; (s)->P.rebond = (A)->r.rebond; \
    code; \
    garde_.sol = (s)->P.sol; (s)->P = garde_; }while(0)

typedef struct { double c, x, y; } Remplissage;
static Remplissage remplissage(Syn *s){
  Remplissage r = { 0, s->W/2, s->H/2 };
  if(!s->pix || !s->fondPix) return r;
  int n = 0, t = 0, choisi = -1;
  for(int i = 3, fin = s->gw*s->gh; i < fin; i += 7){ t++; if(s->pix[i] != s->fondPix[i]){ n++; if(R01()*n < 1) choisi = i; } }
  double px = s->W/s->gw;
  r.c = t ? (double)n/t : 0;
  if(choisi >= 0){ r.x = (choisi % s->gw + .5)*px; r.y = (choisi/s->gw + .5)*px; }
  return r;
}

static void animerAutomate(Syn *s){
  Traceur *A = &s->automate;
  double W = s->W, H = s->H;
  A->horloge += 16.6; A->n--;
  if(A->etat == E_REPOS){
    if(A->n > 0) return;
    Remplissage plein = remplissage(s);
    if(plein.c > .6 && R01() < .35){ effacer(s); A->n = (int)alea(s, 45, 100); return; }
    A->x = alea(s,.1,.9)*W; A->y = alea(s,.04,.6)*H; A->a = alea(s, 0, PI*2);
    tirerAuto(s, A);
    if(R01() < borne((plein.c - .2)*1.7, 0, .8)){
      A->etat = E_GOMME; A->x = plein.x; A->y = plein.y; A->r.taille = (int)alea(s, 30, 100);   /* la gomme aussi va jusqu à 100 */ A->kk = alea(s,.3,.9); A->v = alea(s, 5, 11);
      A->n = (int)alea(s, 45, 130); A->forme = NULL;
      return;
    }
    A->trav = 0;
    if(A->etat == E_TACHE) A->n = (int)alea(s, 22, 70);
    else {
      A->n = (int)alea(s, 50, 150);                          /* des traits plus longs qu'avant */
      if(R01() < .25) A->n *= 4;                              /* et parfois quatre fois plus */
      if(A->r.pinceau == PIN_MOLLE && R01() < .14){            /* parfois une traversée : un trait épais d'un bord à l'autre */
        A->trav = 1; A->r.taille = (int)alea(s, 60, 100); A->r.variation = alea(s, 0, .4);
        A->kk = .04; A->tour = 0; A->zig = 0; A->v = alea(s, 9, 14);
        int gauche = R01() < .5, diag = R01() < .4;
        A->x = gauche ? 16 : W - 16; A->y = diag ? alea(s, .08, .3)*H : alea(s, .2, .75)*H;
        A->a = (gauche ? 0 : PI) + (diag ? (gauche ? 1 : -1)*alea(s, .25, .45) : alea(s, -.08, .08));
        A->n = (int)(W/(A->v*.9)) + 40;
      }
    }
    A->forme = NULL; A->t = -1e9;
    EN_AUTO(s, A, tracerP(s, A, A->x, A->y, A->horloge, 1));
  }else if(A->etat == E_GOMME){
    A->a += (bruit(A->horloge*.004 + A->gg) - .5)*A->kk;
    double nx = A->x + cos(A->a)*A->v, ny = A->y + sin(A->a)*A->v;
    if(nx < 10 || nx > W-10) A->a = PI - A->a;
    if(ny < 10 || ny > H-10) A->a = -A->a;
    nx = borne(nx, 5, W-5); ny = borne(ny, 5, H-5);
    EN_AUTO(s, A, gommer(s, A->x, A->y, nx, ny));
    A->x = nx; A->y = ny; s->sale = 1;
    if(A->n <= 0){ A->etat = E_REPOS; A->n = (int)alea(s, 25, 110); }
  }else if(A->etat == E_TACHE){
    EN_AUTO(s, A, gonflerP(s, A));
    if(A->n <= 0){ A->etat = E_REPOS; A->n = (int)alea(s, 25, 110); }
  }else{
    A->a += (bruit(A->horloge*.004 + A->gg) - .5)*A->kk + A->tour;
    if(A->zig && A->n % A->zig == 0){ A->zs = -A->zs; A->a += 1.9*A->zs; }
    double nx = A->x + cos(A->a)*A->v, ny = A->y + sin(A->a)*A->v*.7;
    if(A->trav){ if(nx < 6 || nx > W - 6 || ny < 4 || ny > H - 4) A->n = 0; }   /* la traversée s'arrête au bord opposé */
    else {
      if(nx < 20 || nx > W-20) A->a = PI - A->a;
      if(ny < 10 || ny > H*.8) A->a = -A->a;
    }
    nx = borne(nx, 4, W-4); ny = borne(ny, 4, A->trav ? H - 4 : H*.85);
    EN_AUTO(s, A, tracerP(s, A, nx, ny, A->horloge, 0));
    if(A->n <= 0){ A->etat = E_REPOS; A->n = (int)alea(s, 25, 110); }
  }
}

/* ================= physique ================= */
static int preparer(Syn *s, Grille *g, int cs){
  g->cs = cs; g->cols = (int)ceil(s->W/cs) + 3; g->rows = (int)ceil((s->H + 3*cs)/cs) + 3; g->n = 0;
  int nc = g->cols*g->rows;
  if(g->cap < nc + 1){ g->cap = nc + 1; g->debut = realloc(g->debut, sizeof(int)*(nc + 1)); g->rempli = realloc(g->rempli, sizeof(int)*(nc + 1)); }
  memset(g->debut, 0, sizeof(int)*(nc + 1)); memset(g->rempli, 0, sizeof(int)*nc);
  return nc;
}
static void hacher(Syn *s){
  double hs = 10, hl = 8;
  for(int i = 0; i < s->N; i++){ if(s->LQ[i]){ if(s->HH[i] > hl) hl = s->HH[i]; } else if(s->HH[i] > hs) hs = s->HH[i]; }
  /* les tailles sont entières dans la version web la plupart du temps ; la grille prend une case de la taille du plus grand rayon */
  int ns = preparer(s, &s->GS, (int)ceil(hs)), nl = preparer(s, &s->GL, (int)ceil(hl));
  for(int i = 0; i < s->N; i++){
    Grille *g = s->LQ[i] ? &s->GL : &s->GS; int cs = g->cs;
    int cx = (int)borne(fl(s->X[i]/cs) + 1, 0, g->cols-1), cy = (int)borne(fl(s->Y[i]/cs) + 1, 0, g->rows-1);
    int c = cy*g->cols + cx; s->celDe[i] = c; g->debut[c+1]++; g->n++;
  }
  for(int c = 0; c < ns; c++) s->GS.debut[c+1] += s->GS.debut[c];
  for(int c = 0; c < nl; c++) s->GL.debut[c+1] += s->GL.debut[c];
  for(int i = 0; i < s->N; i++){ Grille *g = s->LQ[i] ? &s->GL : &s->GS; int c = s->celDe[i]; g->ordre[g->debut[c] + g->rempli[c]++] = i; }
}

#define RHO0 2.2
#define KP .012
#define KN .06
static const struct { double GL, KL, KNL, HL, RL, SL, ET, RHOL, COH, VL, FL0, FL1; } PH = { .2, .035, .16, .35, 1.35, .9, 2.5, 3, 1.25, 1.5, .02, .1 };

static int tenue(Syn *s, Forme *f){ return f == s->automate.forme; }

static void pas(Syn *s){
  const Reglages *P = &s->P; double W = s->W, H = s->H; int N;
  float *X = s->X, *Y = s->Y, *VX = s->VX, *VY = s->VY, *HH = s->HH, *GM = s->GM, *CT = s->CT, *PXa = s->PXa, *PYa = s->PYa;
  /* 1. disparitions */
  double trop = s->eau->nb - LIQ_MAX*s->qualite, evap = trop > 0 ? mn2(.012, trop/s->eau->nb) : 0;
  for(int i = s->N-1; i >= 0; i--){
    Forme *f = parId(s, s->SID[i]); double h = HH[i];
    if(!f || (f->mort > 0 && R01() < f->mort) || Y[i] > H + 1.6*h || Y[i] < -3*h || (evap && s->LQ[i] && R01() < evap)) retirer(s, i);
  }
  if(s->N - s->eau->nb > PLAFOND*s->qualite){
    for(int k = 0; k < s->nf; k++){ Forme *f = s->formes[k]; if(f->mort <= 0 && !f->liquide && !tenue(s, f)){ f->mort = .03; break; } }
  }
  for(int k = s->nf-1; k >= 0; k--){
    Forme *f = s->formes[k];
    if(f->liquide){ if(f->nb <= 0) f->mort = 0; continue; }
    if(f->nb <= 0 && !tenue(s, f)){
      son_disparition(s, f);
      s->parId[f->id] = NULL; free(f);
      memmove(s->formes + k, s->formes + k + 1, sizeof(Forme*)*(s->nf - k - 1)); s->nf--;
    } else f->t += P->vitesse;
  }
  s->tFond += P->vitesse;
  N = s->N;

  /* 2. gravité, frottement, séchage */
  double ang = P->inclinaison*PI/180, sx = sin(ang), sy = cos(ang);
  double g = (.012 + .21*P->mollesse*P->mollesse)*P->gravite;
  double gl = PH.GL*P->gravite, frL = PH.FL0 + PH.FL1*P->viscosite;
  double frein = .035 + .24*P->viscosite;
  double sec = P->sechage <= .01 ? INFINITY : 160 + 2600*pow(1 - P->sechage, 2);
  for(int i = 0; i < N; i++){
    if(s->LQ[i]){
      double k = 1 - frL - .05*mn2(1, CT[i]);
      VX[i] = (float)((VX[i] + gl*sx)*k); VY[i] = (float)((VY[i] + gl*sy)*k);
      PXa[i] = X[i]; PYa[i] = Y[i]; X[i] += VX[i]; Y[i] += VY[i]; CT[i] = 0;
      continue;
    }
    if(s->FZ[i]) continue;
    if(s->RB[i]){                                   /* balle molle : ni séchage ni frein, elle garde son élan */
      VX[i] = (float)((VX[i] + g*1.3*sx)*.995); VY[i] = (float)((VY[i] + g*1.3*sy)*.995);
      PXa[i] = X[i]; PYa[i] = Y[i]; X[i] += VX[i]; Y[i] += VY[i]; CT[i] = 0;
      continue;
    }
    s->AGE[i]++;
    double d = 0;
    if(sec != INFINITY){
      d = borne((s->AGE[i] - .35*sec)/(.65*sec), 0, 1);
      if(d >= 1){ s->FZ[i] = 1; VX[i] = VY[i] = 0; PXa[i] = X[i]; PYa[i] = Y[i]; if(s->son){ s->son->evGel++; s->son->evGelX += X[i]; } continue; }
    }
    double fr = mn2(.9, frein + .28*mn2(1, CT[i]));
    fr += (1 - fr)*d*d;
    VX[i] = (float)((VX[i] + g*GM[i]*sx)*(1 - fr)); VY[i] = (float)((VY[i] + g*GM[i]*sy)*(1 - fr));
    PXa[i] = X[i]; PYa[i] = Y[i]; X[i] += VX[i]; Y[i] += VY[i]; CT[i] = 0;
  }

  /* 3. pression et cohésion */
  hacher(s);
  double heurt = P->contact*.2, heurtL = mx2(heurt, PH.HL);
  {
    Grille *G = &s->GS; int cols = G->cols, rows = G->rows; int *debut = G->debut, *ordre = G->ordre;
    for(int i = 0; i < N; i++){
      if(s->LQ[i] || s->FZ[i]) continue;
      double hi = HH[i], hi2 = hi*hi, xi = X[i], yi = Y[i]; int si = s->SID[i];
      int c = s->celDe[i], ccx = c % cols, ccy = c / cols;
      double rho = 0, rhoN = 0, ct = 0; int nv = 0;
      for(int oy = -1; oy <= 1; oy++){
        int cy = ccy + oy; if(cy < 0 || cy >= rows) continue;
        for(int ox = -1; ox <= 1; ox++){
          int cx = ccx + ox; if(cx < 0 || cx >= cols) continue;
          int cc = cy*cols + cx;
          for(int k = debut[cc], fin = debut[cc+1]; k < fin; k++){
            int j = ordre[k]; if(j == i) continue;
            double dx = X[j] - xi, dy = Y[j] - yi, r2 = dx*dx + dy*dy;
            if(s->SID[j] == si){
              if(r2 < hi2 && nv < 512){
                double r = sqrt(r2); if(r == 0) r = .001; double a = 1 - r/hi;
                rho += a*a; rhoN += a*a*a;
                s->vJ[nv] = j; s->vA[nv] = (float)a; s->vUx[nv] = (float)(dx/r); s->vUy[nv] = (float)(dy/r); nv++;
              }
            }else{
              double hc = .45*(hi + HH[j]);
              if(r2 < hc*hc){
                double r = sqrt(r2); if(r == 0) r = .001; double a = 1 - r/hc;
                ct += a;
                if(s->RB[i] && !s->RB[j] && GM[j] > 0){      /* elle rebondit sur l'autre forme */
                  double D = .55*a*a*hc, ux = dx/r, uy = dy/r; X[i] -= (float)(ux*D); Y[i] -= (float)(uy*D);
                  Forme *fr = parId(s, si); if(fr) fr->chocF += a; continue;
                }
                if(s->RB[j] && !s->RB[i]) continue;
                if(heurt > 0 && GM[i] > 0 && GM[j] > 0){
                  double D = heurt*a*a*hc, ux = dx/r, uy = dy/r;
                  if(s->FZ[j]){ X[i] -= (float)(ux*D); Y[i] -= (float)(uy*D); }
                  else { X[i] -= (float)(ux*D*.5); Y[i] -= (float)(uy*D*.5); X[j] += (float)(ux*D*.5); Y[j] += (float)(uy*D*.5); }
                }
              }
            }
          }
        }
      }
      CT[i] = (float)ct;
      double pr = KP*(rho - RHO0), prN = KN*rhoN, ddx = 0, ddy = 0;
      for(int n = 0; n < nv; n++){
        double a = s->vA[n]; int j = s->vJ[n]; double D = hi*(pr*a + prN*a*a)*.5;
        if(!s->FZ[j]){ X[j] += (float)(s->vUx[n]*D); Y[j] += (float)(s->vUy[n]*D); }
        ddx -= s->vUx[n]*D; ddy -= s->vUy[n]*D;
      }
      X[i] += (float)ddx; Y[i] += (float)ddy;
    }
  }
  if(s->eau->nb > 0){
    Grille *G = &s->GL, *S = &s->GS; int cols = G->cols, rows = G->rows; int *debut = G->debut, *ordre = G->ordre;
    int sc = S->cs, scols = S->cols, srows = S->rows; int *sdebut = S->debut, *sordre = S->ordre, mous = S->n > 0;
    int tot = 0;
    for(int i = 0; i < N; i++){
      if(!s->LQ[i]) continue;
      double hi = HH[i], hi2 = hi*hi, lim2 = hi2*1.21, xi = X[i], yi = Y[i];
      int c = s->celDe[i], ccx = c % cols, ccy = c / cols, d0 = tot;
      double rho = 0, rhoN = 0, ct = 0; int nv = 0;
      for(int oy = -1; oy <= 1; oy++){
        int cy = ccy + oy; if(cy < 0 || cy >= rows) continue;
        for(int ox = -1; ox <= 1; ox++){
          int cx = ccx + ox; if(cx < 0 || cx >= cols) continue;
          int cc = cy*cols + cx;
          for(int k = debut[cc], fin = debut[cc+1]; k < fin; k++){
            int j = ordre[k]; if(j == i) continue;
            double dx = X[j] - xi, dy = Y[j] - yi, r2 = dx*dx + dy*dy;
            if(r2 >= lim2) continue;
            if(tot - d0 < NBMAX) s->nbJ[tot++] = j;
            if(r2 < hi2 && nv < 512){
              double r = sqrt(r2); if(r == 0) r = .001; double a = 1 - r/hi;
              rho += a*a; rhoN += a*a*a;
              s->vJ[nv] = j; s->vA[nv] = (float)a; s->vUx[nv] = (float)(dx/r); s->vUy[nv] = (float)(dy/r); nv++;
            }
          }
        }
      }
      s->nbDeb[i] = d0; s->nbFin[i] = tot;
      if(mous){
        int pc = (int)ceil(.45*(hi + sc)/sc);
        int scx = (int)borne(fl(xi/sc) + 1, 0, scols-1), scy = (int)borne(fl(yi/sc) + 1, 0, srows-1);
        for(int oy = -pc; oy <= pc; oy++){
          int cy = scy + oy; if(cy < 0 || cy >= srows) continue;
          for(int ox = -pc; ox <= pc; ox++){
            int cx = scx + ox; if(cx < 0 || cx >= scols) continue;
            int cc = cy*scols + cx;
            for(int k = sdebut[cc], fin = sdebut[cc+1]; k < fin; k++){
              int j = sordre[k]; double hc = .45*(hi + HH[j]);
              double dx = X[j] - xi, dy = Y[j] - yi, r2 = dx*dx + dy*dy;
              if(r2 >= hc*hc) continue;
              double r = sqrt(r2); if(r == 0) r = .001; double a = 1 - r/hc, D = heurtL*a*a*hc*2.5, ux = dx/r, uy = dy/r;
              ct += a; CT[j] += (float)a;
              X[i] -= (float)(ux*D*.8); Y[i] -= (float)(uy*D*.8);
              if(!s->FZ[j]){ X[j] += (float)(ux*D*.2); Y[j] += (float)(uy*D*.2); }
            }
          }
        }
      }
      CT[i] = (float)ct;
      double pr = PH.KL*(rho - PH.RHOL); if(pr < 0) pr *= PH.COH;
      double prN = PH.KNL*rhoN, ddx = 0, ddy = 0;
      for(int n = 0; n < nv; n++){
        double a = s->vA[n]; int j = s->vJ[n]; double D = hi*(pr*a + prN*a*a)*.5;
        X[j] += (float)(s->vUx[n]*D); Y[j] += (float)(s->vUy[n]*D);
        ddx -= s->vUx[n]*D; ddy -= s->vUy[n]*D;
      }
      X[i] += (float)ddx; Y[i] += (float)ddy;
    }
    for(int i = 0; i < N; i++){
      if(!s->LQ[i]) continue;
      double hi = HH[i], hi2 = hi*hi, xi = X[i], yi = Y[i], rho = 0, rhoN = 0; int nv = 0;
      for(int k = s->nbDeb[i], fin = s->nbFin[i]; k < fin; k++){
        int j = s->nbJ[k]; double dx = X[j] - xi, dy = Y[j] - yi, r2 = dx*dx + dy*dy;
        if(r2 >= hi2) continue;
        double r = sqrt(r2); if(r == 0) r = .001; double a = 1 - r/hi;
        rho += a*a; rhoN += a*a*a;
        s->vJ[nv] = j; s->vA[nv] = (float)a; s->vUx[nv] = (float)(dx/r); s->vUy[nv] = (float)(dy/r); nv++;
      }
      double pr = PH.KL*(rho - PH.RHOL); if(pr < 0) pr *= PH.COH;
      double prN = PH.KNL*rhoN, ddx = 0, ddy = 0;
      for(int n = 0; n < nv; n++){
        double a = s->vA[n]; int j = s->vJ[n]; double D = hi*(pr*a + prN*a*a)*.5;
        X[j] += (float)(s->vUx[n]*D); Y[j] += (float)(s->vUy[n]*D);
        ddx -= s->vUx[n]*D; ddy -= s->vUy[n]*D;
      }
      X[i] += (float)ddx; Y[i] += (float)ddy;
    }
  }

  /* 4. parois, vitesses */
  for(int i = 0; i < N; i++){
    if(s->FZ[i]) continue;
    double h = HH[i];
    if(s->RB[i]){                                   /* rebond sur les bords et le bas de l'écran, avec un peu de perte */
      double e = .86, vx0 = X[i] - PXa[i], vy0 = Y[i] - PYa[i], bas = H - .35*h; Forme *fr = NULL;
      if(Y[i] > bas && vy0 > 0){ Y[i] = (float)bas; PYa[i] = (float)(bas + vy0*e); if(vy0 > 1.2 && (fr = parId(s, s->SID[i]))) fr->chocF += vy0*.15; }
      if(X[i] < .35*h && vx0 < 0){ X[i] = (float)(.35*h); PXa[i] = (float)(.35*h + vx0*e); if(-vx0 > 1.2 && (fr = parId(s, s->SID[i]))) fr->chocF += -vx0*.15; }
      if(X[i] > W - .35*h && vx0 > 0){ X[i] = (float)(W - .35*h); PXa[i] = (float)(W - .35*h + vx0*e); if(vx0 > 1.2 && (fr = parId(s, s->SID[i]))) fr->chocF += vx0*.15; }
    }
    if(X[i] < 3) X[i] = 3; else if(X[i] > W-3) X[i] = (float)(W-3);
    if(P->sol){
      double fond = H - .3*h;
      if(Y[i] > fond){
        double chute = Y[i] - PYa[i];
        if(s->son && chute > 1.2 && PYa[i] < fond - .5){   /* arrivée au sol : relevée pour le son */
          if(s->LQ[i]){ s->son->evChocL++; s->son->evChocLX += X[i]; if(chute > s->son->evChocLV) s->son->evChocLV = chute; }
          else { s->son->evChocS++; s->son->evChocSX += X[i]; if(chute > s->son->evChocSV) s->son->evChocSV = chute; } }
        Y[i] = (float)fond;
      }else if(Y[i] < .3*h) Y[i] = (float)(.3*h);
    }
    double vx = X[i] - PXa[i], vy = Y[i] - PYa[i];
    if(s->LQ[i] && s->son){ double ag = fabs(vx) + fabs(vy); if(ag > .9) s->son->evEnergie += ag - .9; s->son->evEauX += X[i]; s->son->evEauN++; }
    double v2 = vx*vx + vy*vy, vm = .55*h;
    if(v2 > vm*vm){ double k = vm/sqrt(v2); vx *= k; vy *= k; }
    VX[i] = (float)vx; VY[i] = (float)vy;
  }

  /* 5. viscosité */
  double sigma = .04 + .45*P->viscosite, sigmaL = (.05 + .35*P->viscosite)*PH.VL;
  {
    Grille *G = &s->GS; int cols = G->cols, rows = G->rows; int *debut = G->debut, *ordre = G->ordre;
    for(int i = 0; i < N; i++){
      if(s->LQ[i]) continue;
      double hi = HH[i], hi2 = hi*hi; int si = s->SID[i];
      int c = s->celDe[i], ccx = c % cols, ccy = c / cols;
      for(int oy = -1; oy <= 1; oy++){
        int cy = ccy + oy; if(cy < 0 || cy >= rows) continue;
        for(int ox = -1; ox <= 1; ox++){
          int cx = ccx + ox; if(cx < 0 || cx >= cols) continue;
          int cc = cy*cols + cx;
          for(int k = debut[cc], fin = debut[cc+1]; k < fin; k++){
            int j = ordre[k];
            if(j <= i || s->SID[j] != si || (s->FZ[i] && s->FZ[j])) continue;
            double dx = X[j] - X[i], dy = Y[j] - Y[i], r2 = dx*dx + dy*dy;
            if(r2 >= hi2 || r2 < 1e-6) continue;
            double a = 1 - sqrt(r2)/hi;
            double ix = (VX[i] - VX[j])*a*sigma*.5, iy = (VY[i] - VY[j])*a*sigma*.5;
            if(s->FZ[i]){ VX[j] += (float)(2*ix); VY[j] += (float)(2*iy); }
            else if(s->FZ[j]){ VX[i] -= (float)(2*ix); VY[i] -= (float)(2*iy); }
            else { VX[i] -= (float)ix; VY[i] -= (float)iy; VX[j] += (float)ix; VY[j] += (float)iy; }
          }
        }
      }
    }
  }
  if(s->eau->nb > 0){
    for(int i = 0; i < N; i++){
      if(!s->LQ[i]) continue;
      double hi = HH[i], hi2 = hi*hi, xi = X[i], yi = Y[i];
      for(int k = s->nbDeb[i], fin = s->nbFin[i]; k < fin; k++){
        int j = s->nbJ[k]; if(j <= i) continue;
        double dx = X[j] - xi, dy = Y[j] - yi, r2 = dx*dx + dy*dy;
        if(r2 >= hi2 || r2 < 1e-6) continue;
        double a = 1 - sqrt(r2)/hi;
        double ix = (VX[i] - VX[j])*a*sigmaL*.5, iy = (VY[i] - VY[j])*a*sigmaL*.5;
        VX[i] -= (float)ix; VY[i] -= (float)iy; VX[j] += (float)ix; VY[j] += (float)iy;
      }
    }
  }
}

/* ================= toile et fond ================= */
static void accorderGrille(Syn *s){
  int g = 0;
  for(int k = 0; k < s->nf; k++){ Forme *f = s->formes[k]; if(f->nb > 0 || tenue(s, f)) g = pgcd(g, f->px ? f->px : s->P.pixel); }
  if(!g) g = s->P.pixel;
  if(g != s->PX0){ s->PX0 = g; dimensionner(s); }
}
static void finirTransition(Syn *s);
static void dimensionner(Syn *s){
  finirTransition(s);
  int px = s->PX0;
  s->gw = imx(8, (int)floor(s->Wv/px)); s->gh = imx(8, (int)floor(s->Hv/px));
  s->W = s->gw*px; s->H = s->gh*px;
  size_t n = (size_t)s->gw*s->gh;
  s->pix = realloc(s->pix, n*4); s->sousCerne = realloc(s->sousCerne, n*4); s->peintCerne = realloc(s->peintCerne, n);
  s->fk = px >= 3 ? 1 : px == 2 ? 2 : 3; s->fw = (int)ceil((double)s->gw/s->fk) + 2; s->fh = (int)ceil((double)s->gh/s->fk) + 2;
  size_t nf = (size_t)s->fw*s->fh;
  s->champ = realloc(s->champ, nf*sizeof(float)); s->majorite = realloc(s->majorite, nf);
  if(s->nvotes < nf*4){ s->nvotes = nf*4; s->votes = realloc(s->votes, s->nvotes*sizeof(float)); }
  memset(s->champ, 0, nf*sizeof(float));
  s->eau->h = borne(sqrt(s->W*s->H)/38, 12, 26);
  construireFond(s);
}

static void peindreOptique(Syn *s);
static void peindreFond(Syn *s){
  if(s->P.fond >= FOND_DIAG){ peindreOptique(s); return; }
  int px = s->PX0, gw = s->gw, gh = s->gh, kb = px == 1 ? 2 : 1; double t = s->tFond, tw = s->ttw, th = s->tth;
  PAR_LIGNES(0, gh - 1, kb, gw*gh, gy, {
    double Yp = (gy + kb*.5)*px; int tj = imn(s->tnr-1, fl(Yp/th)); double ly = Yp - tj*th;
    for(int gx = 0; gx < gw; gx += kb){
      double Xp = (gx + kb*.5)*px; int ti = imn(s->tnc-1, fl(Xp/tw));
      Tuile *T = &s->tuiles[tj*s->tnc + ti]; double lx = Xp - ti*tw, u = T->u, td = t*T->d; int v;
      switch(T->t){
        case 0: v = 0; break;
        case 1: v = 1; break;
        case 2: v = fl((ly + td*.3)/u + 4096) & 1; break;
        case 3: v = fl((lx + td*.3)/u + 4096) & 1; break;
        case 4: v = fl((lx + ly + td*.4)/(u*1.4) + 4096) & 1; break;
        case 5: v = (fl((lx + td*.2)/(u*1.5) + 4096) + fl(ly/(u*1.5))) & 1; break;
        case 6: { double ox = (T->coin & 1) ? tw : 0, oy = (T->coin & 2) ? th : 0, ex = lx - ox, ey = ly - oy;
                  v = fl((sqrt(ex*ex + ey*ey) - td*.35)/u + 4096) & 1; break; }
        case 7: { double ox = (T->coin & 1) ? tw : 0, oy = (T->coin & 2) ? th : 0;
                  v = fl((atan2(ly-oy, lx-ox) + td*.006)/(PI/2)*9 + 4096) & 1; break; }
        case 8: { double dx = lx - tw/2, dy = ly - th/2;
                  v = fl(atan2(dy, dx)/(PI*2)*4 + T->d*(sqrt(dx*dx + dy*dy)/(u*2) - t*.02) + 4096) & 1; break; }
        default: v = fl((ly + u*1.2*sin(lx/(u*2.2) + td*.05))/u + 4096) & 1;
      }
      for(int k = 0; k < s->nsol; k++){
        Soleil *S = &s->soleils[k]; double dx = Xp - S->x, dy = Yp - S->y, d2 = dx*dx + dy*dy;
        if(d2 < S->R*S->R){ double d = sqrt(d2);
          v = d < S->R*.15 ? 0 : d < S->R*.3 ? 1 : fl((atan2(dy, dx) + t*.005*S->d)/(PI*2)*S->n + 8*S->n) & 1; }
      }
      uint32_t c = v ? BLANC : NOIR; int q = gy*gw + gx;
      s->fondPix[q] = c;
      if(kb == 2){ if(gx + 1 < gw) s->fondPix[q + 1] = c; if(gy + 1 < gh){ s->fondPix[q + gw] = c; if(gx + 1 < gw) s->fondPix[q + gw + 1] = c; } }
    }
  });
}
/* fonds d'art optique, en noir et blanc, animés : diagonales ondulantes, anneaux qui respirent (parfois en moiré),
   ou une palette de cibles concentriques qui pulsent, une sur deux à contresens */
static void peindreOptique(Syn *s){
  int px = s->PX0, gw = s->gw, gh = s->gh, kb = px == 1 ? 2 : 1; double t = s->tFond, pf = s->opt.pf;
  double ca = cos(s->opt.ang), sa = sin(s->opt.ang), w = s->opt.w; int sorte = s->opt.sorte;
  PAR_LIGNES(0, gh - 1, kb, gw*gh, gy, {
    double Y = (floor((gy + kb*.5)*px/pf) + .5)*pf;
    for(int gx = 0; gx < gw; gx += kb){
      double X = (floor((gx + kb*.5)*px/pf) + .5)*pf; int v;
      if(sorte == FOND_RONDS){
        double S = s->opt.S; int ry = fl(Y/S); double X2 = X + (s->opt.quinconce && (ry & 1) ? S/2 : 0); int rx = fl(X2/S);
        double lx = X2 - (rx + .5)*S, ly = Y - (ry + .5)*S, d = sqrt(lx*lx + ly*ly); int sens = ((rx + ry) & 1) ? 1 : -1;
        v = d > S*.5 ? ((rx + ry) & 1) : fl((d/(S*.5))*s->opt.anneaux*2 - t*.05*sens + 1e5) & 1;
      }else if(sorte == FOND_DIAG){
        double a = X*ca + Y*sa, b = -X*sa + Y*ca;
        v = fl((a + s->opt.A*sin(b/s->opt.L + t*.035) + s->opt.A*.5*sin(b/(s->opt.L*.37) - t*.021) - t*.7*s->opt.sens)/w + 1e5) & 1;
      }else{
        double dx = X - s->opt.cx, dy = Y - s->opt.cy, r = sqrt(dx*dx + dy*dy);
        double ond = 1 + .25*sin(r/(w*9) - t*.03) + .04*sin(atan2Vite(dy, dx)*s->opt.k + t*.01);
        v = fl((r - t*.6*s->opt.sens)/(w*ond) + 1e5) & 1;
        if(s->opt.moire){ double ex = X - s->opt.cx2, ey = Y - s->opt.cy2; v ^= fl((sqrt(ex*ex + ey*ey) + t*.4)/w + 1e5) & 1; }
      }
      uint32_t c = v ? BLANC : NOIR; int q = gy*gw + gx;
      s->fondPix[q] = c;
      if(kb == 2){ if(gx + 1 < gw) s->fondPix[q + 1] = c; if(gy + 1 < gh){ s->fondPix[q + gw] = c; if(gx + 1 < gw) s->fondPix[q + gw + 1] = c; } }
    }
  });
}
static void construireFond(Syn *s){
  size_t n = (size_t)s->gw*s->gh;
  s->fondPix = realloc(s->fondPix, n*4); s->aTrame = 0; s->sale = 1;
  if(s->P.fond >= FOND_DIAG){
    /* le grain de l'art optique ne suit pas la taille des pixels : il garde celui de sa création tant que la graine est la même */
    int garde = s->opt.graine == s->graineFond && s->opt.sorte == s->P.fond;
    mulberryEtat = (uint32_t)s->graineFond; double m = mn2(s->W, s->H), W = s->W, H = s->H, pf = garde ? s->opt.pf : s->PX0;
    s->opt.sorte = s->P.fond; s->opt.graine = s->graineFond; s->opt.pf = pf;
    s->opt.w = mx2(pf*2, m*(.012 + .02*mulberry())); s->opt.S = mx2(pf*16, m*(.24 + .16*mulberry())); s->opt.anneaux = 3 + (int)(mulberry()*3);
    s->opt.quinconce = mulberry() < .5; s->opt.sens = mulberry() < .5 ? -1 : 1; s->opt.ang = (mulberry() < .5 ? 1 : -1)*(PI/4 + (mulberry() - .5)*.5);
    s->opt.A = m*(.03 + .06*mulberry()); s->opt.L = m*(.12 + .2*mulberry()); s->opt.cx = W*(.3 + .4*mulberry()); s->opt.cy = H*(.3 + .4*mulberry());
    s->opt.cx2 = W*(.2 + .6*mulberry()); s->opt.cy2 = H*(.2 + .6*mulberry()); s->opt.moire = mulberry() < .2; s->opt.k = 3 + (int)(mulberry()*5);
    s->aTrame = 1; peindreOptique(s); return;
  }
  if(s->P.fond != FOND_TRAME){ uint32_t c = s->P.fond == FOND_NOIR ? NOIR : BLANC; for(size_t i = 0; i < n; i++) s->fondPix[i] = c; return; }
  mulberryEtat = (uint32_t)s->graineFond; int px = s->PX0; double W = s->W, H = s->H;
  int nc = imx(2, imn(4, (int)lround(W/230))) + (int)(mulberry()*2), nr = imx(2, (int)lround(nc*H/W));
  s->tnc = nc; s->tnr = nr; s->ttw = W/nc; s->tth = H/nr;
  s->tuiles = realloc(s->tuiles, sizeof(Tuile)*nc*nr);
  for(int k = 0; k < nc*nr; k++){ Tuile *T = &s->tuiles[k];
    T->t = (int)(mulberry()*10); T->u = mx2(px, s->ttw/(5 + (int)(mulberry()*7))); T->coin = (int)(mulberry()*4); T->d = mulberry() < .5 ? -1 : 1; }
  s->nsol = 2 + (int)(mulberry()*2);
  for(int k = 0; k < s->nsol; k++){ Soleil *S = &s->soleils[k];
    S->x = mulberry()*W; S->y = mulberry()*H; S->R = (.17 + .2*mulberry())*mn2(W, H); S->n = 2*(10 + (int)(mulberry()*14)); S->d = mulberry() < .5 ? -1 : 1; }
  s->aTrame = 1;
  peindreFond(s);
}

/* ================= couleurs des motifs ================= */
static uint32_t couleurDeBase(const Forme *f, double x, double y){
  double gl = (f->px ? f->px : 1)/7.0;
  switch(f->motif){
    /* ——— neuf motifs d'op art, d'après des images d'Etienne, animés pour hypnotiser (port de la version web) ——— */
    case M_TOURBILLON: {   /* des anneaux polygonaux de rayures tordues, chacun à contresens de son voisin, qui s'enfoncent */
      double dx = x - f->cx, dy = y - f->cy, r = sqrt(dx*dx + dy*dy) + 1e-6, r0 = f->ue*1.6; if(r < r0) return f->c1;
      double th = atan2(dy, dx), sec = PI*2/f->m, phi = th - sec*floor(th/sec) - sec*.5, tr = f->t/2.6;
      double L = log(r*cos(phi)/r0 + 1e-6)*f->kr + f->t*.012; int k = fl(L); double fr = L - k;
      double a = th/(PI*2)*f->n + fr*f->tw + ((k & 1) ? 1 : -1)*tr*.004*f->sens + k*.37;
      return (fl(a + 64*f->n) & 1) ? f->c0 : f->c1; }
    case M_ARCHES: {   /* des arches de rayures en quinconce sur des rayures verticales, qui ondulent */
      double w = f->ue, R = w*(2*f->na + 1), S2 = 2*R, V = R*f->kv, A = R*.35, X0 = x - f->x0, Y0 = y - f->y0; int i0 = fl((Y0 + R + A)/V);
      for(int iy = i0; iy >= i0 - 4; iy--){
        double dec = (iy & 1) ? .5 : 0; int ix = (int)lround(X0/S2 - dec); double cx = (ix + dec)*S2, cy = iy*V + A*sin(f->t*.02 + ix*.9 + iy*1.7);
        double dx = X0 - cx, dy = Y0 - cy; if(fabs(dx) >= R) continue; if(dy < 0 && dx*dx + dy*dy >= R*R) continue;
        double rho = dy >= 0 ? fabs(dx) : sqrt(dx*dx + dy*dy); return (fl(rho/w - f->t*.025 + 64) & 1) ? f->c1 : f->c0; }
      return (fl(X0/w + 4096) & 1) ? f->c1 : f->c0; }
    case M_PRISMES: {   /* des alvéoles coupées en deux, dont la coupe tourne en vague */
      double sz = f->ue, X0 = (x - f->x0)/sz, Y0 = (y - f->y0)/sz, q = sqrt(3)/3*X0 - Y0/3, r = 2.0/3*Y0;
      double rx = round(q), rz = round(r), ry = round(-q - r), ex = fabs(rx - q), ez = fabs(rz - r), ey = fabs(ry + q + r);
      if(ex > ey && ex > ez) rx = -ry - rz; else if(ez > ey) rz = -rx - ry;
      double lx = X0 - sqrt(3)*(rx + rz/2), ly = Y0 - 1.5*rz, d = mx2(fabs(lx), mx2(fabs(.5*lx + .866*ly), fabs(-.5*lx + .866*ly)))/.866;
      if(d > .93) return f->c1; if(d > .66) return f->c0; if(d > .52) return f->c1;
      double th = f->t/2.6*.025*f->sens + (rx + 2*rz)*.35; return lx*cos(th) + ly*sin(th) > 0 ? f->c0 : f->c1; }
    case M_SOMMETS: {   /* un treillis de losanges où s'emboîtent des chevrons qui montent sans fin */
      double a = f->ue*f->ns*1.1, b = a*1.85, u = (x - f->x0)/a + (y - f->y0)/b, v = (x - f->x0)/a - (y - f->y0)/b;
      double lu = u/2 - floor(u/2) - .5, lv = v/2 - floor(v/2) - .5, lx = lu + lv, ly = lu - lv;
      if(fabs(lx) + fabs(ly) > .94) return f->c0;
      return (fl((ly - fabs(lx) + 1)*f->ns*.5 + f->t*.03 + 64) & 1) ? f->c0 : f->c1; }
    case M_FAILLES: {   /* des rayures verticales décalées le long d'arcs concentriques, qui s'éloignent en ondes */
      double dx = x - f->x0 - f->ox, dy = y - f->y0 - f->oy; int anneau = fl(sqrt(dx*dx + dy*dy)/f->Ro - f->t*.012);
      return (fl(((x - f->x0)/(2*f->ue) + anneau*f->dec)*2 + 4096) & 1) ? f->c0 : f->c1; }
    case M_ABIME: {   /* un tunnel en étoile dont les contours s'enfoncent et tournent lentement */
      double dx = x - f->cx, dy = y - f->cy, r = sqrt(dx*dx + dy*dy) + 1e-6, th = atan2(dy, dx) + f->t/2.6*.0015*f->sens;
      double v = th/(PI*2)*f->m, tri = fabs((v - floor(v))*2 - 1), d = r*(1 - f->st*tri);
      return (fl(log(d/f->ue + 1)*f->kr + f->t*.014 + 64) & 1) ? f->c0 : f->c1; }
    case M_CADRES: {   /* des carrés concentriques en pyramides : une sur deux monte, sa voisine s'enfonce */
      double T = f->ue*2*f->nr, X0 = (x - f->x0)/T, Y0 = (y - f->y0)/T; int ix = fl(X0), iy = fl(Y0); double lx = X0 - ix - .5, ly = Y0 - iy - .5;
      double ax = fabs(lx), ay = fabs(ly); int hz = ay > ax; double d = (hz ? ay : ax)*2;
      return (fl(d*f->nr + (hz ? .5 : 0) + f->t*.02*(((ix + iy) & 1) ? 1 : -1) + 64) & 1) ? f->c0 : f->c1; }
    case M_LOSANGES: {   /* des carreaux coupés par leurs diagonales, rayés en sens contraires, qui glissent vers leur centre */
      double T = f->ue*2*f->kl, X0 = (x - f->x0)/T, Y0 = (y - f->y0)/T; int ix = fl(X0), iy = fl(Y0); double lx = X0 - ix - .5, ly = Y0 - iy - .5;
      int hz = fabs(lx) > fabs(ly); double sens = hz ? (lx > 0 ? 1 : -1) : (ly > 0 ? 1 : -1);
      double v = (hz != (((ix + iy) & 1) == 1) ? lx + ly : lx - ly)*f->kl*2 + f->t*.03*sens;
      return (fl(v + 64) & 1) ? f->c0 : f->c1; }
    case M_FRISE: {   /* des créneaux à pentes de 45° en contours à onglets ; les rangs glissent, les contours s'en écartent */
      double w = f->ue, A = w*f->Aa, P = A*f->kp, Pv = A + 2*w*f->ee, Y = -(y - f->y0); int k0 = fl(Y/Pv); double dmin = 1e9;
      for(int q = 0; q < 2; q++){ int k = k0 + q; double Yr = Y - k*Pv, u0 = (x - f->x0)/P + f->t*.003*f->sens*((k & 1) ? -1 : 1) + (k & 1)*.5, u = u0 - floor(u0);
        double tri = 1 - fabs(u*2 - 1), T = (tri*(1 + 2*f->cc) - f->cc)*A, pe = 2*(1 + 2*f->cc)*A/P, D = fabs(mx2(Yr - A, mn2(Yr, (Yr - T)/sqrt(1 + pe*pe)))); if(D < dmin) dmin = D; }
      return (fl(dmin/w - f->t*.02 + 4096) & 1) ? f->c0 : f->c1; }
    case M_BANDES: { int i = fl((y - f->y0 + f->t*mx2(.3, gl))/f->ue) % 64; if(i < 0) i += 64; return f->bandes[i]; }
    case M_RAYURES: {
      double v = f->o == 0 ? y - f->y0 : f->o == 1 ? x - f->x0 : f->o == 2 ? (x - f->x0 + y - f->y0)*.72 : (x - f->x0 - y + f->y0)*.72;
      return (fl((v + f->t*mx2(.25, gl))/f->ue) & 1) ? f->c0 : f->c1; }
    case M_RAYONS: {
      double dx = x - f->cx, dy = y - f->cy, d2 = dx*dx + dy*dy, h = f->h;
      if(d2 < h*h*.2) return f->c2;
      if(d2 < h*h*.62) return f->c1;
      return (fl((atan2(dy, dx) + f->t*.005*f->sens)/(PI*2)*f->n + 4*f->n) & 1) ? f->c0 : f->c1; }
    case M_CERCLES: { double dx = x - f->cx, dy = y - f->cy;
      return (fl((sqrt(dx*dx + dy*dy) - f->t*.22)/f->ue + 4096) & 1) ? f->c0 : f->c1; }
    case M_DAMIER:
      return ((fl((x - f->x0 + f->t*mx2(.2, gl)*f->sens)/f->ue + 4096) + fl((y - f->y0)/f->ue + 4096)) & 1) ? f->c0 : f->c1;
    case M_SPIRALE: { double dx = x - f->cx, dy = y - f->cy;
      double k = atan2(dy, dx)/(PI*2)*f->n + f->sens*(sqrt(dx*dx + dy*dy)/(f->ue*2) - f->t*.02);
      return (fl(k + 4096) & 1) ? f->c0 : f->c1; }
    case M_HELICE: { double dx = x - f->cx, dy = y - f->cy, d = sqrt(dx*dx + dy*dy);
      double a = atan2(dy, dx) + .9*sin(d/(f->h*1.1) - f->t*.04) + f->t*.004*f->sens;
      return (fl(a/(PI*2)*f->n + 8*f->n) & 1) ? f->c0 : f->c1; }
    case M_MOIRE: { double dx = x - f->cx, dy = y - f->cy, r = f->h*.55;
      double ox = cos(f->t*.011)*r, oy = sin(f->t*.014)*r, ax = dx - ox, ay = dy - oy, bx = dx + ox, by = dy + oy;
      int a = fl(sqrt(ax*ax + ay*ay)/f->ue), b = fl(sqrt(bx*bx + by*by)/f->ue);
      return ((a ^ b) & 1) ? f->c0 : f->c1; }
    case M_TUNNEL: { double dx = fabs(x - f->cx), dy = fabs(y - f->cy);
      int i = fl(((f->o ? (dx + dy)*.72 : (dx > dy ? dx : dy)) - f->t*mx2(.3, gl))/f->ue);
      if(f->aBandes) return f->bandes[((i % 64) + 64) % 64];
      return (i & 1) ? f->c0 : f->c1; }
    case M_CIBLE: { double dx = x - f->cx, dy = y - f->cy; int i = fl((sqrt(dx*dx + dy*dy) - f->t*.3)/f->ue);
      return f->bandes[((i % 64) + 64) % 64]; }
    case M_ONDES: { double a = f->o ? x - f->x0 : y - f->y0, b = f->o ? y - f->y0 : x - f->x0;
      return (fl((a + f->amp*sin(b/f->lam + f->t*.05*f->sens) + f->t*mx2(.15, gl))/f->ue + 4096) & 1) ? f->c0 : f->c1; }
    case M_CHEVRONS: { double w = f->ue*2.5, z = fmod((x - f->x0)/w, 2); if(z < 0) z += 2; if(z > 1) z = 2 - z;
      return (fl((y - f->y0 + z*w + f->t*mx2(.3, gl)*f->sens)/f->ue + 4096) & 1) ? f->c0 : f->c1; }
    case M_BULLE: { double dx = x - f->cx, dy = y - f->cy, R = f->h*2.4, r2 = (dx*dx + dy*dy)/(R*R);
      if(r2 < 1){ double q = 1 - r2, k = 1/(1 + (1.1 + .7*sin(f->t*.04))*q*q); dx *= k; dy *= k; }
      return ((fl(dx/f->ue + 512) + fl(dy/f->ue + 512)) & 1) ? f->c0 : f->c1; }
    case M_POIS: { double g = f->g; int iy = fl((y - f->y0)/g); double xx = x - f->x0 + (iy & 1)*g*.5; int ix = fl(xx/g);
      double lx = xx - (ix + .5)*g, ly = y - f->y0 - (iy + .5)*g, r = g*(.3 + .17*sin(f->t*.06 + (ix + iy)*.7));
      return lx*lx + ly*ly < r*r ? f->c0 : f->c1; }
    case M_PASTILLES: {
      double g = f->g*.9, X0 = (x - f->x0)/g, Y0 = (y - f->y0)/g; int ix = fl(X0), iy = fl(Y0); double lx = X0 - ix - .5, ly = Y0 - iy - .5;
      double ax = fabs(lx), ay = fabs(ly);
      if(ax < .13 && ay < .13) return f->c3;
      int pair = ((ix + iy) & 1) == 0; double tt = f->t*.03, rP = .54 + .12*sin(tt), rI = .54 - .12*sin(tt);
      double d2 = lx*lx + ly*ly, v2 = mn2((1 - ax)*(1 - ax) + ay*ay, ax*ax + (1 - ay)*(1 - ay));
      double rA = pair ? rP : rI, rV = pair ? rI : rP;
      if(rA >= rV) return d2 < rA*rA*.36 ? f->c2 : d2 < rA*rA ? f->c0 : v2 < rV*rV ? f->c0 : f->c1;
      return v2 < rV*rV ? (v2 < rV*rV*.36 ? f->c2 : f->c0) : d2 < rA*rA*.36 ? f->c2 : d2 < rA*rA ? f->c0 : f->c1; }
    case M_LOBES: {
      double g = f->g*.95, X0 = (x - f->x0)/g, Y0 = (y - f->y0)/g; int ix = fl(X0), iy = fl(Y0); double lx = X0 - ix - .5, ly = Y0 - iy - .5;
      uint32_t hh = imul32((int64_t)((uint32_t)((int64_t)ix*73856093) ^ (uint32_t)((int64_t)iy*19349663)), 0x9E3779B1u) >> 8;
      int64_t h = (int64_t)hh + (int64_t)f->graine; int q = (int)(h & 3);
      double R = .5 + .17*sin(f->t*.022 + ((h >> 2) & 7)*.8); int dans = lx*lx + ly*ly < R*R;
      int k = dans ? q : (int)((q + 1 + ((h >> 5) % 3)) & 3);
      return k == 0 ? f->c0 : k == 1 ? f->c2 : k == 2 ? f->c3 : f->c1; }
    case M_ZEBRE: { double X0 = x - f->x0, Y0 = y - f->y0, tt = f->t*.02;
      double w = Y0 + f->amp*sin(X0/f->lam + tt*1.3) + f->amp*.6*sin((X0 + Y0)/(f->lam*1.7) - tt) + f->amp*.4*sin(Y0/(f->lam*.8) + tt*.7);
      return (fl(w/f->ue + 4096) & 1) ? f->c0 : f->c1; }
    case M_LABYRINTHE: {
      double g = f->g; int ix = fl((x - f->x0)/g), iy = fl((y - f->y0)/g);
      double lx = (x - f->x0)/g - ix, ly = (y - f->y0)/g - iy, hs = h1(ix*12.9898 + iy*78.233 + f->graine);
      double ph = (f->t + hs*240)/240; int tour = fl(ph); double fr = ph - tour, ep = .2*mn2(1, 7*mn2(fr, 1 - fr));
      int bit = h1(ix*3.1 + iy*7.7 + tour*1.37 + f->graine) < .5;
      double dd = bit ? fabs(lx - ly) : fabs(lx + ly - 1);
      return (dd < ep || dd > 1 - ep) ? f->c0 : f->c1; }
    case M_ECAILLES: {
      double g = f->g*1.9, X0 = x - f->x0, Y0 = y - f->y0 + f->t*mx2(.12, gl);
      int j = fl(Y0/(g*.5));
      for(int k = 0; k < 2; k++, j--){
        double lx = fmod(fmod(X0 + (j & 1)*g*.5, g) + g, g) - g*.5, ly = Y0 - j*g*.5, d = sqrt(lx*lx + ly*ly);
        if(d < g*.5 || k == 1) return (fl(d/(g*.125)) & 1) ? f->c0 : f->c1;
      }
      return f->c1; }
    case M_HEXAGONES: {
      double s6 = f->g*.9, hx = s6, hy = s6*1.732, X0 = x - f->x0, Y0 = y - f->y0;
      double ax = fmod(fmod(X0, hx) + hx, hx) - hx*.5, ay = fmod(fmod(Y0, hy) + hy, hy) - hy*.5;
      double bx = fmod(fmod(X0 - hx*.5, hx) + hx, hx) - hx*.5, by = fmod(fmod(Y0 - hy*.5, hy) + hy, hy) - hy*.5;
      if(bx*bx + by*by < ax*ax + ay*ay){ ax = bx; ay = by; }
      ax = fabs(ax); ay = fabs(ay);
      return (fl(mx2(ax, ax*.5 + ay*.866)/(s6*.17) - f->t*.03 + 64) & 1) ? f->c0 : f->c1; }
    case M_POLAIRE: { double dx = x - f->cx, dy = y - f->cy, d = sqrt(dx*dx + dy*dy) + 1;
      return ((fl(atan2(dy, dx)/(PI*2)*f->n + f->t*.012*f->sens + 4*f->n) + fl(log(d)*3.2 - f->t*.012 + 64)) & 1) ? f->c0 : f->c1; }
    case M_ETOILE: { double dx = x - f->cx, dy = y - f->cy;
      double r = sqrt(dx*dx + dy*dy)/(.78 + .22*cos(f->n*atan2(dy, dx) + f->t*.02*f->sens));
      return (fl(r/f->ue - f->t*.04 + 4096) & 1) ? f->c0 : f->c1; }
    case M_BRIQUES: {
      double m = mx2(f->px ? f->px : 1, f->ue*.3), bh = mx2(f->ue*1.6, m*3), bw = bh*2.4; int rang = fl((y - f->y0)/bh);
      double lx = fmod(fmod(x - f->x0 + (rang & 1)*bw*.5 + f->t*mx2(.25, gl)*((rang & 1) ? 1 : -1), bw) + bw, bw), ly = (y - f->y0) - rang*bh;
      return (lx < m || ly < m) ? f->c1 : f->c0; }
    case M_CROIX: {
      double g = f->g*1.1; int iy = fl((y - f->y0)/g); double xx = x - f->x0 + (iy & 1)*g*.5; int ix = fl(xx/g);
      double lx = fabs(xx - (ix + .5)*g), ly = fabs(y - f->y0 - (iy + .5)*g);
      double a = g*(.13 + .05*sin(f->t*.06 + ix*1.3 + iy*.9)), l = g*.37;
      return ((lx < a && ly < l) || (ly < a && lx < l)) ? f->c0 : f->c1; }
    /* méandre : des L emboîtés depuis un coin de chaque tuile, coins tirés au sort */
    case M_MEANDRE: {
      double S = f->ue*2*f->n, X0 = (x - f->x0)/S, Y0 = (y - f->y0)/S; int ix = fl(X0), iy = fl(Y0);
      double lx = X0 - ix, ly = Y0 - iy; int q = (int)(h1(ix*12.9898 + iy*78.233 + f->graine)*4);
      if(q & 1) lx = 1 - lx; if(q & 2) ly = 1 - ly;
      return (fl((lx > ly ? lx : ly)*S/f->ue - f->t*.03*f->sens + 4096) & 1) ? f->c0 : f->c1; }
    /* vertige : damier polaire aux anneaux bombés */
    case M_VERTIGE: {
      double dx = x - f->cx, dy = y - f->cy, a = atan2(dy, dx)/(PI*2)*f->n + 4*f->n; int k = fl(a); double fr = a - k;
      int anneau = fl(log(sqrt(dx*dx + dy*dy)/f->ue + 1)*f->amp - f->t*.015 + sin(PI*fr)*.5*((k & 1) ? 1 : -1));
      return ((anneau + k) & 1) ? f->c0 : f->c1; }
    /* sphères : disques emboîtés de rayures tordues, en sens alternés */
    case M_SPHERES: {
      double dx = x - f->cx, dy = y - f->cy, r = sqrt(dx*dx + dy*dy), R = f->h*2.6; int k = 0; while(k < 4 && r < R){ k++; R *= .62; }
      double sg = (k & 1) ? 1 : -1, a = atan2(dy, dx)/(PI*2)*f->n + sg*(r/(f->ue*2.4*pow(.72, k)) + f->t*.006);
      return (fl(a + 8*f->n) & 1) ? f->c0 : f->c1; }
    /* rosace : étoiles en zigzag emboîtées */
    case M_ROSACE: {
      double dx = x - f->cx, dy = y - f->cy, a = atan2(dy, dx)/(PI*2)*f->n + f->t*.002*f->sens, tri = fabs((a - floor(a))*2 - 1);
      double w = sqrt(dx*dx + dy*dy)*(1 + .22*tri)/(f->ue*3.4) - f->t*.03;
      return w - floor(w) < .5 ? f->c0 : f->c1; }
    /* corail : rayures organiques à la Turing */
    case M_CORAIL: {
      double sc = mx2(f->ue*6.2, (f->px ? f->px : 1)*10), X0 = (x - f->x0)/sc, Y0 = (y - f->y0)/sc, tt = f->t*.004;
      double u = X0 + 1.3*sin(Y0*.55 + tt), v = Y0 + 1.3*sin(X0*.5 - tt*.8);
      return (fl((bruit2(u, v)*.65 + bruit2(u*2.1 + 3.3, v*2.1 - 1.7)*.35)*9 + f->t*.003) & 1) ? f->c0 : f->c1; }
    /* soleils : grille de soleils rayonnants séparés par une croix */
    case M_SOLEILS: {
      double S = f->ue*10.8, X0 = (x - f->x0)/S, Y0 = (y - f->y0)/S; int ix = fl(X0), iy = fl(Y0); double lx = X0 - ix - .5, ly = Y0 - iy - .5;
      if(fabs(lx) > .45 || fabs(ly) > .46) return f->c1;
      if(lx*lx + ly*ly < .02) return f->c2;
      double a = atan2(ly, lx)/(PI*2)*f->n + f->t*.004*(((ix + iy) & 1) ? 1 : -1);
      return (fl(a + 8*f->n) & 1) ? f->c0 : f->c1; }
    /* décalage : barres de couleur dont chaque rang glisse sur une onde */
    case M_DECALAGE: {
      int rang = fl((y - f->y0)/f->ue); double L = f->g*1.7, sh = L*.8*sin(rang*f->lam + f->t*.03*f->sens);
      int i = (fl((x - f->x0 + sh)/L) + ((rang & 1) ? 32 : 0)) % 64; if(i < 0) i += 64;
      return f->bandes[i]; }
    /* fleurs : pétales rayés sur un fond de rayures verticales */
    case M_FLEURS: {
      double S = f->ue*9.6, Y0 = (y - f->y0)/S; int iy = fl(Y0); double X0 = (x - f->x0)/S + (iy & 1)*.5; int ix = fl(X0);
      double lx = X0 - ix - .5, ly = Y0 - iy - .5, h = h1(ix*12.9898 + iy*78.233 + f->graine), r = sqrt(lx*lx + ly*ly); int p = 5 + (int)(h*2);
      double th = atan2(ly, lx) + f->t*.004*(((int)(h*7) & 1) ? 1 : -1);
      if(r < .12) return f->c3;
      if(r < .46*(.68 + .32*cos(p*th))){ double a = PI/2 + (h - .5)*.6, w = (lx*cos(a) + ly*sin(a))/.09; return (fl(w + 64) & 1) ? f->c0 : f->c2; }
      return (fl((x - f->x0)/(f->ue*1.6) + 4096) & 1) ? f->c1 : (f->c1 == BLANC ? NOIR : BLANC); }
    default: return f->c0;
  }
}
static uint32_t couleur(const Forme *f, double x, double y){
  uint32_t c = couleurDeBase(f, x, y);
  if(c != SPECTRE || !f->arc) return c;
  int i = fl(((x - f->x0) + (y - f->y0))*.7/f->pasArc + f->t*.12*f->sens + f->arc0) % 24; if(i < 0) i += 24;
  return ARC[i];
}
static uint32_t couleurChamp(const Forme *f, double x, double y, double v){
  double u = mx2(0, (v - SEUIL)/(f->motif == M_CONTOURS ? 14 : 20));
  if(f->motif == M_CONTOURS){
    if(u < .05) return 0;
    if(v > f->vcoeur) return f->bandes[0];
    int i = fl(sqrt(u)*f->kn - f->t*.03*f->sens + 4096) % 64;
    return f->bandes[i];
  }
  if(u >= .3) return f->c0 == SPECTRE ? couleur(f, x, y) : f->c0;
  double w = atan2(y - f->cy, x - f->cx)/(PI*2)*f->n + f->t*.004*f->sens; w -= floor(w);
  double e = u/.3;
  return fabs(2*w - 1) < e*e*.9 + .04 ? f->c2 : 0;
}
static inline double gris(uint32_t c){ return ((c & 255)*.299 + ((c >> 8) & 255)*.587 + ((c >> 16) & 255)*.114)/255; }
static inline uint32_t tramer(uint32_t c, double v, double s0, int bx, int by){
  double L = gris(c)*(.3 + .7*borne((v - s0)*1.5, 0, 1));
  return L > BAYER[(by & 7)*8 + (bx & 7)] ? BLANC : NOIR;
}
static uint32_t teinteForme(const Forme *f, double X, double Y, double v, int bx, int by){
  uint32_t c = f->parChamp ? couleurChamp(f, X, Y, v) : couleur(f, X, Y);
  return c && f->bit ? tramer(c, v, SEUIL, bx, by) : c;
}
static void bloc(Syn *s, int x0, int y0, int r, uint32_t c){
  int x1 = imn(s->gw, x0 + r), y1 = imn(s->gh, y0 + r); if(x0 < 0) x0 = 0; if(y0 < 0) y0 = 0;
  for(int y = y0; y < y1; y++){ uint32_t *o = s->pix + (size_t)y*s->gw; for(int x = x0; x < x1; x++) o[x] = c; }
}
static void cellule(Syn *s, int x0, int y0, int k, int car, uint32_t fg, uint32_t bg){
  const uint8_t *g = POLICE[car];
  for(int ly = 0; ly < 8; ly++){
    int ligne = ly < 7 ? g[ly] : 0;
    for(int lx = 0; lx < 6; lx++) bloc(s, x0 + lx*k, y0 + ly*k, k, lx < 5 && ((ligne >> (4 - lx)) & 1) ? fg : bg);
  }
}

/* ================= rendu ================= */
typedef struct { const float *champ; int fw, kf, bx0, bx1, by0, by1; } Lecteur;
static inline double val(const Lecteur *L, int x, int y){
  if(L->kf == 1) return L->champ[y*L->fw + x];
  double fx = (x + .5)/L->kf, fy = (y + .5)/L->kf; int i = (int)fx, j = (int)fy;
  if(i < L->bx0) i = L->bx0; else if(i > L->bx1 - 1) i = L->bx1 - 1; if(j < L->by0) j = L->by0; else if(j > L->by1 - 1) j = L->by1 - 1;
  double tx = fx - i, ty = fy - j; int a = j*L->fw + i;
  return (L->champ[a]*(1 - tx) + L->champ[a + 1]*tx)*(1 - ty) + (L->champ[a + L->fw]*(1 - tx) + L->champ[a + L->fw + 1]*tx)*ty;
}

/* peint une scène (une génération de gouttes) sur son fond, dans dst */
static void rendreGen(Syn *s, uint32_t *dst, const uint32_t *fond, int gen){
  uint32_t *garde = s->pix; s->pix = dst;
  int px = s->PX0, gw = s->gw, gh = s->gh, fw = s->fw, fh = s->fh;
  uint32_t *pix = s->pix; float *champ = s->champ;
  PAR_LIGNES(0, gh - 1, 1, gw*gh/4, y, memcpy(pix + (size_t)y*gw, fond + (size_t)y*gw, (size_t)gw*4););

  int nf = s->nf;
  for(int k = 0; k < nf; k++){ Forme *f = s->formes[k]; f->deb = 0; f->cpt = 0; f->sx = 0; f->sy = 0; }
  for(int i = 0; i < s->N; i++){ if(s->GEN[i] != gen) continue; Forme *f = parId(s, s->SID[i]); if(f) f->cpt++; }
  int cumul = 0;
  for(int k = 0; k < nf; k++){ Forme *f = s->formes[k]; f->deb = cumul; cumul += f->cpt; f->cpt = 0; }
  for(int i = 0; i < s->N; i++){ if(s->GEN[i] != gen) continue; Forme *f = parId(s, s->SID[i]); if(!f) continue; s->trie[f->deb + f->cpt++] = i; f->sx += s->X[i]; f->sy += s->Y[i]; }

  int kf = s->fk; double dec = kf == 1 ? .5 : 0, pf = px*kf;
  for(int k = 0; k < nf; k++){
    Forme *f = s->formes[k]; if(!f->cpt) continue;
    double mx = f->sx/f->cpt, my = f->sy/f->cpt;
    f->cx += (mx - f->cx)*.12; f->cy += (my - f->cy)*.12;
    int pxf = f->px ? f->px : px;
    f->ue = mx2(f->u*(f->ke ? f->ke : 1), pxf); f->g = mx2(f->ue*2.4, pxf*4);
    double R = mx2(f->h*(f->liquide ? PH.RL : f->motif == M_POINTES ? 2.5 : f->motif == M_CONTOURS ? 1.9 : .95), px*1.5), rc = R/pf, rc2 = rc*rc;
    double marge = (f->liquide ? rc + 1.8*f->h/pf : rc) + 1;
    int bx0 = fw, by0 = fh, bx1 = -1, by1 = -1;
    for(int n = f->deb, fin = f->deb + f->cpt; n < fin; n++){
      int i = s->trie[n]; double gx = s->X[i]/pf, gy = s->Y[i]/pf;
      int a = fl(gx - marge), b = (int)ceil(gx + marge), c = fl(gy - marge), d = (int)ceil(gy + marge);
      if(a < bx0) bx0 = a; if(b > bx1) bx1 = b; if(c < by0) by0 = c; if(d > by1) by1 = d;
    }
    if(bx0 < 0) bx0 = 0; if(by0 < 0) by0 = 0; if(bx1 > fw-1) bx1 = fw-1; if(by1 > fh-1) by1 = fh-1;
    if(bx1 <= bx0 || by1 <= by0) continue;
    for(int y = by0; y <= by1; y++) memset(champ + y*fw + bx0, 0, sizeof(float)*(bx1 - bx0 + 1));
    int xa = imx(0, (int)ceil(bx0*kf + dec - .5)), xb = imn(gw - 1, fl(bx1*kf + dec - .5));
    int ya = imx(0, (int)ceil(by0*kf + dec - .5)), yb = imn(gh - 1, fl(by1*kf + dec - .5));
    Lecteur L = { champ, fw, kf, bx0, bx1, by0, by1 };
    int r = imx(1, (int)lround((double)pxf/px));
    int ak = imx(1, (int)lround(mx2(pxf, 3)/px));

    int cerner = s->P.cerne && !f->ascii && f->motif != M_POINTES;
    int rx0 = imx(0, xa - r), ry0 = imx(0, ya - r), rx1 = imn(gw - 1, xb + r), ry1 = imn(gh - 1, yb + r);
    if(cerner){ const size_t l = (size_t)(rx1 - rx0 + 1)*4;
      for(int y = ry0; y <= ry1; y++){ const int o = y*gw + rx0; memcpy(s->sousCerne + o, pix + o, l); memset(pix + o, 0, l); } }

    if(f->liquide){
      double seuil = PH.SL, etire = PH.ET/pf, lmax = 1.8*f->h/pf, rcL = rc;
      int K = 0, particulier = 0;
      for(int q = 0; q < NT; q++) s->rangDe[q] = -1;
      for(int n = f->deb, fin = f->deb + f->cpt; n < fin; n++){ int num = (int)s->COL[s->trie[n]];
        if(s->rangDe[num] < 0){ s->rangDe[num] = K; int md = num >= AB && num < AB + AN ? 1 : num >= BB && num < BB + BN ? 2 : 0;
          s->modeDuRang[K] = (uint8_t)md; if(md) particulier = 1; s->teinteDuRang[K++] = TEINTES[num]; } }
      if(s->nvotes < (size_t)fw*fh*K){ s->nvotes = (size_t)fw*fh*K; s->votes = realloc(s->votes, s->nvotes*sizeof(float)); }
      float *votes = s->votes;
      for(int y = by0; y <= by1; y++) memset(votes + (size_t)(y*fw + bx0)*K, 0, sizeof(float)*(bx1 - bx0 + 1)*K);
      for(int n = f->deb, fin = f->deb + f->cpt; n < fin; n++){
        int i = s->trie[n]; double gx = s->X[i]/pf - dec/kf, gy = s->Y[i]/pf - dec/kf; int num = s->rangDe[s->COL[i]];
        double ux = -s->VX[i]*etire, uy = -s->VY[i]*etire, l2 = ux*ux + uy*uy;
        if(l2 > lmax*lmax){ double e = lmax/sqrt(l2); ux *= e; uy *= e; l2 = lmax*lmax; }
        double qx = gx + ux, qy = gy + uy; int lng = l2 > .04/(kf*kf);
        int nvo = s->nbFin[i] - s->nbDeb[i]; double rcc = nvo >= 21 ? rcL*.65 : nvo <= 14 ? rcL : rcL*(1 - (nvo - 14)*.05), rcc2 = rcc*rcc;
        int x0 = (int)ceil(mn2(gx, qx) - rcc), x1 = fl(mx2(gx, qx) + rcc), y0 = (int)ceil(mn2(gy, qy) - rcc), y1 = fl(mx2(gy, qy) + rcc);
        if(x0 < bx0) x0 = bx0; if(x1 > bx1) x1 = bx1; if(y0 < by0) y0 = by0; if(y1 > by1) y1 = by1;
        for(int y = y0; y <= y1; y++){
          double dy = y - gy; int rang = y*fw;
          for(int x = x0; x <= x1; x++){
            double dx = x - gx, ey = dy;
            if(lng){ double t = (dx*ux + dy*uy)/l2; t = t < 0 ? 0 : t > 1 ? 1 : t; dx -= t*ux; ey -= t*uy; }
            double d2 = dx*dx + ey*ey;
            if(d2 < rcc2){ double t = 1 - d2/rcc2, w = t*t; champ[rang + x] += (float)w; votes[(size_t)(rang + x)*K + num] += (float)w; }
          }
        }
      }
      #define RANG_A(xx, yy, out) do{ size_t o_ = (size_t)(kf == 1 ? (yy)*fw + (xx) : (int)borne(lround(((yy) + .5)/kf), by0, by1)*fw + (int)borne(lround(((xx) + .5)/kf), bx0, bx1))*K; \
          int m_ = 0; float mv_ = votes[o_]; for(int c_ = 1; c_ < K; c_++) if(votes[o_ + c_] > mv_){ mv_ = votes[o_ + c_]; m_ = c_; } out = m_; }while(0)
      if(r > 1 || particulier){
        int lettresV = 0, *lettres = &lettresV; const Lecteur *Lq = &L;
        PAR_LIGNES((ya/r)*r, yb, r, (yb - ya + 1)*(xb - xa + 1), y0, {
          int yc = y0 + (r >> 1); if(yc < ya || yc > yb) continue;
          for(int x0 = (xa/r)*r; x0 <= xb; x0 += r){
            int xc = x0 + (r >> 1); if(xc < xa || xc > xb) continue;
            double v = val(Lq, xc, yc); if(v <= seuil) continue;
            int m; RANG_A(xc, yc, m); int md = s->modeDuRang[m];
            if(md == 1){ *lettres = 1; continue; }
            bloc(s, x0, y0, r, md == 2 ? tramer(s->teinteDuRang[m], v, seuil, x0/r, y0/r) : s->teinteDuRang[m]);
          }
        });
        if(lettresV){
          int cw = 6*ak, ch = 8*ak;
          PAR_LIGNES(ya/ch, yb/ch, 1, (yb - ya + 1)*(xb - xa + 1), iy, {
            int yc = iy*ch + (ch >> 1); if(yc < ya || yc > yb) continue;
            for(int ix = xa/cw; ix*cw <= xb; ix++){
              int xc = ix*cw + (cw >> 1); if(xc < xa || xc > xb) continue;
              if(val(Lq, xc, yc) <= seuil) continue;
              int m; RANG_A(xc, yc, m); if(s->modeDuRang[m] != 1) continue;
              uint32_t h = hacheCase(ix, iy, 0), t = s->teinteDuRang[m];
              cellule(s, ix*cw, iy*ch, ak, PLUIE[(h + (uint32_t)((s->tAscii + (h & 1023))/420)) % NPLUIE], t, t == NOIR ? BLANC : NOIR);
            }
          });
        }
      }else if(kf == 1){
        PAR_LIGNES(ya, yb, 1, (yb - ya + 1)*(xb - xa + 1)*2, y, {
          int rang = y*fw, sortie = y*gw;
          for(int x = xa; x <= xb; x++){
            if(champ[rang + x] <= seuil) continue;
            size_t o = (size_t)(rang + x)*K; int m = 0; float mv = votes[o];
            for(int c = 1; c < K; c++) if(votes[o + c] > mv){ mv = votes[o + c]; m = c; }
            pix[sortie + x] = s->teinteDuRang[m];
          }
        });
      }else{
        PAR_LIGNES(by0, by1, 1, (by1 - by0 + 1)*(bx1 - bx0 + 1)*K, y, for(int x = bx0; x <= bx1; x++){
          size_t o = (size_t)(y*fw + x)*K; int m = 0; float mv = votes[o];
          for(int c = 1; c < K; c++) if(votes[o + c] > mv){ mv = votes[o + c]; m = c; }
          s->majorite[y*fw + x] = (uint8_t)m;
        });
        PAR_LIGNES(by0, by1 - 1, 1, (yb - ya + 1)*(xb - xa + 1)*2, j, {
          int y0 = imx(ya, j*kf), y1 = imn(yb, j*kf + kf - 1);
          for(int i = bx0; i < bx1; i++){
            int a = j*fw + i, b = a + fw; double v00 = champ[a], v10 = champ[a + 1], v01 = champ[b], v11 = champ[b + 1];
            if(mx2(mx2(v00, v10), mx2(v01, v11)) <= seuil) continue;
            int x0 = imx(xa, i*kf), x1 = imn(xb, i*kf + kf - 1);
            int m0 = s->majorite[a], unie = m0 == s->majorite[a + 1] && m0 == s->majorite[b] && m0 == s->majorite[b + 1];
            int pleine = mn2(mn2(v00, v10), mn2(v01, v11)) > seuil;
            if(pleine && unie){ uint32_t t0 = s->teinteDuRang[m0]; for(int y = y0; y <= y1; y++) for(int x = x0; x <= x1; x++) pix[y*gw + x] = t0; continue; }
            for(int y = y0; y <= y1; y++){
              double ty = (y + .5)/kf - j, sy = 1 - ty;
              for(int x = x0; x <= x1; x++){
                double tx = (x + .5)/kf - i, w00 = (1 - tx)*sy, w10 = tx*sy, w01 = (1 - tx)*ty, w11 = tx*ty;
                if(!pleine && v00*w00 + v10*w10 + v01*w01 + v11*w11 <= seuil) continue;
                int m = m0;
                if(!unie){ double mv = -1;
                  for(int c = 0; c < K; c++){
                    double v = votes[(size_t)a*K + c]*w00 + votes[(size_t)(a + 1)*K + c]*w10 + votes[(size_t)b*K + c]*w01 + votes[(size_t)(b + 1)*K + c]*w11;
                    if(v > mv){ mv = v; m = c; } } }
                pix[y*gw + x] = s->teinteDuRang[m];
              }
            }
          }
        });
      }
    }else{
      {
        const int *trie = s->trie + f->deb, cpt = f->cpt; const float *PXs = s->X, *PYs = s->Y;
        const float inv = (float)(1/rc2), rcf = (float)rc, decf = (float)(dec/kf), ipf = (float)(1/pf);
        const int hb = by1 - by0 + 1, nbB = (cpt*(int)(rc*rc) > 60000 && hb >= 24) ? 8 : 1;
        PAR_N(nbB, b, {
          const int ya = by0 + hb*b/nbB, yz = by0 + hb*(b + 1)/nbB - 1;   /* cette bande n'écrit que ses lignes */
          for(int n = 0; n < cpt; n++){
            const int i = trie[n]; const float gx = PXs[i]*ipf - decf, gy = PYs[i]*ipf - decf;
            int y0 = (int)ceilf(gy - rcf), y1 = (int)floorf(gy + rcf);
            if(y0 < ya) y0 = ya; if(y1 > yz) y1 = yz;
            for(int y = y0; y <= y1; y++){
              const float dy = y - gy, dy2 = dy*dy, reste = (float)rc2 - dy2; if(reste <= 0) continue;
              const float w = sqrtf(reste); int x0 = (int)ceilf(gx - w), x1 = (int)floorf(gx + w);
              if(x0 < bx0) x0 = bx0; if(x1 > bx1) x1 = bx1;
              float *rang = champ + y*fw;
              for(int x = x0; x <= x1; x++){ const float dx = x - gx, t = 1 - (dx*dx + dy2)*inv; rang[x] += t*t; }   /* boucle sans branche : vectorisée */
            }
          }
        });
      }
      if(f->motif == M_CONTOURS){
        double m = 0; for(int y = by0; y <= by1; y++){ const float *o = champ + y*fw; for(int x = bx0; x <= bx1; x++) if(o[x] > m) m = o[x]; }
        f->vmax = f->vmax ? f->vmax + (m - f->vmax)*.03 : m; f->vcoeur = SEUIL + .62*(f->vmax - SEUIL);
      }
      if(f->ascii){
        int cw = 6*ak, ch = 8*ak; uint32_t fg = f->fg, bg = f->bg; const Lecteur *La = &L;
        PAR_LIGNES(ya/ch, yb/ch, 1, (yb - ya + 1)*(xb - xa + 1), iy, {
          int yc = iy*ch + (ch >> 1); if(yc < ya || yc > yb) continue;
          for(int ix = xa/cw; ix*cw <= xb; ix++){
            int xc = ix*cw + (cw >> 1); if(xc < xa || xc > xb) continue;
            double v = val(La, xc, yc); if(v <= SEUIL) continue;
            int poids = 0;
            for(int q = 0; q < 4; q++){
              double Xq = (xc + (q & 1 ? cw : -cw)*.25 + .5)*px, Yq = (yc + (q & 2 ? ch : -ch)*.25 + .5)*px;
              uint32_t c = f->parChamp ? couleurChamp(f, Xq, Yq, v) : couleurDeBase(f, Xq, Yq);
              poids += f->aBandes ? (c == BLANC ? 0 : c == NOIR ? 2 : 1) : (c == f->c1 || !c) ? 0 : c == f->c0 ? 2 : 1;
            }
            int classe = poids == 0 ? 0 : poids < 4 ? 1 : poids < 8 ? 2 : 3;
            if(v < SEUIL*1.3 && classe > 1) classe -= 2;
            cellule(s, ix*cw, iy*ch, ak, CARS[classe][hacheCase(ix, iy, f->id) % NCARS[classe]], fg, bg);
          }
        });
      }else if(r > 1 || f->bit || f->parChamp){
        const Lecteur *Lp = &L;
        PAR_LIGNES((ya/r)*r, yb, r, (yb - ya + 1)*(xb - xa + 1), y0, {
          int yc = y0 + (r >> 1); if(yc < ya || yc > yb) continue;
          for(int x0 = (xa/r)*r; x0 <= xb; x0 += r){
            int xc = x0 + (r >> 1); if(xc < xa || xc > xb) continue;
            double v = val(Lp, xc, yc); if(v <= SEUIL) continue;
            uint32_t c = teinteForme(f, (xc + .5)*px, (yc + .5)*px, v, x0/r, y0/r);
            if(c) bloc(s, x0, y0, r, c);
          } });
      }else if(kf == 1){
        PAR_LIGNES(ya, yb, 1, (yb - ya + 1)*(xb - xa + 1), y, {
          const float *rang = champ + y*fw; uint32_t *sortie = pix + y*gw; double yp = (y + .5)*px;
          for(int x = xa; x <= xb; x++) if(rang[x] > SEUIL) sortie[x] = couleur(f, (x + .5)*px, yp); });
      }else{
        PAR_LIGNES(by0, by1 - 1, 1, (yb - ya + 1)*(xb - xa + 1), j, {
          int y0 = imx(ya, j*kf), y1 = imn(yb, j*kf + kf - 1);
          for(int i = bx0; i < bx1; i++){
            int a = j*fw + i, b = a + fw; double v00 = champ[a], v10 = champ[a + 1], v01 = champ[b], v11 = champ[b + 1];
            if(mx2(mx2(v00, v10), mx2(v01, v11)) <= SEUIL) continue;
            int x0 = imx(xa, i*kf), x1 = imn(xb, i*kf + kf - 1);
            int pleine = mn2(mn2(v00, v10), mn2(v01, v11)) > SEUIL;
            for(int y = y0; y <= y1; y++){
              double ty = (y + .5)/kf - j, yp = (y + .5)*px; uint32_t *sortie = pix + y*gw;
              for(int x = x0; x <= x1; x++){
                if(!pleine){ double tx = (x + .5)/kf - i; if((v00*(1 - tx) + v10*tx)*(1 - ty) + (v01*(1 - tx) + v11*tx)*ty <= SEUIL) continue; }
                sortie[x] = couleur(f, (x + .5)*px, yp);
              }
            }
          } });
      }
    }
    if(cerner){
      uint8_t *pc = s->peintCerne;
      const uint32_t *sous = s->sousCerne;
      PAR_LIGNES(ry0, ry1, 1, (ry1 - ry0 + 1)*(rx1 - rx0 + 1), y, {
        const int o = y*gw; for(int x = rx0; x <= rx1; x++){ const int i = o + x; const uint32_t v = pix[i]; pc[i] = v != 0; pix[i] = v ? v : sous[i]; } });
      int e = r, d = imx(e, (int)lround(s->P.cerneEp/px/e)*e), h = d >> 1;
      #define DEHORS(x, y) ((x) < rx0 || (x) > rx1 || (y) < ry0 || (y) > ry1 || !pc[(y)*gw + (x)])
      PAR_LIGNES((ry0/e)*e, ry1, e, (ry1 - ry0 + 1)*(rx1 - rx0 + 1), y0, {
        int yc = imn(ry1, y0 + (e >> 1)); if(yc < ry0) continue;
        for(int x0 = (rx0/e)*e; x0 <= rx1; x0 += e){
          int xc = imn(rx1, x0 + (e >> 1)); if(xc < rx0 || DEHORS(xc, yc)) continue;
          if(!(DEHORS(xc - d, yc) || DEHORS(xc + d, yc) || DEHORS(xc, yc - d) || DEHORS(xc, yc + d)
             || (h && (DEHORS(xc - h, yc - h) || DEHORS(xc + h, yc + h) || DEHORS(xc + h, yc - h) || DEHORS(xc - h, yc + h))))) continue;
          for(int y = imx(y0, ry0), y1 = imn(ry1, y0 + e - 1); y <= y1; y++)
            for(int x = imx(x0, rx0), x1 = imn(rx1, x0 + e - 1); x <= x1; x++) if(pc[y*gw + x]) pix[y*gw + x] = NOIR;
        }
      });
    }
  }
  s->pix = garde;
}

/* ================= transition entre deux scènes =================
   Une grande forme molle naît en un point, grandit en ondulant jusqu'à couvrir tout l'écran ; son bord est dessiné
   d'une bande rayée. Dedans, la nouvelle scène ; dehors, l'ancienne continue de vivre jusqu'à disparaître. */
static inline double atan2Vite(double y, double x){   /* précision de l'ordre du millième de radian : bien assez pour un bord */
  double ax = fabs(x), ay = fabs(y), a = mn2(ax, ay)/(mx2(ax, ay) + 1e-12), s2 = a*a;
  double r = ((-0.0464964749*s2 + 0.15931422)*s2 - 0.327622764)*s2*a + a;
  if(ay > ax) r = PI/2 - r; if(x < 0) r = PI - r; if(y < 0) r = -r; return r;
}
static void finirTransition(Syn *s){
  if(!s->trans) return;
  s->trans = 0;
  for(int i = s->N - 1; i >= 0; i--) if(s->GEN[i] == s->genAncien) retirer(s, i);
  s->sale = 1;
}
/* le pinceau de la transition : sa trajectoire, calculée d'avance (spirale depuis un point, ou zigzag d'un bord à l'autre),
   légèrement tremblée comme une main ; elle remplit l'écran en trois secondes */
/* le chemin est d'abord parcouru finement (pas de 3 points), puis le pinceau le suit à vitesse constante */
static int ajouterPoint(Syn *s, int n, double x, double y){
  double m = s->rPinceau*.9; if(x < -m || y < -m || x > s->W + m || y > s->H + m) return n;   /* hors de l'écran : le pinceau ne s'y attarde pas */
  if(n >= s->nChemin){ s->nChemin = s->nChemin ? s->nChemin*2 : 8192; s->chemin = realloc(s->chemin, sizeof(float)*2*s->nChemin); }
  s->chemin[2*n] = (float)x; s->chemin[2*n + 1] = (float)y; return n + 1;
}
/* quinze gestes, jamais deux fois les mêmes de suite : hachures, vagues, colonnes, zigzag, spirale, grande spirale depuis le centre,
   anneaux, losanges, rectangle qui tourne, rayons, essuie-glace depuis un coin, boucles d'écriture, double spirale, gribouillis */
enum { G_HACHURES, G_VAGUES, G_COLONNES, G_ZIGZAG, G_SPIRALE, G_SPIRALE_C, G_SPIRALE_C2, G_ANNEAUX, G_LOSANGES, G_CARRE, G_RAYONS, G_COIN, G_BOUCLES, G_DOUBLE, G_GRIBOUILLIS, NGESTES };
static inline double ondeZig(double a){ return (2/PI)*asin(sin(a)); }
static void tracerChemin(Syn *s){
  double W = s->W, H = s->H, r = mx2(W, H)*alea(s, .055, .15), g1 = R01()*9, g2 = R01()*9; int n = 0;
  s->rPinceau = r;
  int libres[NGESTES], nl = 0; for(int g = 0; g < NGESTES; g++){ int vu = 0; for(int k = 0; k < 4; k++) if(s->derniers[k] == g + 1) vu = 1; if(!vu) libres[nl++] = g; }
  int sorte = libres[(int)(R01()*nl)]; for(int k = 3; k > 0; k--) s->derniers[k] = s->derniers[k - 1]; s->derniers[0] = sorte + 1;
  if(sorte == G_SPIRALE_C2) sorte = G_SPIRALE_C;
  double D = hypot(W, H)/2 + r, cx0 = W/2, cy0 = H/2;
  #define AJ(x, y) (n = ajouterPoint(s, n, (x), (y)))
  if(sorte <= G_ZIGZAG){
    double ecart = r*(sorte == G_VAGUES ? 1.5 : 1.7), th = sorte == G_COLONNES ? PI/2 + alea(s, -.08, .08) : alea(s, .15, .7)*(R01() < .5 ? 1 : -1);
    double ct = cos(th), st = sin(th), amp = sorte == G_VAGUES ? r*alea(s, .6, 1.2) : sorte == G_ZIGZAG ? r*alea(s, .8, 1.3) : 0, lam = r*alea(s, 2.5, 4.5);
    double v = -D; int sens = 1;
    while(v <= D + ecart){
      double a0 = -D*sens, a1 = D*sens;
      for(double u = 0; u <= 1; u += 3/(2*D)){
        double a = a0 + (a1 - a0)*u, o = sorte == G_ZIGZAG ? ondeZig(a/lam + g2) : sin(a/lam + g2), b = v + ecart*.22*(bruit(a*.006 + v*.01 + g1) - .5) + amp*o;
        AJ(cx0 + a*ct - b*st, cy0 + a*st + b*ct);
      }
      double o1 = sorte == G_ZIGZAG ? ondeZig(a1/lam + g2) : sin(a1/lam + g2);
      for(double k = 0; k <= PI; k += 3/(ecart*.5)){ double a = a1 + sens*sin(k)*ecart*.5, b = v + ecart*.5 - cos(k)*ecart*.5 + amp*o1; AJ(cx0 + a*ct - b*st, cy0 + a*st + b*ct); }
      v += ecart; sens = -sens;
    }
  }else if(sorte == G_SPIRALE_C || sorte == G_DOUBLE){
    double ecart = r*1.6, cs[2][2]; int nc = sorte == G_DOUBLE ? 2 : 1;
    if(nc == 2){ cs[0][0] = W*.32; cs[0][1] = H*.5; cs[1][0] = W*.68; cs[1][1] = H*.5; } else { cs[0][0] = W/2 + alea(s, -.03, .03)*W; cs[0][1] = H/2 + alea(s, -.03, .03)*H; }
    double sens = R01() < .5 ? 1 : -1, ph = R01()*PI*2, a = ecart/(2*PI), th[2] = {0, 0}, fin[2];
    for(int k = 0; k < nc; k++){ double cx = cs[k][0], cy = cs[k][1]; fin[k] = (mx2(mx2(hypot(cx, cy), hypot(W - cx, cy)), mx2(hypot(cx, H - cy), hypot(W - cx, H - cy))) + r)/a; }
    int actifs = nc;
    while(actifs > 0){ actifs = 0;
      for(int k = 0; k < nc; k++){ if(th[k] >= fin[k]) continue; actifs++; double lim = mn2(fin[k], th[k] + PI*2);
        while(th[k] < lim){ double rho = a*th[k] + ecart*.25*(bruit(th[k]*.7 + g2 + k*5) - .5), an = sens*(k ? -1 : 1)*th[k] + ph; AJ(cs[k][0] + rho*cos(an), cs[k][1] + rho*sin(an)); th[k] += 3/mx2(10, a*th[k]); } } }
  }else if(sorte == G_LOSANGES){
    double ecart = r*1.5, coin = (W + H)/2 + r, R = r*.4;
    while(R < coin){ for(double k = 0; k <= 4.05; k += 3/mx2(10, R*1.4)){ int q = (int)floor(k) % 4; double f = k - floor(k);
        double A[4][2] = {{R, 0}, {0, R}, {-R, 0}, {0, -R}}; double *P0 = A[q], *P1 = A[(q + 1) % 4];
        AJ(W/2 + P0[0] + (P1[0] - P0[0])*f + r*.1*(bruit(k*3 + R*.01 + g1) - .5), H/2 + P0[1] + (P1[1] - P0[1])*f); } R += ecart; }
  }else if(sorte == G_RAYONS){
    double R = hypot(W, H)/2 + r, ph = R01()*PI*2, sens = R01() < .5 ? 1 : -1; int nr = (int)ceil(2*PI*R/(r*1.5));
    for(int k = 0; k < nr; k++){ double an = ph + sens*k*2*PI/nr; for(double d = 0; d <= R; d += 3) AJ(W/2 + cos(an)*d, H/2 + sin(an)*d);
      for(double d = R; d >= 0; d -= 6) AJ(W/2 + cos(an + PI/nr)*d, H/2 + sin(an + PI/nr)*d); }
  }else if(sorte == G_COIN){
    double cxs = R01() < .5 ? 0 : W, cys = R01() < .5 ? 0 : H, ecart = r*1.6, Rmax = hypot(W, H) + r;
    double a0 = atan2(H/2 - cys, W/2 - cxs) - PI/2*1.1, a1 = a0 + PI*1.1, R = r*.3; int sens = 1;
    while(R < Rmax){ double st = 3/mx2(10, R); if(sens > 0) for(double an = a0; an <= a1; an += st) AJ(cxs + cos(an)*R, cys + sin(an)*R); else for(double an = a1; an >= a0; an -= st) AJ(cxs + cos(an)*R, cys + sin(an)*R); R += ecart; sens = -sens; }
  }else if(sorte == G_BOUCLES){
    double ecart = r*1.55, R = r*1.05, y = -r*.3; int sens = 1;
    while(y < H + r){ double x0 = sens > 0 ? -r : W + r, x1 = sens > 0 ? W + r : -r, L = fabs(x1 - x0);
      for(double t = 0; t <= L; t += 2){ double ph = t/(R*.9); AJ(x0 + sens*t + sens*R*.8*cos(ph + PI), y + R*sin(ph)*(sens > 0 ? -1 : 1)); }
      y += ecart; sens = -sens; }
  }else if(sorte == G_SPIRALE){
    double ecart = r*1.65, cx = alea(s, .3, .7)*W, cy = alea(s, .3, .7)*H, ov = alea(s, .75, 1.3), ph = R01()*PI*2, sens = R01() < .5 ? 1 : -1;
    double coin = mx2(mx2(hypot(cx, cy), hypot(W - cx, cy)), mx2(hypot(cx, H - cy), hypot(W - cx, H - cy)))*mx2(ov, 1/ov) + r, a = ecart/(2*PI), th = 0;
    while(a*th < coin){ double rho = a*th + ecart*.3*(bruit(th*.7 + g2) - .5); AJ(cx + rho*cos(sens*th + ph)*ov, cy + rho*sin(sens*th + ph)/ov); th += 3/mx2(10, a*th); }
  }else if(sorte == G_ANNEAUX){
    double ecart = r*1.6, coin = hypot(W, H)/2 + r, R = r*.4;
    while(R < coin){ double ph = R01()*PI*2; for(double a = 0; a <= PI*2 + .3; a += 3/mx2(10, R)){ double rr = R + ecart*.15*(bruit(a*2 + R*.01 + g1) - .5); AJ(cx0 + rr*cos(a + ph), cy0 + rr*sin(a + ph)); } R += ecart; }
  }else if(sorte == G_CARRE){
    double ecart = r*1.6, x0 = -r*.2, y0 = -r*.2, x1 = W + r*.2, y1 = H + r*.2;
    #define LIGNE(ax, ay, bx, by) do{ double l_ = hypot((bx) - (ax), (by) - (ay)); for(double u = 0; u <= 1; u += 3/mx2(1, l_)) AJ((ax) + ((bx) - (ax))*u + r*.12*(bruit(u*6 + g1) - .5), (ay) + ((by) - (ay))*u + r*.12*(bruit(u*6 + g2) - .5)); }while(0)
    while(x1 - x0 > -ecart && y1 - y0 > -ecart){ LIGNE(x0, y0, x1, y0); LIGNE(x1, y0, x1, y1); LIGNE(x1, y1, x0, y1); LIGNE(x0, y1, x0, y0 + ecart);
      x0 += ecart; y0 += ecart; x1 -= ecart; y1 -= ecart; if(x1 < x0 && y1 < y0) break; }
    #undef LIGNE
  }else{
    /* gribouillis : la main va et vient, toujours vers un coin encore blanc, jusqu'à avoir tout couvert */
    double cs = r*.9; int nx = (int)ceil(W/cs), ny = (int)ceil(H/cs), reste = nx*ny; uint8_t *vu = calloc(nx*ny, 1);
    double x = alea(s, .2, .8)*W, y = alea(s, .2, .8)*H, a = R01()*PI*2, Lmax = W*H/r*4; int cible = -1;
    for(double d = 0; d < Lmax && reste > 0; d += 3){
      if(cible < 0 || vu[cible]){ int best = -1; double bd = 1e18; for(int q = 0; q < nx*ny; q++) if(!vu[q]){ double dd = hypot((q % nx + .5)*cs - x, (q/nx + .5)*cs - y)*(.7 + .6*R01()); if(dd < bd){ bd = dd; best = q; } } cible = best; if(best < 0) break; }
      double tx = (cible % nx + .5)*cs, ty = (cible/nx + .5)*cs, voulu = atan2(ty - y, tx - x), da = voulu - a;
      while(da > PI) da -= PI*2; while(da < -PI) da += PI*2;
      a += da*.06 + (bruit(d*.004 + g1) - .5)*.35; x = borne(x + cos(a)*3, 0, W); y = borne(y + sin(a)*3, 0, H); AJ(x, y);
      if(((int)d) % 9 == 0){ int i0 = (int)(x/cs), j0 = (int)(y/cs);
        for(int j = j0 - 1; j <= j0 + 1; j++) for(int i = i0 - 1; i <= i0 + 1; i++){ if(i < 0 || j < 0 || i >= nx || j >= ny) continue; int q = j*nx + i;
          if(!vu[q] && hypot((i + .5)*cs - x, (j + .5)*cs - y) < r*1.05){ vu[q] = 1; reste--; } } }
    }
    free(vu);
  }
  #undef AJ
  s->nPoints = n; s->iChemin = 0; s->pasChemin = imx(1, (int)(r*.2/3));
}
static void lancerTransition(Syn *s){
  finirTransition(s);
  size_t n = (size_t)s->gw*s->gh;
  s->fondAncien = realloc(s->fondAncien, n*4); s->pixAncien = realloc(s->pixAncien, n*4);
  s->masque = realloc(s->masque, n*2); memset(s->masque, 0, n*2); s->imTrans = 0;
  memcpy(s->fondAncien, s->fondPix, n*4);
  s->genAncien = s->gen; s->gen = (s->gen + 1) & 255;
  /* la durée suit le tempo de la scène : quatre, six ou huit temps (entre 2,5 et 7 secondes) ; en Hardcore, quatre temps, très vite */
  { double tempo = s->son ? s->son->tempo : 110, temps = 60000/mx2(40, tempo*(s->P.vitesse > 0 ? s->P.vitesse : 1)); static const int NT[4] = {4, 6, 8, 8};
    s->tempsT = temps; s->nbTemps = NT[(int)(R01()*4)];
    s->dureeTrans = s->son && s->son->hardcore ? (s->nbTemps = 4, borne(temps*4, 900, 2000)) : borne(temps*s->nbTemps, 2500, 7000); }
  s->trans = 1; s->tTrans0 = s->dernier;
  tracerChemin(s);
  s->bandeT = mx2(s->PX0*2, 16);
  int c = (int)(R01()*4);
  s->bordA = c == 0 ? VERT : c == 1 ? hexc(0xff3ea5) : c == 2 ? hexc(0xffe21f) : BLANC; s->bordB = c == 3 ? VERT : NOIR;
  Traceur *A = &s->automate; A->forme = NULL; A->etat = E_REPOS; A->n = (int)alea(s, 10, 30); A->trav = 0;
}
/* un coup de pinceau : le disque se remplit de la nouvelle scène, une couronne garde le trait du bord */
static void tamponner(Syn *s, double x, double y, double r){
  int gw = s->gw, gh = s->gh; double px = s->PX0, rb = r + s->bandeT, r2 = r*r, rb2 = rb*rb;
  int x0 = imx(0, (int)((x - rb)/px)), x1 = imn(gw - 1, (int)((x + rb)/px)), y0 = imx(0, (int)((y - rb)/px)), y1 = imn(gh - 1, (int)((y + rb)/px));
  for(int gy = y0; gy <= y1; gy++){
    double dy = (gy + .5)*px - y; uint16_t *m = s->masque + gy*gw; uint16_t date = (uint16_t)(s->imTrans + 2);
    for(int gx = x0; gx <= x1; gx++){ double dx = (gx + .5)*px - x, d2 = dx*dx + dy*dy;
      if(d2 < r2){ if(m[gx] < 2) m[gx] = date; } else if(d2 < rb2 && m[gx] < 1) m[gx] = 1; }
  }
}
static void composer(Syn *s){
  /* l'avancée : le temps, mais jamais plus vite qu'au rythme des images (une machine lente voit tout le dessin) */
  double t = mn2((s->dernier - s->tTrans0)/s->dureeTrans, (s->imTrans + 1)/60.0);
  if(t >= 1){ finirTransition(s); return; }
  /* le pinceau avance au rythme de la musique : un élan à chaque temps, puis il ralentit avant le suivant */
  double td = mn2(1, t/.86), bt = td*s->nbTemps; int k0 = (int)floor(bt); double fr = bt - k0, u = borne((k0 + (1 - pow(1 - fr, 2.2)))/s->nbTemps, 0, 1);
  s->imTrans++;
  int fin = imn(s->nPoints - 1, (int)(borne(u, 0, 1)*(s->nPoints - 1)));
  int k = s->iChemin;
  for(; k <= fin; k += s->pasChemin){
    double r = s->rPinceau*(1 + .12*sin(k*.021) + .06*sin(k*.067));   /* la pression du pinceau varie */
    tamponner(s, s->chemin[2*k], s->chemin[2*k + 1], r);
  }
  s->iChemin = k;
  int gw = s->gw, gh = s->gh; double px = s->PX0, tt = (s->dernier - s->tTrans0)*.001;
  uint32_t *nouv = s->pix, *anc = s->pixAncien; uint16_t *m = s->masque;
  int pas = imx(1, (int)(8/px)), revele = 22, ima = s->imTrans + 2;   /* la nouvelle scène paraît 22 images derrière le pinceau */
  PAR_LIGNES(0, gh - 1, 1, gw*gh, gy, for(int gx = 0; gx < gw; gx++){
    int q = gy*gw + gx, v = m[q];
    if(v == 0){ if(t <= .86 || h1(q*.7311 + 3.1) > (t - .86)/.12) nouv[q] = anc[q]; continue; }   /* pas encore touché : l'ancienne scène ; à la fin, ce qui reste se dissout */
    if(v >= 2 && ima - v >= revele) continue;                   /* peint depuis un moment : la nouvelle scène */
    if(v >= 2) nouv[q] = ((((gx - gy)/pas) + (int)(tt*4)) & 1) ? s->bordA : s->bordB;   /* la peinture fraîche du pinceau */
    else nouv[q] = ((((gx + gy)/pas) + (int)(tt*6)) & 1) ? s->bordB : s->bordA;        /* son bord */
  });
}

/* ================= l'heure en lettres pixel ================= */
static const char *CHIFFRES[11][8] = {
  {".####.","##..##","##.###","##.###","###.##","###.##","##..##",".####."}, {"..##..",".###..","####..","..##..","..##..","..##..","..##..","######"},
  {".####.","##..##","....##","...##.","..##..",".##...","##....","######"}, {"#####.","....##","....##",".####.","....##","....##","....##","#####."},
  {"...##.","..###.",".####.","##.##.","######","...##.","...##.","...##."}, {"######","##....","##....","#####.","....##","....##","##..##",".####."},
  {".####.","##....","##....","#####.","##..##","##..##","##..##",".####."}, {"######","....##","...##.","..##..","..##..","..##..","..##..","..##.."},
  {".####.","##..##","##..##",".####.","##..##","##..##","##..##",".####."}, {".####.","##..##","##..##",".#####","....##","....##","....##",".####."},
  {"..","##","##","..","..","##","##",".."} };
static void horlogeDemarrer(Syn *s){
  typeof(s->horloge) *H = &s->horloge;
  time_t now = time(NULL); struct tm lt; localtime_r(&now, &lt);
  char txt[8]; if(R01() < .7) snprintf(txt, sizeof txt, "%02d:%02d", lt.tm_hour, lt.tm_min); else snprintf(txt, sizeof txt, "%02d", lt.tm_min);
  int cols = 0; for(char *p = txt; *p; p++){ int g = *p == ':' ? 10 : *p - '0'; cols += (int)strlen(CHIFFRES[g][0]) + 1; } cols--;
  /* tout en points (pas en cases) : la taille des pixels de la toile peut changer pendant que l'heure est là */
  int k = imx(4, (int)fmin(s->W*.8/cols, s->H*.55/8)); k = imx(1, k/4)*4;
  H->k = k; H->nCols = cols; H->x0 = (int)((s->W - cols*k)/2 + (R01() - .5)*s->W*.1); H->y0 = (int)((s->H - 8*k)/2 + (R01() - .5)*s->H*.2);
  H->n = 0; int cx = 0;
  for(char *p = txt; *p; p++){ int g = *p == ':' ? 10 : *p - '0'; int w = (int)strlen(CHIFFRES[g][0]);
    for(int y = 0; y < 8; y++) for(int x = 0; x < w; x++) if(CHIFFRES[g][y][x] == '#' && H->n < 400){
      H->c[H->n].gx = cx + x; H->c[H->n].gy = y; H->c[H->n].col = cx + x; H->c[H->n].dy = 0; H->c[H->n].vy = 0; H->c[H->n].vivant = 1;
      H->c[H->n].delai = (cx + x)*70 + R01()*120; H->c[H->n].mort = R01()*1600; H->n++; }
    cx += w + 1; }
  const Palette *pl = &PALS[s->P.palette]; uint32_t fond = s->P.fond == FOND_NOIR ? NOIR : BLANC;
  uint32_t l[24]; int nl = 0; for(int i = 0; i < pl->n; i++) if(pl->u[i] != fond && pl->u[i] != SPECTRE) l[nl++] = pl->u[i];
  if(!nl){ l[0] = VERT; l[1] = BLANC; nl = 2; }
  H->ca = l[(int)(R01()*nl)]; H->cb = nl > 1 ? l[(int)(R01()*nl)] : (fond == NOIR ? BLANC : NOIR); if(H->cb == H->ca) H->cb = fond == NOIR ? BLANC : NOIR;
  if(pl->bit || pl->ascii){ H->ca = fond == NOIR ? BLANC : NOIR; H->cb = fond == NOIR ? NOIR : BLANC; }   /* en 1 bit : noir et blanc purs */
  H->motif = (int)(R01()*5); H->mode = R01() < .6 ? 0 : 1; H->etat = 0; H->t0 = s->dernier; H->tenue = alea(s, 9000, 15000); H->actif = 1;
}
static void horlogeAnimer(Syn *s){
  typeof(s->horloge) *H = &s->horloge;
  if(!H->actif){
    if(!s->hcEpisode && !s->trans && s->dernier >= H->prochaine && H->prochaine > 0) horlogeDemarrer(s);
    return;
  }
  double t = s->dernier - H->t0;
  if(H->etat == 0 && t > 1400) H->etat = 1;
  if(H->etat == 1 && (t > 1400 + H->tenue || s->trans)){ H->etat = 2; H->t0 = s->dernier; t = 0; }
  if(H->etat == 2){
    int vivants = 0; double g = .0016*H->k;
    for(int i = 0; i < H->n; i++){ if(!H->c[i].vivant) continue;
      if(H->mode == 0){ if(t > H->c[i].delai){ H->c[i].vy += g*16.6; H->c[i].dy += H->c[i].vy; } if(H->y0 + H->c[i].gy*H->k + H->c[i].dy > s->H + 2) H->c[i].vivant = 0; }
      else if(t > H->c[i].mort) H->c[i].vivant = 0;
      vivants += H->c[i].vivant; }
    if(!vivants){ H->actif = 0; H->prochaine = s->dernier + alea(s, 20000, 45000); }
  }
}
/* l'heure peinte dans le fond de la scène : les dessins qui viennent passent par-dessus */
static const uint32_t *horlogeFond(Syn *s){
  typeof(s->horloge) *H = &s->horloge; size_t n = (size_t)s->gw*s->gh;
  if(H->nFond != n){ free(H->fond); H->fond = malloc(n*4); H->nFond = n; }
  memcpy(H->fond, s->fondPix, n*4);
  double t = s->dernier - H->t0; int px = s->PX0, k = imx(1, (int)lround((double)H->k/px)), gw = s->gw, gh = s->gh;
  for(int i = 0; i < H->n; i++){
    if(!H->c[i].vivant) continue;
    if(H->etat == 0 && H->c[i].col > t/1400*H->nCols) continue;   /* elle apparaît colonne après colonne */
    int X = (int)lround((H->x0 + H->c[i].gx*(double)H->k)/px), Y = (int)lround((H->y0 + H->c[i].gy*(double)H->k + H->c[i].dy)/px);
    for(int y = 0; y < k; y++){ int gy = Y + y; if(gy < 0 || gy >= gh) continue;
      for(int x = 0; x < k; x++){ int gx = X + x; if(gx < 0 || gx >= gw) continue;
        int u = gx, v = gy, b;
        switch(H->motif){ case 0: b = 1; break; case 1: b = ((u + v)/imx(1, k/3)) & 1; break; case 2: b = ((u/imx(1, k/2)) + (v/imx(1, k/2))) & 1; break;
                          case 3: b = (v/imx(1, k/3)) & 1; break; default: b = (x == 0 || y == 0 || x == k - 1 || y == k - 1) ? 0 : 1; }
        H->fond[gy*gw + gx] = b ? H->ca : H->cb; } }
  }
  return H->fond;
}
/* Hardcore, à l'image : chaque coup de la batterie a son effet, lu dans la même partition que le son, au pas qui sonne.
   La grosse caisse éclaire, inverse ou zoome ; la basse découpe en tranches ; le clap fait sauter l'image en arrière ou la
   retourne ; le roulement stroboscope ; la rupture plonge dans le noir ; la montée s'emballe ; la chute éclate en blanc. */
/* le temps du son qui sort en ce moment : ce qui est calculé, moins ce qui attend encore dans l'anneau */
static inline double tJoue(Son *so){ long lu = atomic_load(&so->lu), ec = atomic_load(&so->ecrit); return (so->nEch - (ec - lu))/so->sr; }
enum { EF_AUCUN, EF_FLASH, EF_NOIR, EF_INVERSE, EF_SAUTE, EF_ZOOM, EF_MIROIR, EF_TRANCHES, EF_SCINTILLE };
static void hardcoreImage(Syn *s){
  Son *so = s->son; HC *h = so->hc; int gw = s->gw, gh = s->gh, n = gw*gh; double fr;
  double tj = tJoue(so) - s->latence; int st = hcPasJoue(so, tj, &fr); if(st < 0) return;
  s->sale = 1;
  if((size_t)n != s->hcTaille){ for(int k = 0; k < 6; k++){ free(s->hcSouv[k]); s->hcSouv[k] = NULL; } s->hcNSouv = 0; s->hcTaille = n; }
  if(st != s->hcDernierPas){
    s->hcDernierPas = st;
    double u = h1(st*1.7 + 3.3); int ef;
    if(h->jungle) ef = jgEffet(h, st, u);   /* make me dance : l'explosion éclaire, le silence noircit, le reste rarement */
    else { PasHC e = hcSeq(st, h->preset, h->graine);
    if(e.impact) ef = EF_FLASH; else if(e.pause) ef = EF_NOIR;
    else if(e.montee && e.caisse) ef = e.m >= 6 ? ((st & 1) ? EF_INVERSE : EF_FLASH) : e.m >= 4 ? EF_SAUTE : EF_MIROIR;
    else if(e.section == HS_FOND && !e.roll && e.p % 8 != 0 && u < .55) ef = EF_AUCUN;
    else if(e.roll) ef = e.sorte == HM_BEGAIE ? EF_SAUTE : (st & 1) ? EF_INVERSE : EF_FLASH;
    else if(e.rupture) ef = e.caisse ? (e.rat > 2 ? EF_INVERSE : EF_FLASH) : e.p < 12 ? (e.p % 4 == 0 ? EF_SCINTILLE : EF_AUCUN) : EF_NOIR;
    else if(e.kick){ static const int K4[4] = { EF_FLASH, EF_INVERSE, EF_ZOOM, EF_ZOOM }; ef = e.sorte == HM_GALOP ? ((st & 1) ? EF_MIROIR : EF_ZOOM) : e.sorte == HM_CASSE ? EF_TRANCHES : K4[(int)(u*4)]; }
    else if(e.caisse) ef = e.rat > 1 ? EF_INVERSE : u < .5 ? EF_SAUTE : EF_MIROIR;
    else if(e.basse) ef = u < .6 ? EF_TRANCHES : EF_SCINTILLE;
    else if(e.cri) ef = EF_ZOOM; else ef = EF_AUCUN; }
    s->hcEffet = ef; static const double Z[3] = {1.5, 2, 3}; s->hcZ = Z[(int)(h1(st*.9)*3)]; s->hcZx = h1(st*2.1)*gw; s->hcZy = h1(st*3.7)*gh;
    if(st % 2 == 0){ int k = s->hcISouv; if(!s->hcSouv[k]) s->hcSouv[k] = malloc(n*4); memcpy(s->hcSouv[k], s->pix, n*4); s->hcISouv = (k + 1) % 6; if(s->hcNSouv < 6) s->hcNSouv++; }
  }
  int ef = s->hcEffet; if(ef == EF_AUCUN) return;
  if(fr > (ef == EF_FLASH || ef == EF_INVERSE ? .5 : .85)) return;   /* l'éclair ne dure qu'une moitié de pas */
  uint32_t *pix = s->pix;
  if(ef == EF_FLASH){ const Palette *pl = &PALS[s->P.palette]; uint32_t c = h1(st*5.1) < .5 || !pl->n ? BLANC : pl->u[(int)(h1(st*7.7)*imn(3, pl->n))]; for(int i = 0; i < n; i++) pix[i] = c; return; }
  if(ef == EF_NOIR){ for(int i = 0; i < n; i++) pix[i] = NOIR; return; }
  if(ef == EF_INVERSE){ for(int i = 0; i < n; i++) pix[i] ^= 0x00FFFFFFu; return; }
  if(ef == EF_SAUTE){ if(s->hcNSouv){ uint32_t *q = s->hcSouv[(int)(h1(st*1.3)*s->hcNSouv)]; if(q) memcpy(pix, q, n*4); } return; }
  uint32_t *src = s->pixAncien && s->trans ? NULL : NULL; (void)src;
  static uint32_t *copie = NULL; static size_t nCopie = 0; if(nCopie != (size_t)n){ free(copie); copie = malloc(n*4); nCopie = n; }
  memcpy(copie, pix, n*4);
  if(ef == EF_ZOOM){ double z = s->hcZ, cx = s->hcZx, cy = s->hcZy;
    for(int y = 0; y < gh; y++){ int sy = imn(gh - 1, imx(0, (int)floor(cy + (y - cy)/z))); const uint32_t *o = copie + sy*gw; uint32_t *d = pix + y*gw;
      for(int x = 0; x < gw; x++) d[x] = o[imn(gw - 1, imx(0, (int)floor(cx + (x - cx)/z)))]; } return; }
  if(ef == EF_MIROIR){ for(int y = 0; y < gh; y++){ const uint32_t *o = copie + y*gw; uint32_t *d = pix + y*gw; for(int x = 0; x < gw; x++) d[x] = o[gw - 1 - x]; } return; }
  { int hb = imx(2, (int)(gh/(8 + h1(st)*24))); double now = s->dernier;
    for(int y = 0; y < gh; y++){
      int b = y/hb; double r = h1(b*3.1 + st*.77 + (ef == EF_SCINTILLE ? now*.01 : 0)); const uint32_t *o = copie + y*gw; uint32_t *d = pix + y*gw;
      if(ef == EF_SCINTILLE){ if(r < .3) for(int x = 0; x < gw; x++) d[x] = o[x] ^ 0x00FFFFFFu; continue; }
      int dx = (int)((r - .5)*gw*.5);
      for(int x = 0; x < gw; x++){ int sx = ((x - dx) % gw + gw) % gw; d[x] = o[sx]; } } }
}
static void rendre(Syn *s){
  int px = s->PX0, gw = s->gw, gh = s->gh;
  if(s->aTrame && s->P.vitesse > 0){
    int cases = px == 1 ? gw*gh/4 : gw*gh;
    int saut = (cases > 120000 ? 3 : cases > 50000 ? 2 : 1)*(s->qualite < .7 ? 2 : 1);
    if(s->imagesFond++ % saut == 0) peindreFond(s);
  }
  if(s->trans){ rendreGen(s, s->pixAncien, s->fondAncien, s->genAncien); rendreGen(s, s->pix, s->fondPix, s->gen); composer(s); }
  else rendreGen(s, s->pix, s->horloge.actif ? horlogeFond(s) : s->fondPix, s->gen);
  if(s->son && s->son->hardcore) hardcoreImage(s);
}

/* ================= veille : un nouveau tableau toutes les minutes environ ================= */
static double choix(Syn *s, const double *l, int n){ return l[(int)(R01()*n)]; }
static void nouveauTableau(Syn *s, int premier){
  static const double vit[] = {.7,.85,1,1,1.2,1.5}, sech[] = {0,.2,.3,.5}, grav[] = {.4,.8,1,1,1.4}, incl[] = {0,0,0,-12,12}, vitHC[] = {1.4,1.6,1.8,2};
  Reglages *P = &s->P; Son *so = s->son; int hc = so && so->hardcore;
  P->vitesse = hc ? choix(s, vitHC, 4) : choix(s, vit, 6);
  P->mollesse = hc ? alea(s, .2, .5) : alea(s, .25, .8); P->viscosite = alea(s, .15, .6); P->sechage = choix(s, sech, 4); P->contact = alea(s, .1, .6);
  P->gravite = choix(s, grav, 5); P->inclinaison = choix(s, incl, 5);
  P->cerne = 0;
  P->sol = R01() < .3;
  /* l'économiseur : jamais le fond blanc. Le noir le plus souvent ; sinon un fond animé en noir et blanc :
     la trame, ou l'art optique (diagonales, anneaux qui respirent, palette de cibles). En Hardcore, l'optique domine. */
  double r = R01(); int fond;
  if(premier) fond = FOND_NOIR;
  else if(hc) fond = r < .35 ? FOND_NOIR : r < .5 ? FOND_TRAME : r < .67 ? FOND_DIAG : r < .84 ? FOND_CERCLES : FOND_RONDS;
  else fond = r < .62 ? FOND_NOIR : r < .76 ? FOND_TRAME : r < .84 ? FOND_DIAG : r < .92 ? FOND_CERCLES : FOND_RONDS;
  if(!premier) lancerTransition(s);             /* chaque tableau arrive par un grand geste de pinceau qui l'apporte */
  if(fond != P->fond || FOND_ANIME(fond)){ P->fond = fond; s->graineFond = (int)(R01()*1e9); if(!premier) construireFond(s); }
  s->sale = 1;
  static const int TON[12] = { PAL_CHARTE, PAL_ACIDE, PAL_VAGUE, PAL_ASCII, PAL_ACIDE, PAL_VAGUE, PAL_CHARTE, PAL_ARC, PAL_ARC, PAL_VAGUE, PAL_VAGUE, PAL_BIT };
  if(hc){
    static const int PH[5] = { PAL_ACIDE, PAL_ARC, PAL_BIT, PAL_CHARTE, PAL_ACIDE };
    P->palette = PH[(int)(R01()*5)];
    static const int TR[6] = { 0, 0, 2, -2, 5, -5 };
    if(so->hc->jungle) so->tempo = 120;   /* la jungle : 120 bpm ; ses montées accélèrent, ses mix glissent */
    else { so->hc->transpoDemi = TR[(int)(R01()*6)];
    so->tempo = so->tempo < 140 ? 165 : borne(so->tempo + (int)(R01()*5 - 2)*3, 152, 180); }   /* comme un DJ : quelques bpm d'une scène à l'autre */
    accorder(s); sonTempo(s); espace(s);
    return;
  }
  /* chaque tableau change de musique, dans l'ordre des douze styles ; les couleurs suivent, et l'orchestre est tiré
     parmi ceux qui conviennent au style, jamais deux fois de suite le même */
  s->horloge.prochaine = R01() < .45 ? s->dernier + alea(s, 3000, 9000) : 0;   /* un tableau sur deux environ donne l'heure */
  s->scene = premier ? (int)(R01()*12) : (s->scene + 1) % 12;
  P->palette = TON[s->scene];
  if(so){
    so->style = ORDRE_VEILLE[s->scene]; so->tempo = STYLES[so->style].bpm;
    { const Style *st = &STYLES[so->style]; int l[6], n = 0; for(int k = 0; k < st->nOrch; k++) if(st->orch[k] != so->orch) l[n++] = st->orch[k];
      so->orch = n ? l[(int)(R01()*n)] : OR_SYNTHE; }
    so->dynamique = R01() < .5;
    if(!premier) passer(s); else accorder(s);
    sonTempo(s); espace(s);
  }
}
/* un épisode Hardcore : une ou deux fois l'arc de pression entier (environ 45 s chacun) ; l'image change de scène
   à chaque chute et au début de chaque arc ; la musique ne s'arrête jamais, le tempo ne bouge que de quelques bpm */
static void entrerHardcore(Syn *s){
  Son *so = s->son; if(!so) return;
  so->hardcore = 1; so->hcDebut = so->pasTotal; so->tempo = 100;   /* < 140 : la scène prendra 165 bpm */
  HC *h = so->hc; h->preset = (int)(R01()*NHP); h->graine = R01()*999; h->nPas = 0; memset(h->tPas, 0, sizeof h->tPas);
  s->hcEpisode = 1; s->hcArcs = 1 + (R01() < .45); s->hcBarVue = 0; s->hcDernierPas = -1;
  if(h->jungle){ h->transpoDemi = 0; h->jg->finPiste = s->hcArcs; h->jg->subF = 0; so->tempo = 120; }   /* make me dance : un ou deux morceaux, mixés */
  nouveauTableau(s, 0);
}
static void suivreHardcore(Syn *s, double t){
  Son *so = s->son; double fr; int st = hcPasJoue(so, tJoue(so) - s->latence, &fr);
  if(st < 0) return;
  if(so->hc->jungle){   /* la jungle : l'image change au début d'un morceau, à l'échange des basses, à l'explosion d'une chute */
    if(st == s->hcBarVue) return; int avant = s->hcBarVue; s->hcBarVue = st;
    if(st >= so->hc->jg->finPiste*JG_PER + JG_MIX){ so->hardcore = 0; s->hcEpisode = 0; nouveauTableau(s, 0); s->prochainTableau = t + alea(s, 40, 85)*1000; return; }
    for(int q = avant + 1; q <= st; q++) if(q > 0 && jgMoment(so->hc, q)){ nouveauTableau(s, 0); break; }
    return; }
  int bar = st/16, pos = bar % 32;
  if(bar == s->hcBarVue) return;
  s->hcBarVue = bar;
  if(pos == 0 && bar > 0){
    if(--s->hcArcs <= 0){ so->hardcore = 0; s->hcEpisode = 0; nouveauTableau(s, 0); s->prochainTableau = t + alea(s, 40, 85)*1000; }
    else nouveauTableau(s, 0);
  }else if(pos == 24) nouveauTableau(s, 0);   /* la chute : une nouvelle image, d'un coup de pinceau très rapide */
}

/* ================= boucle ================= */
static double horloge(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec*1e3 + t.tv_nsec/1e6; }
static void regler(Syn *s){
#ifdef SYN_FIGE
  return;   /* essais : pas d adaptation à la vitesse, pour des images reproductibles */
#endif
  if(++s->compteReglage < 20) return;
  s->compteReglage = 0;
  double limite = mx2(16.6, mn2(34, s->periode))*.72;
  if(s->cout > limite && s->coutPas > .3*s->cout && s->qualite > .35) s->qualite = mx2(.35, s->qualite - .06);
  else if(s->cout < limite*.6 && s->qualite < 1) s->qualite = mn2(1, s->qualite + .02);
  /* puissance de la machine : si une image coûte trop cher, le plus petit pixel grossit (2, 4, 6, 10) ;
     si elle respire largement, il redescend. Quelques secondes entre deux décisions. */
  if(++s->compteMachine < 8) return;
  s->compteMachine = 0;
  static const int PAS[4] = {1, 2, 4, 6}; int k = 0; while(k < 3 && PAS[k] != s->pxMin) k++;
  if(s->cout > limite*.8 && k < 3) s->pxMin = PAS[k + 1];
  else if(s->cout < limite*.3 && k > 0) s->pxMin = PAS[k - 1];
}

int syn_image(Syn *s, double t){
  if(s->premiere){ s->premiere = 0; s->dernier = t; s->prochainTableau = t + alea(s, 40, 85)*1000; }
  double dt = mn2(60, t - s->dernier); s->dernier = t; if(dt < 0) dt = 0;
  s->periode += (dt - s->periode)*.05;
  if(s->hcEpisode && s->son) suivreHardcore(s, t);
  else if(t >= s->prochainTableau){ if(s->son && R01() < s->partHC) entrerHardcore(s); else nouveauTableau(s, 0); s->prochainTableau = t + alea(s, 40, 85)*1000; }
  if(++s->compteGrille >= 45){ s->compteGrille = 0; accorderGrille(s); }
  int n = 0; double t0 = horloge();
  s->reste += dt;
  int maxPas = s->coutPas > 6 ? 1 : 2;
  while(s->reste >= 16.6 && n < maxPas){ animerAutomate(s); pas(s); horlogeAnimer(s); s->reste -= 16.6; n++; }
  if(s->horloge.actif) s->sale = 1;
  if(s->reste > 16.6) s->reste = 0;
  double t1 = horloge();
  int change = 0;
  s->tAscii = t;
  if(n || s->sale){ s->sale = 0; rendre(s); change = 1; }
  if(n){ double t2 = horloge(); s->coutPas += ((t1 - t0)/n - s->coutPas)*.1; s->cout += ((t2 - t0) - s->cout)*.1; regler(s); }
  if(n){ rebondsSon(s); }
  if(s->son){ if(n) physiqueSon(s); sonAvancer(s); }
  return change;
}

Syn *syn_creer(double largeur, double hauteur, uint64_t graine){
  preparerTables();
  Syn *s = calloc(1, sizeof(Syn));
  s->hasard[0] = graine ^ 0x9E3779B97F4A7C15ull; s->hasard[1] = (graine * 0xBF58476D1CE4E5B9ull) | 1;
  for(int i = 0; i < 20; i++) alea01(s);
  s->nbJ = malloc(sizeof(int)*MAX*NBMAX);
  s->P = (Reglages){ .taille = 34, .mollesse = .5, .viscosite = .35, .sechage = .3, .contact = .3, .pixel = 6, .motif = -1, .palette = PAL_ACIDE,
                     .fond = FOND_BLANC, .vitesse = 1, .sol = 0, .pinceau = PIN_MOLLE, .gravite = 1, .inclinaison = 0, .variation = 0, .cerne = 0, .cerneEp = 6 };
  s->partHC = .22; s->tourNB = R01() < .5; s->qualite = 1; s->pxMin = 1; s->periode = 16.6; s->premiere = 1;
  s->Wv = largeur; s->Hv = hauteur; s->PX0 = s->P.pixel; s->graineFond = (int)(R01()*1e9);
  /* le liquide : un seul corps, toujours peint au-dessus */
  s->eau = calloc(1, sizeof(Forme)); s->eau->id = ++s->uid; s->eau->liquide = 1; s->eau->motif = M_LIQUIDE; s->eau->h = 18;
  s->capf = 64; s->formes = malloc(sizeof(Forme*)*s->capf); s->formes[0] = s->eau; s->nf = 1; ranger(s, s->eau);
  Traceur *A = &s->automate; A->etat = E_REPOS; A->n = 40; A->g0 = R01()*99; A->gg = R01()*99;
  nouveauTableau(s, 1);
  dimensionner(s);
  return s;
}
void syn_detruire(Syn *s){
  if(!s) return;
  sonDetruire(s->son);
  for(int k = 0; k < s->nf; k++) free(s->formes[k]);
  free(s->formes); free(s->parId); free(s->nbJ); free(s->pix); free(s->fondPix); free(s->sousCerne); free(s->peintCerne);
  free(s->champ); free(s->majorite); free(s->votes); free(s->tuiles); free(s->fondAncien); free(s->pixAncien); free(s->masque); free(s->chemin);
  free(s->GS.debut); free(s->GS.rempli); free(s->GL.debut); free(s->GL.rempli);
  free(s);
}
const uint32_t *syn_pixels(const Syn *s, int *gw, int *gh, int *taille){
  if(gw) *gw = s->gw; if(gh) *gh = s->gh; if(taille) *taille = s->PX0;
  return s->pix;
}

/* branche le moteur sonore : fréquence d'échantillonnage de la sortie, volume de 0 à 1 */
void syn_activer_son(Syn *s, double frequence, double volume){ syn_activer_son_canaux(s, frequence, volume, 2); }
void syn_latence(Syn *s, double secondes){ s->latence = borne(secondes, 0, .5); }
/* la sortie change (une enceinte de plus, AirPlay…) : le moteur passe en stéréo ou en 5.1. À appeler son arrêté. */
void syn_sortie(Syn *s, int canaux){
  Son *so = s->son; if(!so) return; int nc = canaux > 2 ? 6 : 2; if(nc == so->nc) return;
  so->nc = nc; free(so->anneau); so->anneau = calloc((size_t)so->capa*nc, sizeof(float)); atomic_store(&so->lu, 0); atomic_store(&so->ecrit, 0);
  so->compGain = 1;
}
int syn_canaux(const Syn *s){ return s->son ? s->son->nc : 0; }
void syn_activer_son_canaux(Syn *s, double frequence, double volume, int canaux){
  if(s->son) return;
  sonCreer(s, frequence, volume, canaux);
  Son *so = s->son; so->style = ORDRE_VEILLE[s->scene % 12]; so->tempo = STYLES[so->style].bpm;
  { const Style *st = &STYLES[so->style]; so->orch = st->nOrch ? st->orch[(int)(R01()*st->nOrch)] : OR_SYNTHE; }
  accorder(s); sonTempo(s); espace(s);
}

/* volume de la musique, de 0 à 1 (0 : muet) */
void syn_volume(Syn *s, double volume){ if(s->son) viser(&s->son->maitre, borne(volume, 0, 1)*1.5, .15); }
/* un nouveau tableau, tout de suite */
void syn_nouveau_tableau(Syn *s){ nouveauTableau(s, 0); s->prochainTableau = s->dernier + alea(s, 40, 85)*1000; }
