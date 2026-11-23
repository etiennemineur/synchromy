/* ================= make me dance : la jungle anglaise des années 90, en set de DJ =================
   Port du moteur de la version web (etienne.design/tools/synchromy). Des morceaux de 128 mesures (environ quatre minutes
   à 120 bpm), chacun dans un genre : ragga, atmosphérique, darkside, amen, jump-up. Chaque morceau s'ouvre sur seize mesures
   d'intro et se ferme sur seize mesures d'outro, mixées sur deux platines : la batterie qui entre arrive filtrée, les basses
   s'échangent à mi-mix, le tempo glisse d'un morceau à l'autre. Des breaks composés pour Synchromy, découpés en 32 tranches,
   des boîtes à rythmes analogiques dessous, une sub ronde ou une basse analogique, des nappes et des accords doux.
   Aucune distorsion : une colle douce, une compression parallèle, une petite pièce, une grande salle, un écho calé. */
#include "jg_sons.h"

#define JG_PISTE 2048
#define JG_MIX 256
#define JG_PER (JG_PISTE - JG_MIX)
static const double JG_G0 = 43.65;   /* fa 1 : la tonique de référence */

/* ---------- le hasard reproductible du web (Park-Miller) ---------- */
typedef struct { double x; } RngJ;
static inline RngJ rngJ(double s){ RngJ r; r.x = fmod(floor(fabs(s)), 2147483646.0) + 1; return r; }
static inline double rj(RngJ *r){ r->x = fmod(r->x*16807.0, 2147483647.0); return r->x/2147483647.0; }
#define PICKJ(R, a) ((a)[(int)(rj(R)*(int)(sizeof(a)/sizeof((a)[0])))])

/* ---------- les tables ---------- */
enum { JB_INTRO, JB_OUTRO, JB_ROLLER, JB_DNB, JB_RAGGA, JB_AMEN, JB_DEMI, JB_BREAKDOWN, JB_MONTEE, JB_DOUBLE, JB_REWIND, JB_STOP, NJB };
typedef struct { int nb0, nb1; double e; int section, brut; } BlocDefJ;
static const BlocDefJ JBLOCS[NJB] = {
  [JB_INTRO] = {4, 4, .6, HS_FOND, 0},   [JB_OUTRO] = {4, 4, .7, HS_CHUTE, 0}, [JB_ROLLER] = {4, 8, 1, HS_CHUTE, 0}, [JB_DNB] = {4, 8, 1, HS_CHUTE, 0},
  [JB_RAGGA] = {4, 6, .95, HS_CHUTE, 0}, [JB_AMEN] = {2, 4, .85, HS_COMPLEXE, 0}, [JB_DEMI] = {2, 4, .85, HS_CHUTE, 0}, [JB_BREAKDOWN] = {2, 4, .3, HS_COMPLEXE, 0},
  [JB_MONTEE] = {1, 2, .7, HS_MONTEE, 0}, [JB_DOUBLE] = {8, 8, .95, HS_COMPLEXE, 1}, [JB_REWIND] = {1, 1, .6, HS_COMPLEXE, 1}, [JB_STOP] = {1, 1, .1, HS_COMPLEXE, 1} };
/* les enchaînements : poids du bloc suivant */
static double JSUITE[NJB][NJB];
static void jSuite(int de, int vers, double w){ JSUITE[de][vers] = w; }

enum { MO_EOLIEN, MO_DORIEN, MO_PHRYGIEN, MO_HARMONIQUE };
static const int JMODES[4][7] = { {0,2,3,5,7,8,10}, {0,2,3,5,7,9,10}, {0,1,3,5,7,8,10}, {0,2,3,5,7,8,11} };
static inline int degSemiJ(const int *m, int k){ int o = (int)floor(k/7.0); return o*12 + m[k - 7*o]; }

/* les breaks : a « la balle », b « le serré », c « le lourd », e « le tambourin », f « le groove » */
enum { BK_A, BK_B, BK_C, BK_E, BK_F, NBK };
typedef struct { int k[32], nk, s[32], ns, g[32], ng, h[32], nh; char kc[32], sc[32]; } RoleJ;
static RoleJ JROLES[NBK];

enum { GJ_RAGGA, GJ_ATMOS, GJ_DARK, GJ_AMEN, GJ_JUMP, NGJ };
enum { GR_SUB, GR_ANALOG, GR_DUB };
enum { RV_AUCUN, RV_RHODES, RV_PIANO };
typedef struct { const char *nom; double vit; int modes[3], nModes, brk[3], nBrk; double blocs[NJB]; int grave, skank, rave; double nappe; int nappeGrave;
                 double swing, filtre, tamb; int octaves; double mordant; int hache; } GenreJ;
static GenreJ JGENRES[NGJ];
static const double JGAIN[NJB] = { [JB_DNB] = .92, [JB_ROLLER] = 1.05, [JB_RAGGA] = 1.2, [JB_AMEN] = 1.18, [JB_DEMI] = .95, [JB_DOUBLE] = 1.05 };

enum { FJ_ROLL, FJ_KICKS, FJ_STUTTER, FJ_HALFBAR, FJ_REVERSE, FJ_CHOP, FJ_CUT, FJ_RICOCHET, FJ_PROGRESSIF, FJ_TROU, FJ_DEMITROU };
static const int JFINS[10] = { FJ_ROLL, FJ_ROLL, FJ_KICKS, FJ_STUTTER, FJ_HALFBAR, FJ_REVERSE, FJ_CHOP, FJ_CUT, FJ_RICOCHET, FJ_PROGRESSIF };
static const int JFINS8[6] = { FJ_TROU, FJ_DEMITROU, FJ_PROGRESSIF, FJ_ROLL, FJ_TROU, FJ_PROGRESSIF };
static const char *JCOUPES[12] = { "....S.......S...", "........KkS.KkS.", "..K.....K.K.S..S", "....S..S..S.S.SS", "K.K.S.g.K.K.S.gS", "........hhhhS...",
                                   "....S...KK..S.--", "K..K..S...K.S.k.", "..h.S.h...h.S.h.", "K...S..K..K.S...", "....S.g.g.K.S..g", "K.k.S...K.k.T..." };
static const char *JKITS[6][2] = { {"K.........K.....", "....S.......S..."}, {"K.........K..K..", "....S.......S..."}, {"K......K..K.....", "....S.......S..."},
                                   {"K.K.......K.....", "....S.......S..s"}, {"K.........KK....", "....S.......S.s."}, {"K..K......K.....", "....S..s....S..."} };
static const int JBASSES[7][10][2] = {
  {{0,10},{10,6},{16,16},{32,10},{42,6},{48,8},{56,8},{-1,0}}, {{0,6},{6,10},{16,8},{24,8},{32,6},{38,10},{48,16},{-1,0}},
  {{0,16},{16,6},{22,10},{32,16},{48,4},{52,4},{56,8},{-1,0}}, {{0,4},{4,4},{8,8},{16,12},{28,4},{32,4},{36,4},{40,8},{48,16},{-1,0}},
  {{0,12},{12,4},{16,12},{28,4},{32,12},{44,4},{48,8},{56,8},{-1,0}}, {{0,8},{8,8},{16,6},{22,10},{32,8},{40,8},{48,6},{54,10},{-1,0}},
  {{0,3},{3,3},{6,10},{16,16},{32,3},{35,3},{38,10},{48,8},{56,8},{-1,0}} };
static const int JDNB[4][16][2] = {
  {{0,3},{3,3},{6,4},{10,6},{16,3},{19,3},{22,4},{26,6},{32,3},{35,3},{38,4},{42,6},{48,6},{54,4},{58,6},{-1,0}},
  {{0,6},{6,2},{8,6},{14,2},{16,6},{22,2},{24,8},{32,6},{38,2},{40,6},{46,2},{48,12},{60,4},{-1,0}},
  {{0,4},{4,4},{8,8},{16,4},{20,4},{24,8},{32,4},{36,4},{40,8},{48,4},{52,4},{56,4},{60,4},{-1,0}},
  {{0,10},{10,3},{13,3},{16,10},{26,3},{29,3},{32,10},{42,3},{45,3},{48,8},{56,4},{60,4},{-1,0}} };
static const int JACCORDS[6][4] = { {0,5,6,4}, {0,3,6,5}, {0,6,5,4}, {0,0,5,6}, {0,4,5,3}, {0,2,5,6} };
static const char *JRAVE[5] = { "x..x..x...x.....", "x.....x...x..x..", "...x..x....x..x.", "x..x......x..x..", "..x...x...x....." };
static const int JCHANTS[4][8] = { {0,2,4,7,9,7,4,2}, {0,4,2,5,4,7,6,9}, {7,6,4,2,4,2,0,-1}, {0,2,0,4,2,5,4,7} };

static void jgPreparer(void){
  static int fait = 0; if(fait) return; fait = 1;
  /* les rôles de chaque break : grosses caisses, caisses claires, fantômes, charlestons seuls */
  const char **MO[NBK] = { JG_MOTIF_BRK_A, JG_MOTIF_BRK_B, JG_MOTIF_BRK_C, JG_MOTIF_BRK_E, JG_MOTIF_BRK_F };
  for(int b = 0; b < NBK; b++){ RoleJ *R = &JROLES[b]; memset(R, 0, sizeof *R);
    for(int j = 0; j < 32; j++){ char a = MO[b][0][j], c = MO[b][1][j]; R->kc[j] = a; R->sc[j] = c;
      if(a == 'x') R->k[R->nk++] = j; if(c == 'X') R->s[R->ns++] = j; if(c == 'g' && a != 'x') R->g[R->ng++] = j; if(c == '.' && a != 'x') R->h[R->nh++] = j; } }
  /* les enchaînements */
  jSuite(JB_INTRO, JB_ROLLER, 3); jSuite(JB_INTRO, JB_DNB, 3);
  { int d = JB_DNB; jSuite(d, JB_ROLLER, 2); jSuite(d, JB_AMEN, 1.2); jSuite(d, JB_DEMI, .6); jSuite(d, JB_RAGGA, 1); jSuite(d, JB_BREAKDOWN, 1); jSuite(d, JB_REWIND, .25); jSuite(d, JB_DOUBLE, .4); jSuite(d, JB_STOP, .25); jSuite(d, JB_MONTEE, 1.2); }
  { int d = JB_ROLLER; jSuite(d, JB_DNB, 2); jSuite(d, JB_AMEN, 1.4); jSuite(d, JB_RAGGA, 1.2); jSuite(d, JB_DEMI, .6); jSuite(d, JB_BREAKDOWN, 1); jSuite(d, JB_REWIND, .25); jSuite(d, JB_DOUBLE, .4); jSuite(d, JB_STOP, .25); jSuite(d, JB_MONTEE, 1.2); }
  { int d = JB_RAGGA; jSuite(d, JB_ROLLER, 2); jSuite(d, JB_DNB, 1.6); jSuite(d, JB_AMEN, 1); jSuite(d, JB_DEMI, .6); jSuite(d, JB_BREAKDOWN, .6); jSuite(d, JB_REWIND, .25); jSuite(d, JB_MONTEE, .8); }
  { int d = JB_AMEN; jSuite(d, JB_ROLLER, 2); jSuite(d, JB_DNB, 2); jSuite(d, JB_RAGGA, .8); jSuite(d, JB_DEMI, .6); jSuite(d, JB_BREAKDOWN, .6); jSuite(d, JB_MONTEE, .8); }
  { int d = JB_DEMI; jSuite(d, JB_ROLLER, 2); jSuite(d, JB_DNB, 1.6); jSuite(d, JB_AMEN, 1); jSuite(d, JB_MONTEE, 1); }
  { int d = JB_BREAKDOWN; jSuite(d, JB_MONTEE, 4); jSuite(d, JB_ROLLER, .8); jSuite(d, JB_DNB, .8); }
  { int d = JB_MONTEE; jSuite(d, JB_ROLLER, 3); jSuite(d, JB_DNB, 3); jSuite(d, JB_AMEN, 1); jSuite(d, JB_RAGGA, .6); }
  { int d = JB_DOUBLE; jSuite(d, JB_ROLLER, 2); jSuite(d, JB_DNB, 1.5); jSuite(d, JB_AMEN, 1); }
  { int d = JB_REWIND; jSuite(d, JB_ROLLER, 3); jSuite(d, JB_DNB, 2.5); jSuite(d, JB_AMEN, 1); }
  { int d = JB_STOP; jSuite(d, JB_ROLLER, 3); jSuite(d, JB_DNB, 2.5); jSuite(d, JB_AMEN, 1); }
  jSuite(JB_OUTRO, JB_ROLLER, 1);
  /* les genres */
  for(int g = 0; g < NGJ; g++){ GenreJ *G = &JGENRES[g]; memset(G, 0, sizeof *G); for(int b = 0; b < NJB; b++) G->blocs[b] = -1; G->filtre = 1; }
  { GenreJ *G = &JGENRES[GJ_RAGGA]; G->nom = "ragga"; G->vit = .98; G->modes[0] = MO_EOLIEN; G->modes[1] = MO_DORIEN; G->nModes = 2; G->brk[0] = BK_A; G->brk[1] = BK_A; G->brk[2] = BK_F; G->nBrk = 3;
    G->blocs[JB_RAGGA] = 4; G->blocs[JB_ROLLER] = 1.6; G->blocs[JB_AMEN] = .5; G->blocs[JB_DEMI] = .4; G->blocs[JB_BREAKDOWN] = .5; G->blocs[JB_DOUBLE] = .2; G->blocs[JB_DNB] = .3;
    G->grave = GR_DUB; G->skank = 1; G->swing = .13; G->filtre = .95; G->tamb = .2; }
  { GenreJ *G = &JGENRES[GJ_ATMOS]; G->nom = "atmosphérique"; G->vit = .96; G->modes[0] = MO_DORIEN; G->modes[1] = MO_DORIEN; G->modes[2] = MO_EOLIEN; G->nModes = 3; G->brk[0] = BK_E; G->brk[1] = BK_E; G->brk[2] = BK_B; G->nBrk = 3;
    G->blocs[JB_ROLLER] = 3; G->blocs[JB_DNB] = 1.6; G->blocs[JB_BREAKDOWN] = 2.2; G->blocs[JB_DEMI] = .4; G->blocs[JB_AMEN] = .2; G->blocs[JB_RAGGA] = .15; G->blocs[JB_DOUBLE] = .3;
    G->grave = GR_SUB; G->rave = RV_RHODES; G->nappe = 1.15; G->swing = .07; G->filtre = .87; G->tamb = .7; }
  { GenreJ *G = &JGENRES[GJ_DARK]; G->nom = "darkside"; G->vit = 1; G->modes[0] = MO_PHRYGIEN; G->modes[1] = MO_HARMONIQUE; G->modes[2] = MO_PHRYGIEN; G->nModes = 3; G->brk[0] = BK_A; G->brk[1] = BK_C; G->brk[2] = BK_A; G->nBrk = 3;
    G->blocs[JB_DEMI] = 3; G->blocs[JB_ROLLER] = 2; G->blocs[JB_AMEN] = 1.2; G->blocs[JB_DNB] = .6; G->blocs[JB_BREAKDOWN] = .7; G->blocs[JB_RAGGA] = .1; G->blocs[JB_DOUBLE] = .2;
    G->grave = GR_ANALOG; G->nappe = .8; G->nappeGrave = 1; G->filtre = .8; }
  { GenreJ *G = &JGENRES[GJ_AMEN]; G->nom = "amen"; G->vit = 1.02; G->modes[0] = MO_EOLIEN; G->modes[1] = MO_PHRYGIEN; G->nModes = 2; G->brk[0] = BK_A; G->nBrk = 1;
    G->blocs[JB_AMEN] = 4; G->blocs[JB_ROLLER] = 1.6; G->blocs[JB_DEMI] = .5; G->blocs[JB_DNB] = .4; G->blocs[JB_BREAKDOWN] = .4; G->blocs[JB_RAGGA] = .2;
    G->grave = GR_SUB; G->mordant = 2; G->hache = 1; }
  { GenreJ *G = &JGENRES[GJ_JUMP]; G->nom = "jump-up"; G->vit = 1.04; G->modes[0] = MO_EOLIEN; G->modes[1] = MO_DORIEN; G->nModes = 2; G->brk[0] = BK_F; G->brk[1] = BK_B; G->brk[2] = BK_E; G->nBrk = 3;
    G->blocs[JB_DNB] = 3; G->blocs[JB_DOUBLE] = 2; G->blocs[JB_ROLLER] = 1; G->blocs[JB_AMEN] = .5; G->blocs[JB_BREAKDOWN] = .5; G->blocs[JB_RAGGA] = .2;
    G->grave = GR_SUB; G->octaves = 1; G->rave = RV_PIANO; G->tamb = .3; }
}

/* ---------- la musique d'un morceau : genre, mode, tonalité, basses, accords, une petite mélodie ---------- */
typedef struct { int ok, d, n, o, g; } NoteBJ;
typedef struct { int o, d, n, nul; } NoteCJ;
typedef struct { int genre, mode[7], cle, voixDub, acc[4][4], chant[8], nCell, Lcell; NoteBJ basse[64], dnb[64]; char rave[17]; NoteCJ cell[12]; } MelJ;
static int jgGenre(int c, double g){
  RngJ R = rngJ(h1(g*9.13)*2e9 + 5); int l[NGJ] = { GJ_RAGGA, GJ_ATMOS, GJ_DARK, GJ_AMEN, GJ_JUMP };
  for(int i = NGJ - 1; i > 0; i--){ int j = (int)(rj(&R)*(i + 1)); int t = l[i]; l[i] = l[j]; l[j] = t; }
  return l[((c % NGJ) + NGJ) % NGJ];
}
static void jgMel(MelJ *M, int c, double g){
  memset(M, 0, sizeof *M); RngJ R = rngJ(h1(c*7.77 + g*1.31)*2e9 + c);
  M->genre = jgGenre(c, g); const GenreJ *G = &JGENRES[M->genre];
  const int *mode = JMODES[G->modes[(int)(rj(&R)*G->nModes)]]; memcpy(M->mode, mode, sizeof M->mode);
  /* une cellule mélodique, pour les breakdowns */
  { static const int K0[4] = {0, 2, 4, 7}, DUR[8] = {1, 2, 2, 3, 3, 4, 5, 6}, PAS[12] = {-5, -4, -3, -2, 2, 3, 4, 5, 7, -7, 1, -1};
    int n = 3 + (int)(rj(&R)*5), o = 0, k = K0[(int)(rj(&R)*4)];
    for(int i = 0; i < n && i < 11; i++){ int d = DUR[(int)(rj(&R)*8)]; if(i){ k += PAS[(int)(rj(&R)*12)]; if(k < -4) k = -4; if(k > 12) k = 12; }
      M->cell[i].o = o; M->cell[i].d = d; M->cell[i].n = degSemiJ(mode, k); M->cell[i].nul = i && rj(&R) < .12; o += d; M->nCell = i + 1; }
    if(o % 4 == 0){ M->cell[M->nCell - 1].d++; o++; } M->Lcell = o; }
  { static const int KB[9] = {0, 0, 4, 5, 6, 2, 3, -2, -3}; const int (*L)[2] = JBASSES[(int)(rj(&R)*7)];
    for(int i = 0; L[i][0] >= 0; i++){ int s0 = L[i][0], k = i == 0 || s0 == 32 ? 0 : KB[(int)(rj(&R)*9)]; NoteBJ *b = &M->basse[s0]; b->ok = 1; b->d = L[i][1]; b->n = degSemiJ(mode, k); b->o = 1; } }
  { const int *A = JACCORDS[(int)(rj(&R)*6)]; for(int r = 0; r < 4; r++) for(int j = 0; j < 4; j++) M->acc[r][j] = degSemiJ(mode, A[r] + 2*j); }
  { static const int KD[7] = {0, 0, 7, 4, 7, 5, -2}; const int (*L)[2] = JDNB[(int)(rj(&R)*4)]; int oPrec = 1;
    for(int i = 0; L[i][0] >= 0; i++){ int s0 = L[i][0], k = s0 % 16 == 0 ? 0 : KD[(int)(rj(&R)*7)], o = k == 7 ? 2 : 1; NoteBJ *b = &M->dnb[s0];
      b->ok = 1; b->d = L[i][1]; b->n = degSemiJ(mode, k == 7 ? 0 : k); b->o = o; b->g = o != oPrec || (s0 % 4 == 0 && rj(&R) < .2); oPrec = o; } }
  M->voixDub = G->grave == GR_DUB;
  { static const int CL[8] = {0, 0, 2, -2, 5, -5, 3, -3}; M->cle = CL[(int)(rj(&R)*8)]; }
  rj(&R);   /* (le mordant du web) */
  strcpy(M->rave, JRAVE[(int)(rj(&R)*5)]); rj(&R);
  { const int *ch = JCHANTS[(int)(rj(&R)*4)]; for(int i = 0; i < 8; i++) M->chant[i] = degSemiJ(mode, ch[i]); }
}
/* la note de la cellule qui tombe à ce pas (transformée au fil des tours : rétrograde, renversée, transposée, trouée) */
static int jgNoteCellule(const MelJ *M, int pos, double graine, int *n, int *d){
  if(M->Lcell <= 0) return 0; int k = pos/M->Lcell, loc = pos - k*M->Lcell, tf = (int)(h1(graine + k*1.618)*8);
  int i = -1; for(int q = 0; q < M->nCell; q++) if(M->cell[q].o == loc){ i = q; break; } if(i < 0 || M->cell[i].nul) return 0;
  int sm = M->cell[i].n, n0 = M->cell[0].n;
  if(tf == 3) sm = 2*n0 - sm; else if(tf == 4){ static const int T[5] = {6, 5, 1, -2, 7}; sm += T[(int)(h1(graine + k*2.7)*5)]; }
  else if(tf == 5){ static const int O[4] = {-12, 0, 0, 12}; sm += O[(int)(h1(graine + k*3.1 + i*7.7)*4)]; }
  else if(tf == 6 && i > 0 && h1(graine + k*2.3 + i*5.1) < .4) return 0;
  while(sm > 24) sm -= 12; while(sm < -10) sm += 12; *n = sm; *d = M->cell[i].d; return 1;
}

/* ---------- la forme d'un morceau : 128 mesures, une intro et une outro de seize mesures ---------- */
typedef struct { int type, nb, N, debut, brk, brk2, grave, tamb, drop, coup, chante, voixDub, fins[4], fins8[4], coupes[4], kits[4]; double vit, vers, graine, swing; } BlocJ;
typedef struct { int d, k, cur, bloc; } BarreJ;
typedef struct { int ok, c; double g; MelJ M; BlocJ B[64]; int nB; BarreJ X[160]; int nX; } PisteJ;
static PisteJ JCACHE[6]; static int jCacheI = 0;
static const PisteJ *jgPiste(int c, double g){
  for(int i = 0; i < 6; i++) if(JCACHE[i].ok && JCACHE[i].c == c && JCACHE[i].g == g) return &JCACHE[i];
  PisteJ *P = &JCACHE[jCacheI]; jCacheI = (jCacheI + 1) % 6; memset(P, 0, sizeof *P); P->c = c; P->g = g;
  jgMel(&P->M, c, g); const GenreJ *G = &JGENRES[P->M.genre];
  RngJ R = rngJ(h1(c*3.33 + g*.71)*2e9 + 17);
  int d = 0, prec = -1, vus[NJB] = {0}; double vitSuite = G->vit;
  int brkPiste = G->brk[(int)(rj(&R)*G->nBrk)];
  #define POSE(TYPE, NB) do{ int type_ = (TYPE), nb_ = (NB); BlocJ *B = &P->B[P->nB++]; memset(B, 0, sizeof *B); B->type = type_; B->nb = nb_; B->N = nb_*16; B->debut = d; \
    B->brk = type_ == JB_INTRO || type_ == JB_OUTRO ? brkPiste : type_ == JB_DEMI ? (rj(&R) < .7 ? BK_C : BK_A) : G->brk[(int)(rj(&R)*G->nBrk)]; \
    int alt_ = (type_ == JB_DNB || type_ == JB_ROLLER || type_ == JB_AMEN) && nb_ >= 8 && rj(&R) < .7; B->brk2 = -1; \
    if(alt_){ int l_[3], n_ = 0; for(int q = 0; q < G->nBrk; q++) if(G->brk[q] != B->brk) l_[n_++] = G->brk[q]; if(!n_){ if(B->brk != BK_E) l_[n_++] = BK_E; if(B->brk != BK_B) l_[n_++] = BK_B; } B->brk2 = l_[(int)(rj(&R)*n_)]; } \
    B->vit = vitSuite; B->vers = type_ == JB_MONTEE ? (vitSuite > G->vit*1.01 ? vitSuite : rj(&R) < .6 ? G->vit*4/3 : G->vit) : vitSuite; B->graine = rj(&R)*1e5; \
    B->grave = G->grave == GR_ANALOG; B->tamb = rj(&R) < G->tamb; for(int q = 0; q < 4; q++) B->fins8[q] = JFINS8[(int)(rj(&R)*6)]; \
    B->drop = prec == JB_MONTEE || prec == JB_REWIND || prec == JB_STOP || prec == JB_BREAKDOWN || prec == JB_INTRO; \
    for(int q = 0; q < 4; q++) B->fins[q] = JFINS[(int)(rj(&R)*10)]; for(int q = 0; q < 4; q++) B->coupes[q] = (int)(rj(&R)*12); \
    B->voixDub = type_ == JB_RAGGA || P->M.voixDub; B->coup = rj(&R) < .5; \
    B->swing = G->swing > 0 && (type_ == JB_ROLLER || type_ == JB_RAGGA || type_ == JB_AMEN || type_ == JB_DOUBLE || type_ == JB_DNB) ? G->swing*(.8 + .4*rj(&R)) : 0; \
    B->chante = rj(&R) < .45; for(int q = 0; q < 4; q++) B->kits[q] = (int)(rj(&R)*6); \
    vus[type_]++; if(type_ == JB_MONTEE) vitSuite = B->vers; \
    for(int k_ = 0; k_ < nb_ && P->nX < 160; k_++){ BarreJ *X = &P->X[P->nX++]; X->d = d; X->k = k_; X->cur = (k_ % 2)*16; X->bloc = P->nB - 1; d += 16; } \
    prec = type_; }while(0)
  POSE(JB_INTRO, 16);
  const int FIN = JG_PISTE - 256; int type;
  { double w[NJB] = {0}, t = 0; int cand[5] = { JB_ROLLER, JB_DNB, JB_RAGGA, JB_AMEN, JB_DEMI };
    for(int q = 0; q < 5; q++){ double b = G->blocs[cand[q]]; if(b > 0){ w[cand[q]] = b; t += b; } }
    double x = rj(&R)*t; type = JB_ROLLER; for(int q = 0; q < 5; q++){ x -= w[cand[q]]; if(w[cand[q]] > 0 && x <= 0){ type = cand[q]; break; } } }
  while(d < FIN && P->nB < 60){
    int reste = (FIN - d)/16; const BlocDefJ *T = &JBLOCS[type];
    int nb = (T->nb0 + (int)(rj(&R)*(T->nb1 - T->nb0 + 1)))*(T->brut ? 1 : 4);
    if(type == JB_MONTEE){ static const int MN[3] = {8, 8, 16}; nb = vitSuite > G->vit*1.01 ? 8 : MN[(int)(rj(&R)*3)]; }
    int brut1 = T->brut && type != JB_DOUBLE; if(brut1) nb = 1;
    int decal = (d/16) % 4; if(!brut1 && decal) nb = imx(4, nb - decal);
    if(reste - nb < 8) nb = reste; if(nb > reste) nb = reste;
    if(nb == reste && (type == JB_MONTEE || type == JB_BREAKDOWN || type == JB_STOP || type == JB_REWIND)) type = vus[JB_ROLLER] ? JB_DNB : JB_ROLLER;
    POSE(type, nb);
    double w[NJB] = {0}, tot = 0;
    for(int q = 0; q < NJB; q++){ double b = JSUITE[type][q]; if(q == type || b <= 0) continue;
      b *= G->blocs[q] >= 0 ? pow(G->blocs[q], 1.6) : (q == JB_MONTEE || q == JB_REWIND || q == JB_STOP) ? 1 : .15;
      w[q] = b/(1 + .8*vus[q]); tot += w[q]; }
    double x = rj(&R)*tot; int suite = JB_ROLLER; for(int q = 0; q < NJB; q++){ if(w[q] <= 0) continue; x -= w[q]; if(x <= 0){ suite = q; break; } }
    type = suite;
  }
  POSE(JB_OUTRO, 16);
  #undef POSE
  P->ok = 1; return P;
}
typedef struct { const PisteJ *P; const BarreJ *X; const BlocJ *B; int p, c; } InfoJ;
static InfoJ jgBarreDe(int c, int loc, double g){
  InfoJ I; I.P = jgPiste(c, g); I.c = c; int lo = 0, hi = I.P->nX - 1;
  while(lo < hi){ int m = (lo + hi + 1) >> 1; if(I.P->X[m].d <= loc) lo = m; else hi = m - 1; }
  I.X = &I.P->X[lo]; I.B = &I.P->B[I.X->bloc]; I.p = loc - I.X->d; return I;
}

/* ---------- un pas de la partition ---------- */
typedef struct { int on, nom, i, n, inv, dur; double r, rmp, niv; } TrJ;
typedef struct {
  int p, k, bloc, genre, section, montee, etage, impact, pause, trou, jet, china, boum, dub, renfort, kick, caisse, rat, roll, c200, fin, debutPiste;
  TrJ tr; int kit, kitK, kitTamb; double kitS, kitH;
  int bj, bjN, bjD, bjV, bjO, bjG;
  int nappe, nappeN[4], nNappe, nappeGrave; double nappeNiv, nappeDur;
  int skank, skankN[4];
  int rave, nRave, raveN[5]; double raveR, raveDur, raveNiv;
  int mel, melN, melD;
  int cle, mode[7];
  double vit, filtre, mordant, swing, prog, gain;
  int deck2, entre; TrJ tr2; double mixX;
} PasJ;
static inline TrJ trJ(int nom, int i){ TrJ t = { 1, nom, ((i % 32) + 32) % 32, 1, 0, 1, 1, 0, 1 }; return t; }
static PasJ jgPas(int st, InfoJ I){
  const BlocJ *B = I.B; const BarreJ *X = I.X; const MelJ *M = &I.P->M; const GenreJ *GJ = &JGENRES[M->genre];
  int p = I.p, k = X->k, mi = k & 1, fin = k == B->nb - 1, N = B->N, depuis = X->d - B->debut + p, phr = k % 4;
  int nom = B->brk2 >= 0 && k >= B->nb/2 ? B->brk2 : B->brk; const RoleJ *RO = &JROLES[nom];
  #define U(sv) h1(st*1.37 + (sv)*7.31 + B->graine)
  PasJ e; memset(&e, 0, sizeof e); e.p = p; e.k = k; e.bloc = B->type; e.genre = M->genre; e.vit = B->vit > 0 ? B->vit : 1; e.cle = M->cle; e.filtre = 1; e.mordant = 1; e.swing = B->swing;
  e.renfort = 1; e.section = JBLOCS[B->type].section; e.gain = JGAIN[B->type] > 0 ? JGAIN[B->type] : 1; memcpy(e.mode, M->mode, sizeof e.mode);
  int K0 = RO->nk ? RO->k[0] : 0, K1 = RO->nk > 1 ? RO->k[1] : K0 + 2, S0 = RO->ns ? RO->s[0] : 4, S1 = RO->ns > 1 ? RO->s[1] : S0, G0 = RO->ng ? RO->g[0] : 7;
  #define TR(I) trJ(nom, (I))
  #define DROIT() TR(X->cur + p)
  TrJ nul = {0};
  #define ROLE(C, Q) ((C) == 'K' ? TR(K0) : (C) == 'k' ? TR(K1) : (C) == 'S' ? TR(S0) : (C) == 'T' ? TR(S1) : (C) == 'g' ? ({ TrJ t_ = TR(G0); t_.niv = .8; t_; }) : \
                      (C) == 'h' ? TR(RO->nh ? RO->h[(Q) % RO->nh] : X->cur + (Q)) : (C) == '-' ? nul : TR(X->cur + (Q)))
  #define MICRO(Q) do{ if(e.tr.on && U(1) <= (Q)){ int c_ = (int)(U(2)*4); if(c_ == 0 && RO->ng) e.tr.i = RO->g[(int)(U(3)*RO->ng)]; else if(c_ == 2) e.tr.r = 1.5; else e.tr.n = 2; } }while(0)
  #define BASSE() do{ const NoteBJ *n_ = GJ->octaves ? &M->dnb[(k % 4)*16 + p] : &M->basse[(k % 4)*16 + p]; \
    if(n_->ok){ e.bj = 1; e.bjN = n_->n; e.bjD = n_->d; e.bjV = B->voixDub ? GR_DUB : B->grave ? GR_ANALOG : GR_SUB; e.bjO = n_->o ? n_->o : 1; e.bjG = n_->g; } }while(0)
  #define ACC(i) (M->acc[(i) % 4])
  /* les retouches de fin de phrase */
  #define FINJ(SORTE) do{ int so_ = (SORTE); if(p >= 8){ e.fin = p >= 12; switch(so_){ \
    case FJ_ROLL: if(p >= 12){ e.tr = TR(S0); e.tr.n = p >= 14 ? 2 : 1; e.tr.r = pow(2, (p - 12)/24.0); } break; \
    case FJ_KICKS: e.tr = ROLE("KkS.KkSS"[p - 8], p); break; \
    case FJ_STUTTER: e.tr = TR(X->cur + 8 + (p % 4)); break; \
    case FJ_HALFBAR: e.tr = TR(X->cur + p - 8); break; \
    case FJ_REVERSE: if(p >= 12){ e.tr = TR(X->cur + p); e.tr.inv = 1; } break; \
    case FJ_CHOP: e.tr = ROLE(JCOUPES[B->coupes[0]][p % 16], p); break; \
    case FJ_CUT: if(p >= 12){ e.tr = nul; e.renfort = 0; } break; \
    case FJ_TROU: if(p >= 12){ e.tr = p == 12 ? TR(S0) : nul; e.tr.niv = .85; e.jet = p == 12; e.kit = 0; e.bj = 0; e.renfort = 0; e.pause = p > 12; e.trou = p == 12; } break; \
    case FJ_DEMITROU: if(p >= 14){ e.tr = p == 14 ? TR(S0) : nul; e.tr.niv = .8; e.jet = p == 14; e.kit = 0; e.bj = 0; e.renfort = 0; e.pause = p > 14; e.trou = p == 14; } break; \
    case FJ_PROGRESSIF: if(p < 12){ if(p % 2) e.tr = nul; else { e.tr = TR(S0); e.tr.niv = .5 + (p - 8)*.04; } } else if(p < 14){ e.tr = TR(S0); e.tr.niv = .7 + (p - 12)*.06; } \
                        else { e.tr = TR(S0); e.tr.n = p == 14 ? 2 : 3; e.tr.niv = .9; e.tr.r = p == 15 ? pow(2, 1/12.0) : 1; } e.renfort = 0; break; \
    case FJ_RICOCHET: e.tr = TR(p % 2 ? S1 : S0); e.tr.n = p < 12 ? 1 : p < 14 ? 2 : 4; e.tr.r = pow(2, M->chant[p - 8]/12.0); e.tr.rmp = p >= 14 ? 2 : 0; break; \
  } } }while(0)
  int montee = B->type == JB_MONTEE; double prog = montee ? (double)depuis/N : 0, vitM = 0;
  if(montee && B->vers > B->vit) vitM = B->vit + (B->vers - B->vit)*pow((double)depuis/N, 1.6);
  int etage = 0;
  switch(B->type){
    case JB_INTRO: { int deux = k >= B->nb/2;   /* huit mesures de batterie seule, puis la basse et la nappe */
      e.renfort = deux; e.filtre = deux ? 1 : .62 + .38*mn2(1, depuis/(N/2.0));
      if(k >= 2 || deux) e.tr = DROIT(); else if(p % 2 == 0 && RO->nh){ e.tr = TR(RO->h[(p >> 1) % RO->nh]); e.tr.niv = .7; }
      if(k < 2 && (p == 0 || p == 10)){ e.tr = TR(K0); e.tr.niv = .85; }
      if(deux) BASSE(); if(p == 0 && k == B->nb/2){ e.nappe = 1; memcpy(e.nappeN, ACC(k), sizeof(int)*4); e.nNappe = 4; e.nappeDur = 64; e.nappeNiv = .7; }
      if(phr == 3 && k % 8 == 7) FINJ(FJ_ROLL); break; }
    case JB_OUTRO: { int un = k < B->nb/2; e.tr = DROIT(); MICRO(.02); e.renfort = un; if(un) BASSE(); if(phr == 3 && k % 8 == 7) FINJ(FJ_ROLL); break; }
    case JB_ROLLER: e.tr = DROIT(); MICRO(.03); BASSE();
      if(B->chante && phr == 2 && e.tr.on && RO->sc[e.tr.i] == 'X') e.tr.r = pow(2, M->chant[(p >> 1) % 8]/12.0);
      if(phr == 3) FINJ(B->fins[(k >> 2) % 4]); if(k % 8 == 7) FINJ(B->fins8[(k >> 3) % 4]); break;
    case JB_DNB: { const char *Kk = JKITS[B->kits[(k >> 2) % 4]][0], *Ks = JKITS[B->kits[(k >> 2) % 4]][1]; char kc = Kk[p], sc = Ks[p];
      e.tr = DROIT(); e.tr.niv = .65; MICRO(.03);
      e.kit = 1; e.kitK = kc == 'K'; e.kitS = sc == 'S' ? 1 : sc == 's' ? .35 : 0; e.kitH = p % 2 == 0 ? (p % 4 == 2 ? .3 : .2) : .1; e.kitTamb = B->tamb;
      if(phr == 1 && p == 14 && U(41) < .6) e.kitS = .7;
      if(phr == 2 && p == 7 && U(42) < .45) e.kitS = .45;
      if(phr == 2 && p == 11 && U(43) < .55){ e.tr = TR(S0); e.tr.inv = 1; e.tr.niv = .75; }
      if(e.tr.on && p % 4 == 3 && U(44) < .07) e.tr.n = 2;
      { const NoteBJ *n = &M->dnb[(k % 4)*16 + p]; if(n->ok){ e.bj = 1; e.bjN = n->n; e.bjD = n->d; e.bjV = B->grave ? GR_ANALOG : GR_SUB; e.bjO = n->o; e.bjG = n->g; } }
      if(phr == 3){ FINJ(B->fins[(k >> 2) % 4]); if(p >= 12 && e.kit){ e.kitK = 0; e.kitS = p % 2 ? 0 : .8; } }
      if(k % 8 == 7){ int f8 = B->fins8[(k >> 3) % 4]; FINJ(f8);
        if(f8 == FJ_PROGRESSIF && p >= 8){ e.kit = 1; e.kitK = p == 8; e.kitS = 0; e.kitH = 0; }
        if((f8 == FJ_TROU || f8 == FJ_DEMITROU) && (e.pause || e.jet)){ e.kit = 0; e.rave = 0; } }
      break; }
    case JB_RAGGA: e.tr = DROIT(); MICRO(.03); BASSE();
      if(p == 4 || p == 12){ e.skank = 1; memcpy(e.skankN, ACC(k), sizeof(int)*4); }
      if(p == 12 && mi && U(21) < .5) e.dub = 1;
      if(phr == 3) FINJ(B->fins[(k >> 2) % 4]); break;
    case JB_AMEN: e.tr = ROLE(JCOUPES[B->coupes[k % 4]][p % 16], p); MICRO(.08); BASSE(); if(phr == 3) FINJ(k % 8 == 7 ? B->fins8[(k >> 3) % 4] : FJ_RICOCHET); break;
    case JB_DEMI: e.tr = DROIT();
      if(p == 0 || (p == 8 && U(20) < .35)){ const NoteBJ *n = &M->basse[(k % 4)*16]; e.bj = 1; e.bjN = n->ok ? n->n : 0; e.bjD = p == 0 ? 14 : 8; e.bjV = GR_SUB; e.bjO = 1; }
      if(p == 0 && k % 4 == 0){ e.nappe = 1; memcpy(e.nappeN, ACC(k), sizeof(int)*4); e.nNappe = 4; e.nappeDur = 64; e.nappeNiv = 1; }
      if(phr == 3) FINJ(FJ_ROLL); break;
    case JB_DOUBLE: e.tr = ROLE("K.h.S.h.h.K.S.h."[p], p); MICRO(.05); BASSE(); if(fin) FINJ(FJ_ROLL); break;
    case JB_BREAKDOWN: if(p % 2 == 0 && RO->nh){ e.tr = TR(RO->h[(p >> 1) % RO->nh]); e.tr.niv = .45; } e.renfort = 0; e.filtre = .85;
      if(p == 0 && k % 2 == 0){ e.nappe = 1; memcpy(e.nappeN, ACC(k), sizeof(int)*4); e.nNappe = 4; e.nappeDur = 32; e.nappeNiv = 1.2; }
      if(p == 0){ e.bj = 1; e.bjN = 0; e.bjD = 16; e.bjV = GR_SUB; e.bjO = 1; }
      { int n, d; if(jgNoteCellule(M, depuis, B->graine, &n, &d)){ e.mel = 1; e.melN = n; e.melD = d; } }
      if(fin) FINJ(FJ_ROLL); break;
    case JB_MONTEE: {
      etage = k == B->nb - 1 ? 7 : (int)floor(k*7.0/imx(1, B->nb - 1));
      if(etage == 7){ if(p < 8){ e.tr = TR(S0); e.tr.n = 4; e.tr.r = pow(2, (6 + p)/12.0); e.tr.rmp = 1; } else e.pause = 1; }
      else { static const int Q4[4] = {0, 1, 2, 3};
        if(etage < 2) e.tr = DROIT(); else if(etage < 4){ int q = Q4[p % 4]; e.tr = TR(q == 0 ? K0 : q == 1 ? K0 + 1 : q == 2 ? S0 : S0 + 1); }
        else { e.tr = TR(p % 4 == 0 ? K0 : S0); e.tr.n = etage == 6 ? 2 : 1; }
        if(etage >= 4) e.tr.r = pow(2, (etage - 4 + p/16.0)*2.5/12);
        if(p % 4 == 0){ static const int DG[7] = {0, 0, 2, 2, 4, 4, 7}; e.bj = 1; e.bjN = degSemiJ(M->mode, DG[etage]); e.bjD = 3; e.bjV = GR_SUB; e.bjO = 1; } }
      e.filtre = .6 + .4*prog; break; }
    case JB_REWIND: e.tr = TR(31 - ((p*2) % 32)); e.tr.inv = 1; e.tr.r = 1.2 + 1.2*p/16.0; e.tr.n = p % 4 == 3 ? 2 : 1; e.renfort = 0;
      if(p >= 12){ e.tr = nul; e.pause = 1; } break;
    case JB_STOP: e.pause = !(p == 0 && B->coup); if(!e.pause){ e.tr = TR(K0); e.boum = 1; } break;
  }
  /* l'explosion : la boucle qui reprend après la montée, le rewind, le silence ou le breakdown */
  int impact = B->drop && depuis == 0 && (B->type == JB_DNB || B->type == JB_ROLLER || B->type == JB_RAGGA || B->type == JB_AMEN || B->type == JB_DEMI || B->type == JB_DOUBLE);
  if(impact){ e.boum = 1; e.china = 1; } e.impact = impact;
  if(e.tr.on && !e.tr.inv){ char kc = JROLES[e.tr.nom].kc[e.tr.i], sc = JROLES[e.tr.nom].sc[e.tr.i]; e.kick = kc == 'x'; e.caisse = sc == 'X'; if(e.tr.n > 1 && sc != '.'){ e.caisse = 1; e.rat = e.tr.n; } }
  if(e.kit){ if(e.kitK) e.kick = 1; if(e.kitS >= .7) e.caisse = 1; }
  if(e.renfort) e.renfort = e.kick && p % 4 == 0 && B->type != JB_DNB;
  if(p == 0 && k % 8 == 0 && (B->type == JB_DNB || B->type == JB_ROLLER || B->type == JB_RAGGA || B->type == JB_AMEN)) e.china = 1;
  e.roll = e.rat > 1;
  e.c200 = e.caisse && e.rat <= 1 && p % 8 == 4 && !e.pause && (B->type == JB_ROLLER || B->type == JB_AMEN || B->type == JB_RAGGA || B->type == JB_DOUBLE || B->type == JB_DNB);
  if(vitM > 0) e.vit = vitM;
  /* la couleur du genre */
  int corps = B->type == JB_ROLLER || B->type == JB_DNB || B->type == JB_RAGGA || B->type == JB_AMEN || B->type == JB_DEMI || B->type == JB_DOUBLE || B->type == JB_OUTRO;
  if(corps && !e.pause && !e.jet){
    if(GJ->skank && B->type != JB_DNB && B->type != JB_OUTRO && (p == 4 || p == 12)){ e.skank = 1; memcpy(e.skankN, ACC(k), sizeof(int)*4); }
    if(GJ->skank && p == 12 && (k & 1)) e.dub = 1;
    if(GJ->rave == RV_PIANO && B->type != JB_OUTRO && phr != 3 && M->rave[p] == 'x'){ e.rave = 1; e.raveR = ACC(k)[0]; e.nRave = 0; e.raveDur = 2.6; e.raveNiv = 1; }
    if(GJ->rave == RV_RHODES && B->type != JB_OUTRO && (p == 0 || (p == 10 && (k & 1)))){ e.rave = 1; e.raveR = 0; e.nRave = 4; memcpy(e.raveN, ACC(k), sizeof(int)*4); e.raveDur = 7; e.raveNiv = .8; }
    if(GJ->nappe > 0 && B->type != JB_OUTRO && p == 0 && k % 4 == 0 && !e.nappe){ const int *a = ACC(k); e.nappe = 1; e.nappeDur = 64; e.nappeNiv = GJ->nappe;
      if(GJ->nappeGrave){ e.nappeN[0] = a[0]; e.nappeN[1] = a[0] + 7; e.nNappe = 2; e.nappeGrave = 1; } else { memcpy(e.nappeN, a, sizeof(int)*4); e.nNappe = 4; } }
    if(GJ->hache && B->type == JB_ROLLER && p % 4 == 3 && U(51) < .3 && e.tr.on) e.tr = TR(X->cur + ((int)(U(52)*32)) % 32);
  }
  if(e.nappe && GJ->nappeGrave && !e.nappeGrave){ e.nappeN[1] = e.nappeN[0] + 7; e.nNappe = 2; e.nappeGrave = 1; }
  if(GJ->mordant > 0 && (B->type == JB_AMEN || B->type == JB_ROLLER)) e.mordant = pow(2, GJ->mordant/12);
  e.filtre = mn2(e.filtre, GJ->filtre);
  e.montee = montee; e.etage = etage; e.prog = prog;
  return e;
  #undef U
  #undef TR
  #undef DROIT
  #undef ROLE
  #undef MICRO
  #undef BASSE
  #undef ACC
  #undef FINJ
}

/* ---------- le set : deux platines pendant le mix ; la fin d'un épisode laisse l'outro jouer seule ---------- */
static PasJ jgSeq(double g, int st, int finPiste){
  int c = st/JG_PER, loc = st - c*JG_PER;
  if(finPiste > 0 && c >= finPiste){
    if(loc < JG_MIX){ PasJ e = jgPas(st, jgBarreDe(c - 1, loc + JG_PER, g)); return e; }
    PasJ e; memset(&e, 0, sizeof e); e.pause = 1; e.vit = 1; e.filtre = 1; e.mordant = 1; e.gain = 1; e.p = loc % 16; return e;
  }
  if(!(c > 0 && loc < JG_MIX)){ PasJ e = jgPas(st, jgBarreDe(c, loc, g)); e.debutPiste = c == 0 && loc == 0; return e; }
  double x = (double)loc/JG_MIX; int avant = x < .5;
  InfoJ sort = jgBarreDe(c - 1, loc + JG_PER, g), entre = jgBarreDe(c, loc, g);
  PasJ e = jgPas(st, avant ? sort : entre), e2 = jgPas(st + 3333331, avant ? entre : sort);
  double vS = sort.B->vit > 0 ? sort.B->vit : 1, vE = entre.B->vit > 0 ? entre.B->vit : 1, s = x*x*(3 - 2*x);
  e.vit = vS + (vE - vS)*s; e.deck2 = 1; e.tr2 = e2.tr; e.mixX = x; e.entre = avant; e.debutPiste = loc == JG_MIX/2;
  return e;
}

/* ================= le son ================= */
enum { VJ_TRANCHE, VJ_ECH, VJ_SUB, VJ_ANA, VJ_NAPPE, VJ_ACCORD, VJ_SKANK, VJ_CLOCHE };
enum { BJ_BRK, BJ_DECK2, BJ_KIT, BJ_BASSE, BJ_MEL };
typedef struct {
  int actif, type, bus, son, inv, n; long debut, fin, n0, n1;   /* n0, n1 : bornes de lecture dans la source */
  double pos, pas, niv, pan, sRev, sEcho, att, rel;
  double f, f0, tg, dur, oct; int dub, grave; double lfoF;
  double ph[24], fr[24], amp[24], dec[24]; double env; Biquad q[2]; int compte;
} VoixJ;
#define NVJ 96
enum { AJ_BASSE, AJ_D2HP, AJ_D2G, AJ_ETIME, AJ_GAIN, AJ_LP, AJ_HP, AJ_FB, AJ_EG, NAJ };
typedef struct JG {
  int vit0; double vitPas, transpo, subF; int finPiste;
  VoixJ v[NVJ];
  Evenement ev[256]; int nEv; double aV[NAJ], aC[NAJ], aT[NAJ];
  /* traitements */
  Biquad brkHp, brkCorps, brkBoue, brkAir, d2Hp, para1, para2, basseHp, melHp, roomHp, revHp, echHp, echLp, lpM[3], hpM[3]; int compte;
  double colleG, paraG, basseG, melG, limG;
  float *room[4]; int roomN[4], roomI[4]; double roomLp[4];
  float *hall[4]; int hallN[4], hallI[4]; double hallLp[4];
  float *ech[2]; int echN, echI;
  double lpCour, hpCour, d2Cour;
} JG;

static void jgCreer(Syn *s){
  jgPreparer(); Son *so = s->son; double sr = so->sr; JG *j = calloc(1, sizeof(JG)); so->hc->jg = j;
  j->transpo = 1; j->vitPas = 1;
  static const double A0[NAJ] = { .8, 30, 0, .375, 1.05, 20000, 18, .42, 0 };
  for(int k = 0; k < NAJ; k++){ j->aV[k] = j->aC[k] = A0[k]; j->aT[k] = .05; }
  waNeuf(&j->brkHp, WA_HAUT, 34, -3, 0, sr); waNeuf(&j->brkCorps, WA_PIC, 105, .9, 2, sr); waNeuf(&j->brkBoue, WA_PIC, 380, 1, -2.5, sr); waNeuf(&j->brkAir, WA_PIC, 10000, .7, -1.5, sr);
  waNeuf(&j->d2Hp, WA_HAUT, 30, -3, 0, sr); j->d2Cour = 30;
  waNeuf(&j->para1, WA_BAS, 9000, -4.4, 0, sr); waNeuf(&j->para2, WA_PIC, 420, .9, -4, sr);
  waNeuf(&j->basseHp, WA_HAUT, 26, -3, 0, sr); waNeuf(&j->melHp, WA_HAUT, 140, -3, 0, sr);
  waNeuf(&j->roomHp, WA_HAUT, 280, -3, 0, sr); waNeuf(&j->revHp, WA_HAUT, 220, -3, 0, sr);
  waNeuf(&j->echHp, WA_HAUT, 320, -3, 0, sr); waNeuf(&j->echLp, WA_BAS, 3800, -3, 0, sr);
  for(int c = 0; c < 3; c++){ waNeuf(&j->lpM[c], WA_BAS, 20000, -6, 0, sr); waNeuf(&j->hpM[c], WA_HAUT, 18, -3, 0, sr); }
  j->lpCour = 20000; j->hpCour = 18; j->colleG = j->paraG = j->basseG = j->melG = j->limG = 1;
  static const double RM[4] = { .0131, .0173, .0219, .0283 }, HL[4] = { .0433, .0517, .0601, .0689 };
  for(int i = 0; i < 4; i++){ j->roomN[i] = (int)(RM[i]*sr); j->room[i] = calloc(j->roomN[i], sizeof(float)); j->hallN[i] = (int)(HL[i]*sr); j->hall[i] = calloc(j->hallN[i], sizeof(float)); }
  j->echN = (int)(sr*2.2); j->ech[0] = calloc(j->echN, sizeof(float)); j->ech[1] = calloc(j->echN, sizeof(float));
}
static void jgDetruire(JG *j){ if(!j) return; for(int i = 0; i < 4; i++){ free(j->room[i]); free(j->hall[i]); } free(j->ech[0]); free(j->ech[1]); free(j); }
static void jgAuto(JG *j, double t, int k, double v, double tau, int saut){
  if(j->nEv >= 256) return; int i = j->nEv++;
  while(i > 0 && j->ev[i - 1].t > t){ j->ev[i] = j->ev[i - 1]; i--; }
  Evenement *q = &j->ev[i]; q->t = t; q->k = k; q->v = v; q->tau = tau; q->saut = saut;
}
static VoixJ *jgVoix(Syn *s, int type, int bus, double t, double duree){
  JG *j = s->son->hc->jg; VoixJ *v = NULL; long m = 0; int k = 0;
  for(int i = 0; i < NVJ; i++) if(!j->v[i].actif){ v = &j->v[i]; break; }
  if(!v){ for(int i = 0; i < NVJ; i++) if(i == 0 || j->v[i].debut < m){ m = j->v[i].debut; k = i; } v = &j->v[k]; }
  memset(v, 0, sizeof *v); v->actif = 1; v->type = type; v->bus = bus; double sr = s->son->sr;
  v->debut = (long)(t*sr); v->fin = v->debut + (long)((duree + .03)*sr); return v;
}
/* une tranche de break : la 32e partie, jouée à la vitesse du tempo (plus aiguë quand la jungle accélère), sans déborder */
static void jgTranche(Syn *s, TrJ T, double t, double dPas, double mordant, int jet, int dub, int bus){
  if(!T.on) return; const JgSon *S = &JG_SONS[T.nom]; double sr = s->son->sr, Ls = S->n/32.0/S->sr, cale = Ls/dPas;
  int n = T.n > 0 ? T.n : 1; double dt = dPas*(T.dur > 0 ? T.dur : 1)/n; int i = ((T.i % 32) + 32) % 32;
  for(int r = 0; r < n; r++){
    double v = cale*(T.r > 0 ? T.r : 1)*mordant*pow(2, T.rmp*r/imx(1, n - 1)/12.0); if(!(v > 0)) continue;
    double t0 = t + r*dt, len = mn2(dt, Ls*(T.dur > 0 ? T.dur : 1)/v);
    VoixJ *V = jgVoix(s, VJ_TRANCHE, bus, t0, len + .006); V->son = T.nom; V->inv = T.inv;
    V->niv = (T.niv > 0 ? T.niv : 1)*(n > 1 ? .78 + .22*r/n : 1); V->pan = n > 1 ? ((r & 1) ? .22 : -.22) : 0;
    long a = (long)(i*S->n/32.0), b = (long)((i + (T.dur > 0 ? T.dur : 1))*S->n/32.0); V->n0 = 0; V->n1 = S->n - 1;
    V->pas = v*S->sr/sr; if(T.inv){ V->pos = b - 1; V->pas = -V->pas; } else V->pos = a;
    V->fin = V->debut + (long)((len + .004)*sr); V->att = .0015; V->rel = .006;
    V->sRev = jet ? 1.1 : (T.inv || n > 2) ? .18 : 0; V->sEcho = jet ? .9 : dub ? .6 : 0;
  }
}
/* un échantillon entier : vitesse (hauteur), niveau, place, bus, envois */
static void jgEch(Syn *s, int son, double t, double niv, double vitesse, double pan, int bus, double sRev){
  const JgSon *S = &JG_SONS[son]; double sr = s->son->sr, pas = (vitesse > 0 ? vitesse : 1)*S->sr/sr, d = S->n/(pas*sr);
  VoixJ *V = jgVoix(s, VJ_ECH, bus, t, d); V->son = son; V->pos = 0; V->pas = pas; V->n0 = 0; V->n1 = S->n - 1; V->niv = niv; V->pan = pan; V->sRev = sRev; V->att = .0005; V->rel = .004;
}
/* la sub : une sinusoïde (et son octave, discrète) qui tombe d'une octave et demie à l'attaque, ou glisse depuis la note d'avant */
static void jgSub(Syn *s, double t, double f, double dur, double niv, int dub, int oct, int glisse, double dPas){
  JG *j = s->son->hc->jg; while(f < 36) f *= 2; while(f > 72) f /= 2; f *= oct > 0 ? oct : 1; dur = borne(dur, .1, 2.6);
  double de = j->subF; j->subF = f;
  VoixJ *V = jgVoix(s, VJ_SUB, BJ_BASSE, t, dur); V->f = f; int gl = glisse && de > 20 && fabs(de/f - 1) > .01; V->f0 = gl ? de : f*2.2; V->tg = gl ? .075 : .045;
  V->dur = dur; V->niv = niv; V->dub = dub; V->lfoF = 1/(dPas*2); waNeuf(&V->q[0], WA_BAS, 220, .5, 0, s->son->sr);
}
/* la basse analogique : deux scies à peine désaccordées dans un passe-bas à quatre pôles qui se referme, une sinusoïde dessous */
static void jgAna(Syn *s, double t, double f, double dur, double niv, int oct, int glisse){
  JG *j = s->son->hc->jg; while(f < 41) f *= 2; while(f > 82) f /= 2; f *= oct > 0 ? oct : 1; dur = borne(dur, .1, 2.6);
  double de = j->subF; j->subF = f;
  VoixJ *V = jgVoix(s, VJ_ANA, BJ_BASSE, t, dur); V->f = f; int gl = glisse && de > 20 && fabs(de/f - 1) > .01; V->f0 = gl ? de : 0; V->dur = dur; V->niv = niv;
  V->fr[0] = pow(2, (-5 + (h1(t*13.1) - .5)*3)/1200); V->fr[1] = pow(2, (5 + (h1(t*7.7) - .5)*3)/1200);
  waNeuf(&V->q[0], WA_BAS, 900, .54, 0, s->son->sr); waNeuf(&V->q[1], WA_BAS, 900, 1.1, 0, s->son->sr);
}
/* la nappe : un accord, deux triangles désaccordés par note, qui monte lentement et s'éteint lentement, au loin */
static void jgNappe(Syn *s, double t, const int *notes, int nn, double dur, double niv, int grave){
  JG *j = s->son->hc->jg; dur = borne(dur, .5, 8); double base = JG_G0*(grave ? 4 : 8)*j->transpo;
  VoixJ *V = jgVoix(s, VJ_NAPPE, BJ_MEL, t, dur); V->dur = dur; V->niv = .012*(niv > 0 ? niv : 1); V->n = 0; V->sRev = .45;
  for(int q = 0; q < nn && q < 4; q++) for(int d = 0; d < 2; d++){ V->fr[V->n] = base*pow(2, notes[q]/12.0)*pow(2, (d ? 6 : -6)/1200.0); V->ph[V->n] = h1(t*3.1 + V->n); V->n++; }
  waNeuf(&V->q[0], WA_BAS, 1800, .5, 0, s->son->sr);
}
/* l'accord : un piano numérique fait de sinusoïdes ; le rhodes de l'atmosphérique, le piano de rave du jump-up */
static void jgAccord(Syn *s, double t, double r, const int *notes, int nn, double dur, double niv){
  JG *j = s->son->hc->jg; static const int M9[5] = {0, 3, 7, 10, 14}; if(!nn){ notes = M9; nn = 5; } dur = borne(dur, .12, 2.4);
  double base = JG_G0*4*j->transpo*pow(2, r/12.0); static const double AM[4] = {.0075, .0045, .0024, .0012}, TA[4] = {.3, .15, .08, .05};
  VoixJ *V = jgVoix(s, VJ_ACCORD, BJ_MEL, t, dur + .04); V->dur = dur; V->niv = niv > 0 ? niv : 1; V->sEcho = .3; V->n = 0;
  for(int q = 0; q < nn && q < 5; q++) for(int h = 0; h < 4; h++){ double f = base*pow(2, notes[q]/12.0)*(h + 1)*(h ? 1.0015 : pow(2, (q % 2 ? 4 : -4)/1200.0));
    V->fr[V->n] = f; V->amp[V->n] = AM[h]; V->dec[V->n] = TA[h]; V->n++; }
}
/* le skank : un accord pincé, très bref, que l'écho du dub fait rebondir */
static void jgSkank(Syn *s, double t, const int *notes){
  JG *j = s->son->hc->jg; double base = JG_G0*8*j->transpo; VoixJ *V = jgVoix(s, VJ_SKANK, BJ_MEL, t, .1); V->sEcho = .45; V->n = 3;
  for(int q = 0; q < 3; q++) V->fr[q] = base*pow(2, notes[q]/12.0);
}
/* la cloche du breakdown : une sinusoïde et deux partiels inharmoniques */
static void jgCloche(Syn *s, double t, double f, double dur){ VoixJ *V = jgVoix(s, VJ_CLOCHE, BJ_MEL, t, mn2(2, dur + .6)); V->f = f; V->sEcho = .2; V->sRev = .3; }

/* un pas : on programme ses sons à l'heure t (le pas dure dPas/vit) */
static void grooveJungle(Syn *s, int st, double t){
  Son *so = s->son; HC *h = so->hc; JG *j = h->jg;
  PasJ e = jgSeq(h->graine, st, j->finPiste); int p = e.p;
  double vit = e.vit > 0 ? e.vit : 1, dPas = so->dPas/vit; j->vitPas = vit;
  h->nPas++; { int q = h->nPas % 64; h->tPas[q] = t; h->stPas[q] = st; }
  j->transpo = pow(2, e.cle/12.0);
  if(p == 0) jgAuto(j, t, AJ_GAIN, 1.05*e.gain, 1.2, 0);
  jgAuto(j, t, AJ_ETIME, dPas*3, .08, 0); jgAuto(j, t, AJ_FB, e.dub ? .62 : .42, .05, 0);
  jgAuto(j, t, AJ_EG, e.montee && e.etage == 7 ? .75 : e.dub ? .9 : e.bloc == JB_BREAKDOWN ? .32 : e.fin ? .4 : e.section == HS_CHUTE ? .08 : 0, .03, 0);
  jgAuto(j, t, AJ_LP, e.filtre < 1 ? 280*pow(20000.0/280, e.filtre) : 20000, .03, 0);
  jgAuto(j, t, AJ_HP, e.montee ? 20*pow(21, mn2(1, e.prog*1.08)) : 18, e.montee ? .03 : .001, 0);
  double tb = t + ((p & 1) && e.swing > 0 ? dPas*e.swing : 0);
  /* la seconde platine */
  if(e.deck2){ double y = e.entre ? e.mixX/.5 : (e.mixX - .5)/.5, hp = e.entre ? 30 + 620*pow(1 - y, 2) : 30 + 720*y*y, g = e.entre ? .3 + .62*y : .92*(1 - y);
    jgAuto(j, t, AJ_D2HP, hp, .05, 0); jgAuto(j, t, AJ_D2G, g, .05, 0); if(e.tr2.on) jgTranche(s, e.tr2, tb, dPas, 1, 0, 0, BJ_DECK2); }
  else jgAuto(j, t, AJ_D2G, 0, .05, 0);
  /* la batterie */
  if(e.tr.on) jgTranche(s, e.tr, tb, dPas, e.mordant, e.jet, e.dub, BJ_BRK);
  if(e.trou){ jgAuto(j, t, AJ_BASSE, 0, .006, 0); jgAuto(j, t + dPas*(16 - p), AJ_BASSE, .8, .004, 0); }
  #define ESQUIVE(T) do{ jgAuto(j, (T), AJ_BASSE, .8*.6, 0, 1); jgAuto(j, (T) + .01, AJ_BASSE, .8, .045, 0); }while(0)
  if(e.kick && e.renfort){ jgEch(s, JS_K_X0X, t, e.section == HS_CHUTE ? .8 : .6, 1, 0, BJ_KIT, 0); ESQUIVE(t); }
  if(e.c200) jgEch(s, JS_SN_X0X, tb, .4, 1, .03, BJ_KIT, 0);
  if(e.kit && !e.pause){
    if(e.kitK){ jgEch(s, JS_K_X0X, tb, .85, 1, 0, BJ_KIT, 0); jgEch(s, JS_K_MC, tb, .32, 1, 0, BJ_KIT, 0); ESQUIVE(tb); }
    if(e.kitS > 0){ jgEch(s, JS_SN_X0X, tb, .5*e.kitS, 1, .03, BJ_KIT, 0); jgEch(s, JS_SN_A, tb, .42*e.kitS, 1, .05, BJ_KIT, .06*e.kitS); }
    if(e.kitH > 0){ int son = e.kitTamb ? (p % 4 == 2 ? JS_TAMB_O : JS_TAMB) : (p % 8 == 6 && (e.k & 1) ? JS_HX_O : JS_HX_F);
      jgEch(s, son, tb, e.kitH*(e.kitTamb ? 1.05 : .8), 1, (p & 2) ? .2 : -.12, BJ_KIT, 0); } }
  #undef ESQUIVE
  /* la basse, les accords, la nappe, le skank */
  if(e.bj && !e.pause){ double fb = JG_G0*j->transpo*pow(2, e.bjN/12.0);
    if(e.bjV == GR_ANALOG) jgAna(s, tb, fb, dPas*e.bjD*.97, .68, e.bjO, e.bjG); else jgSub(s, tb, fb, dPas*e.bjD*.97, .7, e.bjV == GR_DUB, e.bjO, e.bjG, dPas); }
  if(e.rave && !e.pause) jgAccord(s, tb, e.raveR, e.nRave ? e.raveN : NULL, e.nRave, dPas*e.raveDur, e.raveNiv);
  if(e.nappe) jgNappe(s, t, e.nappeN, e.nNappe, dPas*e.nappeDur, e.nappeNiv, e.nappeGrave);
  if(e.skank) jgSkank(s, tb, e.skankN);
  if(e.mel && !e.pause) jgCloche(s, t, JG_G0*8*j->transpo*pow(2, e.melN/12.0), dPas*e.melD);
  if(e.boum){ double f = JG_G0*j->transpo; while(f < 40) f *= 2; while(f > 60) f /= 2; jgEch(s, JS_K_808, t, .85, f/48, 0, BJ_BASSE, 0); }
  if(e.china && !e.impact) jgEch(s, JS_CRASH_HC, t, .22, 1, .25, BJ_KIT, .15);
  if(e.impact) jgEch(s, JS_CRASH_HC, t, .38, 1, -.25, BJ_KIT, .2);
}
static void jgSuivre(double *v, double c, double tau, double sr){ *v += (c - *v)*(1 - exp(-1/(sr*mx2(tau, 1e-4)))); }
static inline double jgComp(double *g, double niv, double seuil, double rapport, double att, double rel, double sr){
  double dB = niv > 1e-6 ? 20*log10(niv) : -120, o = dB > seuil ? seuil + (dB - seuil)/rapport : dB, c = pow(10, (o - dB)/20);
  double tau = c < *g ? att : rel; *g += (c - *g)*(1 - exp(-1/(sr*tau))); return *g;
}
static inline double jgLire(const JgSon *S, double pos, int inv){ long i0 = (long)floor(pos); double fr = pos - i0; if(i0 < 0 || i0 >= S->n - 1) return 0;
  return (S->d[i0]*(1 - fr) + S->d[i0 + 1]*fr)/32768.0; }
/* un échantillon du moteur : grave (mono), le reste en stéréo (et l'ambiance à l'arrière), les espaces dans h->fxo */
static void jgEchantillon(Syn *s, double t, double *grave, double aigu[4], int surround){
  Son *so = s->son; HC *h = so->hc; JG *j = h->jg; double sr = so->sr; long ech = so->nEch + so->iBloc;
  double brk[2] = {0}, d2[2] = {0}, kit[2] = {0}, basse = 0, mel[2] = {0}, rev[2] = {0}, eco[2] = {0}, ecoD[2] = {0};
  for(int i = 0; i < NVJ; i++){
    VoixJ *v = &j->v[i]; if(!v->actif) continue; if(ech < v->debut) continue; if(ech >= v->fin){ v->actif = 0; continue; }
    double tl = (ech - v->debut)/sr, x = 0, xl, xr;
    switch(v->type){
      case VJ_TRANCHE: case VJ_ECH: { const JgSon *S = &JG_SONS[v->son];
        if(v->pos < v->n0 || v->pos > v->n1){ v->actif = 0; continue; }
        x = jgLire(S, v->pos, v->inv); v->pos += v->pas;
        double g = v->niv*mn2(1, tl/v->att)*mn2(1, (v->fin - ech)/(sr*v->rel)); x *= g; break; }
      case VJ_SUB: { double fq = tl < v->tg ? v->f0*pow(v->f/v->f0, tl/v->tg) : v->f;
        double o = sin(2*PI*v->ph[0]) + .14*sin(2*PI*v->ph[1]); v->ph[0] += fq/sr; v->ph[0] -= floor(v->ph[0]); v->ph[1] += 2*fq/sr; v->ph[1] -= floor(v->ph[1]);
        o = biquad(&v->q[0], 0, o);
        double a = tl < .005 ? tl/.005*v->niv : v->dub ? v->niv : (tl < .08 ? v->niv : v->niv*(.85 + .15*exp(-(tl - .08)/.4)));
        if(tl > v->dur - .04) a *= borne((v->dur - tl)/.04, 0, 1);
        if(v->dub){ a *= .55 + .45*sin(2*PI*v->lfoF*tl); }
        x = o*a; break; }
      case VJ_ANA: { double fq = v->f0 > 0 && tl < .075 ? v->f0*pow(v->f/v->f0, tl/.075) : v->f, s2 = 0;
        for(int q = 0; q < 2; q++){ double f = fq*v->fr[q], dt = f/sr; s2 += polyScie(v->ph[q], dt); v->ph[q] += dt; v->ph[q] -= floor(v->ph[q]); }
        if((v->compte++ & 15) == 0){ double fc = tl < mn2(.32, v->dur*.6) ? 900*pow(240.0/900, tl/mn2(.32, v->dur*.6)) : 240;
          for(int q = 0; q < 2; q++){ Biquad b = v->q[q]; waRegler(&v->q[q], WA_BAS, fc, q ? 1.1 : .54, 0, sr); memcpy(v->q[q].z1, b.z1, sizeof b.z1); memcpy(v->q[q].z2, b.z2, sizeof b.z2); } }
        s2 = biquad(&v->q[1], 0, biquad(&v->q[0], 0, s2));
        double as = tl < .006 ? tl/.006*v->niv*.3 : tl < .05 ? v->niv*.3 : v->niv*(.22 + .08*exp(-(tl - .05)/.2));
        double fs = v->f0 > 0 && tl < .075 ? fq : (tl < .04 ? v->f*1.8*pow(1/1.8, tl/.04) : v->f), so2 = sin(2*PI*v->ph[2]); v->ph[2] += fs/sr; v->ph[2] -= floor(v->ph[2]);
        double ab = tl < .005 ? tl/.005*v->niv : v->niv*(.88 + .12*exp(-(tl - .08)/.4));
        double fin = tl > v->dur - .035 ? borne((v->dur - tl)/.035, 0, 1) : 1; x = (s2*as + so2*ab)*fin; break; }
      case VJ_NAPPE: { double s8 = 0; for(int q = 0; q < v->n; q++){ double ph = v->ph[q]; s8 += 4*fabs(ph - .5) - 1; v->ph[q] += v->fr[q]/sr; v->ph[q] -= floor(v->ph[q]); }
        if((v->compte++ & 31) == 0){ double d = v->dur, fc = tl < d*.5 ? 1800 + (6500 - 1800)*tl/(d*.5) : 6500 + (3000 - 6500)*(tl - d*.5)/(d*.5);
          Biquad b = v->q[0]; waRegler(&v->q[0], WA_BAS, fc, .5, 0, sr); memcpy(v->q[0].z1, b.z1, sizeof b.z1); memcpy(v->q[0].z2, b.z2, sizeof b.z2); }
        double ta = mn2(1.2, v->dur*.3), a = tl < ta ? tl/ta : tl < v->dur*.75 ? 1 : borne((v->dur - tl)/(v->dur*.25), 0, 1);
        x = biquad(&v->q[0], 0, s8)*a*v->niv; break; }
      case VJ_ACCORD: { double s5 = 0;
        for(int q = 0; q < v->n; q++){ double a = tl < .002 ? tl/.002 : exp(-(tl - .004)/v->dec[q]); s5 += sin(2*PI*v->ph[q])*v->amp[q]*a; v->ph[q] += v->fr[q]/sr; v->ph[q] -= floor(v->ph[q]); }
        double fin = tl > v->dur ? borne(1 - (tl - v->dur)/.03, 0, 1) : 1; x = s5*v->niv*fin; break; }
      case VJ_SKANK: { double s3 = 0; for(int q = 0; q < 3; q++){ double ph = v->ph[q]; s3 += q ? sin(2*PI*ph) : 4*fabs(ph - .5) - 1; v->ph[q] += v->fr[q]/sr; v->ph[q] -= floor(v->ph[q]); }
        double a = tl < .003 ? tl/.003*.08 : .08*pow(.001/.08, (tl - .003)/.087); x = s3*a; break; }
      case VJ_CLOCHE: { double s3 = sin(2*PI*v->ph[0]) + .3*sin(2*PI*v->ph[1]) + .1*sin(2*PI*v->ph[2]); v->ph[0] += v->f/sr; v->ph[1] += v->f*2.76/sr; v->ph[2] += v->f*5.4/sr;
        for(int q = 0; q < 3; q++) v->ph[q] -= floor(v->ph[q]); x = s3*.11*(tl < .003 ? tl/.003 : exp(-(tl - .003)/.5)); break; }
    }
    double pp = (borne(v->pan, -1, 1) + 1)/2; xl = x*cos(pp*PI/2)*1.41; xr = x*sin(pp*PI/2)*1.41;
    switch(v->bus){
      case BJ_BRK: brk[0] += xl; brk[1] += xr; break;
      case BJ_DECK2: d2[0] += xl; d2[1] += xr; break;
      case BJ_KIT: kit[0] += xl; kit[1] += xr; break;
      case BJ_BASSE: basse += x; break;
      default: mel[0] += xl; mel[1] += xr; break;
    }
    if(v->sRev > 0){ rev[0] += xl*v->sRev; rev[1] += xr*v->sRev; }
    if(v->sEcho > 0){ if(v->sEcho > .85){ ecoD[0] += xl*v->sEcho; ecoD[1] += xr*v->sEcho; } else { eco[0] += xl*v->sEcho; eco[1] += xr*v->sEcho; } }   /* le coup jeté passe toujours */
  }
  /* les automations */
  while(j->nEv > 0 && j->ev[0].t <= t){ Evenement *q = &j->ev[0]; j->aC[q->k] = q->v; j->aT[q->k] = q->tau > 0 ? q->tau : .0005; if(q->saut) j->aV[q->k] = q->v; memmove(j->ev, j->ev + 1, sizeof(Evenement)*(--j->nEv)); }
  for(int k = 0; k < NAJ; k++) jgSuivre(&j->aV[k], j->aC[k], j->aT[k], sr);
  int maj = (j->compte++ & 31) == 0;
  #define MAJ(B, TYPE, F, Q) do{ Biquad b_ = (B); waRegler(&(B), TYPE, F, Q, 0, sr); memcpy((B).z1, b_.z1, sizeof b_.z1); memcpy((B).z2, b_.z2, sizeof b_.z2); }while(0)
  if(maj && fabs(j->aV[AJ_D2HP] - j->d2Cour) > 1){ j->d2Cour = j->aV[AJ_D2HP]; MAJ(j->d2Hp, WA_HAUT, j->d2Cour, -3); }
  /* la seconde platine, puis le bus du break : le grave plus rond, sans carton, des aigus doux */
  double g2 = j->aV[AJ_D2G];
  for(int c = 0; c < 2; c++){ double x = brk[c] + biquad(&j->d2Hp, c, d2[c])*g2;
    x = biquad(&j->brkAir, c, biquad(&j->brkBoue, c, biquad(&j->brkCorps, c, biquad(&j->brkHp, c, x))));
    brk[c] = x*1.4 + kit[c]; }
  /* la batterie : la colle douce, la compression parallèle, la petite pièce */
  double gc = jgComp(&j->colleG, mx2(fabs(brk[0]), fabs(brk[1])), -14, 2.5, .01, .12, sr);
  double gp = jgComp(&j->paraG, mx2(fabs(brk[0]), fabs(brk[1])), -32, 6, .004, .09, sr);
  double bat[2], room[2];
  for(int c = 0; c < 2; c++){ double pa = biquad(&j->para2, c, biquad(&j->para1, c, brk[c]*gp))*.5; bat[c] = brk[c]*gc + pa; }
  { double in = biquad(&j->roomHp, 0, (brk[0] + brk[1])*.5)*.35, o[4], sum = 0;
    for(int i = 0; i < 4; i++){ o[i] = j->room[i][j->roomI[i]]; sum += o[i]; }
    for(int i = 0; i < 4; i++){ double gfb = pow(10, -3.0*j->roomN[i]/(sr*.5)), y = (o[i] - sum*.5)*gfb; j->roomLp[i] += (y - j->roomLp[i])*.5;
      j->room[i][j->roomI[i]] = (float)(j->roomLp[i] + in*(i & 1 ? -1 : 1)); if(++j->roomI[i] >= j->roomN[i]) j->roomI[i] = 0; }
    room[0] = (o[0] + o[2])*.32; room[1] = (o[1] + o[3])*.32; }
  /* la basse : sous le seuil d'écoute coupé, compression lente, l'esquive sous la grosse caisse */
  double b = biquad(&j->basseHp, 0, basse); b *= jgComp(&j->basseG, fabs(b), -16, 2, .03, .25, sr)*j->aV[AJ_BASSE];
  /* la mélodie, à part : compression douce, envois vers la salle et l'écho */
  double m[2]; { double gm = jgComp(&j->melG, mx2(fabs(mel[0]), fabs(mel[1])), -22, 1.8, .03, .3, sr);
    for(int c = 0; c < 2; c++){ double x = biquad(&j->melHp, c, mel[c]); m[c] = x*gm*4.5; rev[c] += x*4.5*.5; eco[c] += x*4.5*.35; } }
  /* l'écho ping-pong, à la croche pointée */
  double eL, eR;
  { double dt = borne(j->aV[AJ_ETIME], .02, 1)*sr, lu = j->echI - dt; while(lu < 0) lu += j->echN; int i0 = (int)lu, i1 = (i0 + 1) % j->echN; double fr = lu - i0;
    eL = j->ech[0][i0]*(1 - fr) + j->ech[0][i1]*fr; eR = j->ech[1][i0]*(1 - fr) + j->ech[1][i1]*fr;
    double in = biquad(&j->echHp, 0, (eco[0] + eco[1])*.5*j->aV[AJ_EG] + (ecoD[0] + ecoD[1])*.5);
    j->ech[0][j->echI] = (float)(in + biquad(&j->echLp, 0, eR)*j->aV[AJ_FB]); j->ech[1][j->echI] = (float)eL; if(++j->echI >= j->echN) j->echI = 0;
    eL *= .5; eR *= .5; }
  /* la grande salle (environ 2,6 s), pour les nappes, les accords et les coups jetés */
  double hall[2];
  { double in = biquad(&j->revHp, 0, (rev[0] + rev[1])*.5 + (eL + eR)*.15)*.3, o[4], sum = 0;
    for(int i = 0; i < 4; i++){ o[i] = j->hall[i][j->hallI[i]]; sum += o[i]; }
    for(int i = 0; i < 4; i++){ double gfb = pow(10, -3.0*j->hallN[i]/(sr*2.6)), y = (o[i] - sum*.5)*gfb; j->hallLp[i] += (y - j->hallLp[i])*.45;
      j->hall[i][j->hallI[i]] = (float)(j->hallLp[i] + in*(i & 1 ? -1 : 1)); if(++j->hallI[i] >= j->hallN[i]) j->hallI[i] = 0; }
    hall[0] = (o[0] + o[2])*.55; hall[1] = (o[1] + o[3])*.55; }
  /* la somme ; le gain du DJ par partie ; les filtres de la montée et des genres ; un limiteur doux */
  double gS = .8*j->aV[AJ_GAIN];
  double L = (bat[0] + room[0] + m[0])*gS, R = (bat[1] + room[1] + m[1])*gS, Bm = b*gS, fxL = (eL + hall[0])*gS, fxR = (eR + hall[1])*gS;
  if(maj){ double lf = j->aV[AJ_LP], hf = j->aV[AJ_HP];
    if(fabs(lf - j->lpCour) > lf*.01){ j->lpCour = lf; for(int c = 0; c < 3; c++) MAJ(j->lpM[c], WA_BAS, lf, -6); }
    if(fabs(hf - j->hpCour) > 1){ j->hpCour = hf; for(int c = 0; c < 3; c++) MAJ(j->hpM[c], WA_HAUT, hf, -3); } }
  #undef MAJ
  if(j->lpCour < 19000){ L = biquad(&j->lpM[0], 0, L); R = biquad(&j->lpM[1], 0, R); Bm = biquad(&j->lpM[2], 0, Bm); }
  if(j->hpCour > 22){ L = biquad(&j->hpM[0], 0, L); R = biquad(&j->hpM[1], 0, R); Bm = biquad(&j->hpM[2], 0, Bm); }
  { double pk = mx2(fabs(L + Bm + fxL), fabs(R + Bm + fxR)), c = pk > .9 ? .9/pk : 1; if(c < j->limG) j->limG = c; else j->limG += (c - j->limG)*(1 - exp(-1/(sr*.08))); }   /* attaque immédiate : jamais de dépassement */
  double G = j->limG;
  *grave = Bm*G; aigu[0] = L*G; aigu[1] = R*G; aigu[2] = aigu[3] = 0;
  h->fxo[0] = fxL*G; h->fxo[1] = fxR*G; h->fxo[2] = surround ? fxL*G*.9 : 0; h->fxo[3] = surround ? fxR*G*.9 : 0;
}
/* pour l'image : l'effet du pas qui sonne — l'explosion éclaire, le silence noircit ; le reste, rarement, sur les temps */
static int jgEffet(HC *h, int st, double u){
  PasJ e = jgSeq(h->graine, st, h->jg->finPiste);
  if(e.impact) return 1;   /* EF_FLASH */
  if(e.pause && !e.trou) return 2;   /* EF_NOIR */
  if(e.montee && e.caisse) return e.etage >= 6 ? ((st & 1) ? 3 : 1) : e.etage >= 4 ? 4 : 6;
  if(e.kick && e.p == 0 && u < .45) return 5;   /* EF_ZOOM */
  if(e.caisse && e.p % 8 == 4 && u < .3) return u < .15 ? 4 : 6;   /* EF_SAUTE, EF_MIROIR */
  if(e.roll && u < .5) return 8;   /* EF_SCINTILLE */
  if(e.bj && e.p == 0 && u < .2) return 7;   /* EF_TRANCHES */
  return 0;
}
/* un bon moment pour changer d'image : le début d'un morceau, l'échange des basses, l'explosion d'une chute */
static int jgMoment(HC *h, int st){ PasJ e = jgSeq(h->graine, st, h->jg->finPiste); return e.debutPiste || e.impact; }
