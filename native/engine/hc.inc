/* Synchromy — Hardcore, version mathcore. Port de la version web (novembre 2026) :
   un trio guitare, basse, batterie à l'unisson ; des riffs comptés en groupes irréguliers de doubles croches ; la polymétrie ;
   des surprises (blast, arrêts net, breakdown, décalage, triolet) ; une descente qui ferme le haut du spectre ; une montée
   qui finit en blast, une demi-mesure de silence aspirée, puis une explosion (souffle, sous-basses, éclats métalliques,
   cri de foule) qui ouvre grand la réverbération et l'écho. Un arc sur deux, l'émeute berlinoise : un break passé à
   l'échantillonneur 8 bits, la guitare hachée, la foule, la sirène, la puce sonore.
   La partition (hcSeq) est la même pour le son et pour l'image : chaque saute de l'image tombe sur un coup. */

/* ================= partition ================= */
enum { HM_MOTIF, HM_MOTIFFIN, HM_VARIE, HM_CASSE, HM_BEGAIE, HM_STOP, HM_BLAST, HM_BREAKDOWN, HM_DECALE, HM_TRIOLET, HM_RALENTI,
       HM_CARRE, HM_CROCHES, HM_RAFALE, HM_RELANCE, HM_GALOP };
enum { HS_FOND, HS_COMPLEXE, HS_MONTEE, HS_CHUTE };
enum { PH_ETABLI, PH_VARIATION, PH_SURPRISE, PH_DESCENTE, PH_MONTEE };
enum { HP_A, HP_B, HP_C, HP_D, NHP };   /* quatre graines de départ (l'ancien choix de préréglage) */
static const double GAMME_HC[8] = { 43.65, 46.25, 51.91, 58.27, 65.41, 77.78, 87.31, 103.83 };   /* fa phrygien */

typedef struct { int pos, palier, section, phase, m; double pression; } ArcHC;
/* l'arc, sur 32 mesures : structure nette (8), variations (4), surprises (2), descente (2), montée (8),
   retour du riff à pleine force (6), descente (2) */
static ArcHC hcArc(int b){
  ArcHC A; A.pos = ((b % 32) + 32) % 32; A.palier = (b/32) % 4; A.m = A.pos - 16; int pos = A.pos;
  A.section = pos < 8 ? HS_FOND : pos < 16 ? HS_COMPLEXE : pos < 24 ? HS_MONTEE : HS_CHUTE;
  A.phase = pos < 8 ? PH_ETABLI : pos < 12 ? PH_VARIATION : pos < 14 ? PH_SURPRISE : pos < 16 ? PH_DESCENTE : pos < 24 ? PH_MONTEE : pos < 30 ? PH_ETABLI : PH_DESCENTE;
  A.pression = A.phase == PH_DESCENTE ? (pos < 16 ? .5 : .78) : A.section == HS_FOND ? .34 + .06*A.palier : A.section == HS_COMPLEXE ? .5 + .06*A.palier
             : A.section == HS_MONTEE ? .55 + .45*A.m/8.0 : 1;
  return A;
}
static int hcMesure(int b, double g){
  ArcHC A = hcArc(b); double u = h1(b*7.13 + g);
  if(A.phase == PH_MONTEE) return A.m < 4 ? HM_CARRE : A.m < 6 ? HM_CROCHES : A.m < 7 ? HM_RAFALE : HM_RELANCE;
  if(A.phase == PH_DESCENTE) return HM_RALENTI;
  if(A.phase == PH_SURPRISE){ static const int SU[5] = { HM_BLAST, HM_STOP, HM_BREAKDOWN, HM_DECALE, HM_TRIOLET };
    int i = (int)(h1((b/32)*5.7 + g)*5); return SU[(i + (A.pos - 12)*2) % 5]; }
  if(A.phase == PH_VARIATION) return A.pos == 11 ? (u < .5 ? HM_CASSE : HM_BEGAIE) : HM_VARIE;
  return A.pos == 7 || A.pos == 27 || A.pos == 29 ? HM_MOTIFFIN : HM_MOTIF;
}
/* les riffs : des groupes de doubles croches, et le degré (en demi-tons) de l'accord qui ouvre chaque groupe */
typedef struct { int n, g[8], d[8]; } RiffM;
static const RiffM MATH_RIFFS[6] = {
  { 6, {3,3,3,3,2,2},   {0, 1, 0, 6, 0, 11} },
  { 6, {3,3,2,3,3,2},   {0, 0, 1, 0, 0, 6} },
  { 4, {5,5,3,3},       {0, 6, 1, 7} },
  { 7, {2,3,2,3,2,2,2}, {0, 1, 0, 6, 0, 5, 4} },
  { 3, {7,7,2},         {0, 1, 11} },
  { 6, {3,2,3,2,3,3},   {0, 6, 0, 5, 1, 0} } };
typedef struct { int n, g[6]; } PolyM;
static const PolyM MATH_POLY[5] = { {3, {3,2,2}}, {4, {3,3,3,2}}, {5, {3,3,2,3,2}}, {2, {2,3}}, {3, {5,3,3}} };
/* le break de l'émeute : grosse caisse, caisse claire (x) et notes fantômes (o) */
static const char *BREAKS_HC[3][4] = {
  { "x.x.......xx....", "x.x......x.x....", "....x..o.o..x..o", "....x..o.o..x.x." },
  { "x..x..x...x.....", "x..x......x..x..", "....x...o...x...", "....x..o..o.x.oo" },
  { "x.....x.x.......", "x.x...x...x..x..", "....x..o....x.o.", "..o.x..o....x..." } };
static inline int mathDiss(int d){ int r = ((d % 12) + 12) % 12; return r == 1 || r == 6 || r == 11; }
typedef struct { int k, gi, gl; } PasM;
static PasM mathPas(const int *gr, int n, int i){
  int L = 0; for(int j = 0; j < n; j++) L += gr[j];
  int r = ((i % L) + L) % L, gi = 0; while(r >= gr[gi]){ r -= gr[gi]; gi++; }
  PasM P = { r, gi, gr[gi] }; return P;
}
typedef struct {
  int p, b, sorte, kick, caisse, rat, roll, china, ride, hat, toms, gM, gD, gTrem, basse, bD, pause, accent,
      montee, descente, descDebut, emeute, breakbeat, ghost, hache, section, phase, palier, m, impact, riser, clap, rupture, cri;
  double gDur, prog, dprog, pression;
} PasHC;
static void mGroupe(PasHC *e, PasM P0, int d, int fort, int alterne){
  if(P0.k == 0){ e->kick = 1; e->accent = 1; e->gM = mathDiss(d) ? 'x' : 'o'; e->gD = d; e->gDur = P0.gl*.92; e->basse = 1; e->bD = d;
    if(alterne ? P0.gi % 2 == 1 : P0.gi == 0) e->caisse = 1;
    if(fort || P0.gi % 2 == 0) e->china = fort || P0.gi == 0; }
  else { e->kick = P0.k % 2 == 1 || fort; e->gM = 'm'; e->gD = 0; e->gDur = 1; e->ride = e->p % 2 == 0; }
}
static PasHC hcSeq(int st, int preset, double g){
  (void)preset; PasHC E; memset(&E, 0, sizeof E); PasHC *e = &E;
  int p = st % 16, b = st/16, mi = b & 1, cyc = b/32; ArcHC A = hcArc(b);
  int sorte = hcMesure(b, g);
  if(A.section == HS_CHUTE && (A.pos == 28 || A.pos == 29)) sorte = HM_BREAKDOWN;   /* la chute finit en breakdown */
  const RiffM *RF = &MATH_RIFFS[(int)(h1(cyc*3.1 + g)*6)];
  e->p = p; e->b = b; e->sorte = sorte; e->rat = 1;
  int montee = A.phase == PH_MONTEE; double prog = montee ? (A.m*16 + p)/128.0 : 0;
  int descente = sorte == HM_RALENTI; double dprog = descente ? ((A.pos % 2)*16 + p)/32.0 : 0;
  if(sorte == HM_MOTIF || sorte == HM_MOTIFFIN){
    PasM P0 = mathPas(RF->g, RF->n, p); int d = RF->d[P0.gi] + (mi && P0.gi == RF->n - 1 ? 1 : 0);
    mGroupe(e, P0, d, A.section == HS_CHUTE, 1);
    if(A.section == HS_FOND && A.pos < 2 && P0.k > 0) e->kick = 0;               /* l'entrée : la guitare seule, puis la batterie */
    if(sorte == HM_MOTIFFIN && p >= 12){ e->gM = p % 2 == 0 ? 'x' : 0; e->gD = p - 12; e->gDur = 1.8; e->kick = p % 2 == 0; e->caisse = 0; e->toms = 1; e->china = p == 12; }
  }
  else if(sorte == HM_VARIE){ const PolyM *G = &MATH_POLY[(int)(h1(cyc*5.3 + g)*5)]; PasM P0 = mathPas(G->g, G->n, (A.pos - 8)*16 + p);
    static const int DV[5] = {0, 1, 6, 0, 5}; mGroupe(e, P0, DV[P0.gi % 5], 0, 0);
    if(A.pos == 10 && p >= 12){ e->kick = e->caisse = 1; e->rat = 2; e->gM = 'x'; e->gD = p - 12; e->gDur = 1; } }
  else if(sorte == HM_CASSE || sorte == HM_BEGAIE || sorte == HM_STOP){   /* l'arrêt net : des coups à l'unisson, des trous */
    static const int H_STOP[5] = {0, 3, 7, 10, 13}, H_CASSE[7] = {0, 2, 5, 8, 9, 12, 14}, H_BEG[10] = {0, 1, 2, 6, 7, 8, 12, 13, 14, 15};
    static const int DH[10] = {0, 1, 6, 11, 5, 0, 7, 1, 6, 0};
    const int *h = sorte == HM_STOP ? H_STOP : sorte == HM_CASSE ? H_CASSE : H_BEG; int n = sorte == HM_STOP ? 5 : sorte == HM_CASSE ? 7 : 10, idx = -1;
    for(int j = 0; j < n; j++) if(h[j] == p) idx = j;
    if(idx >= 0){ e->kick = e->caisse = e->china = e->accent = 1; int d = DH[idx]; e->gM = mathDiss(d) ? 'x' : 'o'; e->gD = d; e->gDur = 1.2; e->basse = 1; e->bD = d; }
    else e->pause = sorte == HM_STOP; }
  else if(sorte == HM_BLAST){ static const int DB[4] = {0, 1, 6, 5};
    e->kick = p % 2 == 0; e->caisse = p % 2 == 1; e->china = p % 4 == 0; e->gM = 'x'; e->gD = DB[p >> 2]; e->gDur = .5; e->gTrem = 1;
    if(p % 4 == 0){ e->basse = 1; e->bD = DB[p >> 2]; } }
  else if(sorte == HM_BREAKDOWN){ static const int GB[5] = {3, 3, 3, 3, 4}; PasM P0 = mathPas(GB, 5, p);   /* demi-temps, sourdines en groupes de trois */
    e->caisse = p == 8; e->china = p == 0 || p == 8;
    if(P0.k == 0){ e->kick = e->accent = 1; e->gM = 'o'; e->gD = mi && P0.gi == 4 ? 1 : 0; e->gDur = P0.gl*.9; e->basse = 1; e->bD = 0; }
    else if(P0.k == 1){ e->gM = 'm'; e->gD = 0; e->gDur = 1; e->kick = 1; } }
  else if(sorte == HM_DECALE){ PasM P0 = mathPas(RF->g, RF->n, p + 15); mGroupe(e, P0, RF->d[P0.gi], 0, 1); }
  else if(sorte == HM_TRIOLET){ static const int G3[1] = {3}, DT[6] = {0, 6, 1, 0, 11, 6}; PasM P0 = mathPas(G3, 1, p); mGroupe(e, P0, DT[P0.gi % 6], 0, 1); }
  else if(descente){   /* la frappe ralentit, la guitare descend */
    static const int GA[7] = {2, 2, 2, 2, 2, 3, 3}, GBB[3] = {4, 5, 7}; int pair = A.pos % 2 == 0; PasM P0 = pair ? mathPas(GA, 7, p) : mathPas(GBB, 3, p); int base = pair ? 0 : -7;
    if(P0.k == 0){ e->kick = 1; e->caisse = P0.gi % 2 == 1; e->gM = 'x'; e->gD = base - P0.gi; e->gDur = P0.gl*.95; e->basse = 1; e->bD = base - P0.gi; }
    else if(p % 2 == 0) e->ride = 1; }
  else if(montee){   /* la montée : du trémolo en croches au blast */
    int d = A.m;
    if(sorte == HM_RELANCE){ if(p < 8){ e->kick = e->caisse = e->accent = 1; e->gM = 'o'; e->gD = 7 + p; e->gDur = .9; e->basse = 1; e->bD = 7 + p; } else e->pause = 1; }
    else if(A.m < 2){ e->kick = p % 2 == 0; e->caisse = p == 4 || p == 12; e->gM = 's'; e->gD = d; e->gDur = .5; e->gTrem = 1; e->hat = p % 2 == 0; if(p % 4 == 0){ e->basse = 1; e->bD = d; } }
    else if(A.m < 4){ e->kick = 1; e->caisse = p % 4 == 2; e->gM = 'o'; e->gD = d; e->gDur = .5; e->gTrem = 1; e->ride = p % 2 == 0; if(p % 4 == 0){ e->basse = 1; e->bD = d; } }
    else { e->kick = p % 2 == 0; e->caisse = p % 2 == 1; e->rat = A.m == 6 && p >= 12 ? 2 : 1; e->china = p % 4 == 0; e->gM = 'x'; e->gD = d; e->gDur = .5; e->gTrem = 1; if(p % 4 == 0){ e->basse = 1; e->bD = d; } } }
  /* l'émeute, un arc sur deux : le break remplace la batterie dans les structures établies et leurs variations */
  int emeute = ((cyc + (int)g) & 1) == 1, brk = emeute && (sorte == HM_MOTIF || sorte == HM_MOTIFFIN || sorte == HM_VARIE);
  if(brk){ int k = (int)(h1(cyc*2.7 + g)*3); const char *K = BREAKS_HC[k][mi], *S = BREAKS_HC[k][2 + mi];
    e->kick = K[p] == 'x'; e->caisse = S[p] == 'x'; e->ghost = S[p] == 'o'; e->china = 0;
    if(p >= 12 && sorte != HM_MOTIF){ e->kick = p == 12; e->caisse = 1; e->rat = p >= 14 ? 3 : 2; } }
  e->roll = e->rat > 1;
  e->montee = montee; e->prog = prog; e->descente = descente; e->dprog = dprog; e->descDebut = descente && A.pos % 2 == 0 && p == 0;
  e->emeute = emeute; e->breakbeat = brk; e->hache = emeute && (sorte == HM_MOTIFFIN || sorte == HM_VARIE) && p >= 12;
  e->section = A.section; e->phase = A.phase; e->palier = A.palier; e->m = A.m;
  e->pression = montee ? .55 + .45*prog : descente ? A.pression*(1 - .45*dprog) : A.pression;
  e->impact = A.pos == 24 && p == 0; e->riser = montee && A.m == 0 && p == 0; e->clap = e->caisse; e->rupture = sorte == HM_STOP;
  return E;
}

/* ================= outils de signal (au sens de Web Audio) ================= */
enum { WA_BAS, WA_HAUT, WA_BANDE, WA_PIC, WA_TOUT };
/* les formules de la spécification Web Audio : passe-bas et passe-haut prennent leur Q en décibels, les autres en rapport */
static void waRegler(Biquad *b, int type, double f, double Q, double gdb, double sr){
  double w0 = 2*PI*borne(f, 5, sr*.49)/sr, cw = cos(w0), sw = sin(w0), A = pow(10, gdb/40), al, b0, b1, b2, a0, a1, a2;
  switch(type){
    case WA_BAS:  al = sw/(2*pow(10, Q/20)); b0 = (1 - cw)/2; b1 = 1 - cw; b2 = b0; a0 = 1 + al; a1 = -2*cw; a2 = 1 - al; break;
    case WA_HAUT: al = sw/(2*pow(10, Q/20)); b0 = (1 + cw)/2; b1 = -(1 + cw); b2 = b0; a0 = 1 + al; a1 = -2*cw; a2 = 1 - al; break;
    case WA_BANDE: al = sw/(2*Q); b0 = al; b1 = 0; b2 = -al; a0 = 1 + al; a1 = -2*cw; a2 = 1 - al; break;
    case WA_PIC:  al = sw/(2*Q); b0 = 1 + al*A; b1 = -2*cw; b2 = 1 - al*A; a0 = 1 + al/A; a1 = -2*cw; a2 = 1 - al/A; break;
    default:      al = sw/(2*Q); b0 = 1 - al; b1 = -2*cw; b2 = 1 + al; a0 = 1 + al; a1 = -2*cw; a2 = 1 - al; break;
  }
  b->b0 = b0/a0; b->b1 = b1/a0; b->b2 = b2/a0; b->a1 = a1/a0; b->a2 = a2/a0;
}
static inline void waNeuf(Biquad *b, int type, double f, double Q, double gdb, double sr){ memset(b, 0, sizeof *b); waRegler(b, type, f, Q, gdb, sr); }
/* courbes de saturation, comme les WaveShaper de la page : lecture linéaire de la table, bornée à ses extrémités */
typedef struct { float *c; int n; double cle[4]; } Courbe;
static Courbe COURBES[160]; static int nCourbes = 0;
enum { CB_REPLI, CB_DIODE, CB_SATUREE, CB_CUBIQUE, CB_TANH24 };
static const Courbe *courbeHC(int sorte, double a, double b, double c){
  for(int i = 0; i < nCourbes; i++){ Courbe *k = &COURBES[i]; if(k->cle[0] == sorte && k->cle[1] == a && k->cle[2] == b && k->cle[3] == c) return k; }
  if(nCourbes >= 160){ for(int i = 0; i < nCourbes; i++) if(COURBES[i].cle[0] == sorte) return &COURBES[i]; return &COURBES[0]; }   /* table pleine : la plus proche parente */
  Courbe *k = &COURBES[nCourbes++];
  int n = sorte == CB_REPLI || sorte == CB_CUBIQUE ? 4096 : sorte == CB_SATUREE ? 1024 : 2048;
  k->c = malloc(sizeof(float)*n); k->n = n; k->cle[0] = sorte; k->cle[1] = a; k->cle[2] = b; k->cle[3] = c;
  for(int i = 0; i < n; i++){
    double x = sorte == CB_SATUREE ? (double)i/(n - 1)*2 - 1 : i*2.0/n - 1, y;
    if(sorte == CB_REPLI){ double kk = borne(a/9, .5, 25), as = borne(b, 0, .45), f = borne(c, 0, .85), dc = tanh(kk*as);
      y = borne((tanh(kk*(x + as)) - dc)*(1 - f*.6) + sin(x*PI*(1 + f*3))*(f*.8), -.98, .98); }
    else if(sorte == CB_DIODE){ double kk = borne(a/12, .5, 16), dc = tanh(kk*.12); y = tanh(kk*(x + .12)) - dc; if(x > 0) y *= 1.08; y = borne(y, -.95, .95); }
    else if(sorte == CB_SATUREE) y = tanh(a*x)/tanh(a);
    else if(sorte == CB_CUBIQUE) y = borne((x <= -1 ? -2.0/3 : x >= 1 ? 2.0/3 : x - x*x*x/3)*1.48, -.99, .99);
    else y = tanh(x*2.4)*.95;
    k->c[i] = (float)y;
  }
  return k;
}
static inline double faconner(const Courbe *k, double x){
  if(x <= -1) return k->c[0]; if(x >= 1) return k->c[k->n - 1];
  double v = (k->n - 1)*.5*(x + 1); int i = (int)v; double f = v - i; return k->c[i] + (k->c[i + 1] - k->c[i])*f;
}
/* rampes : exponentielle et linéaire, d'un point (t0, v0) à un point (t1, v1) */
static inline double rampeE(double t, double t0, double v0, double t1, double v1){ if(t <= t0) return v0; if(t >= t1) return v1; return v0*exp(log(v1/v0)*(t - t0)/(t1 - t0)); }
static inline double rampeL(double t, double t0, double v0, double t1, double v1){ if(t <= t0) return v0; if(t >= t1) return v1; return v0 + (v1 - v0)*(t - t0)/(t1 - t0); }
static inline double blanc(Syn *s){ return alea01(s)*2 - 1; }

/* ================= voix ================= */
enum { HV_KICK, HV_CAISSE, HV_CHINA, HV_RIDE, HV_TOM, HV_HAT, HV_BHAT, HV_BKICK, HV_GTR, HV_BASSE, HV_MONTEE, HV_IMPACT,
       HV_DESC, HV_ASPI, HV_EXN, HV_BOOM, HV_ECLAT, HV_FOULE, HV_SIRENE, HV_PUCE };
/* les bus : grave (dans la colle, mono), aigus (placés, passe-haut et saturation), guitare et Atari (placés, dans la colle) */
enum { HB_COLLE, HB_AIGU, HB_GTR, HB_ATARI, HB_REV, HB_ECHO, NHB };
typedef struct {
  int actif, type, bus, mode, n; long debut, fin;
  double vel, dur, f, f2, pk, angle, haut, sRev, sEcho, vib;
  double ph[6], fr[6], phl; Biquad q[6]; const Courbe *k1; int compte;
} VoixHC;
#define NVHC 112
typedef struct { double t, v, tau; int k, saut; } Evenement;
enum { AU_REV, AU_ECHO, AU_GTR, AU_FB, AU_LP, AU_ETIME, NAU };
typedef struct HC {
  int actif, preset, transpoDemi; double graine, transpo, press; int palier, montee, emeute; double expl;
  VoixHC v[NVHC];
  Evenement hp[64]; int nHp; double hpF, hpCible, hpTau; Biquad hpM[6]; int hpCompte;   /* passe-haut de montée, un filtre par sortie */
  Evenement au[160]; int nAu; double auV[NAU], auC[NAU], auT[NAU];                      /* automations : espaces, descente */
  double colleGain; Biquad ahp[4]; const Courbe *kSat, *kClip;
  Biquad lpB, lpA[4], lpF[4]; double lpCour;                                                /* le passe-bas de la descente */
  double atH[4]; int atC;                                                                   /* l'échantillonneur 8 bits */
  float *rv[4]; int rn[4], ri[4]; double rlp[4]; Biquad rhp;                                /* réverbération : quatre lignes, matrice de Householder */
  float *ec[2]; int en, ei; double eFb; Biquad ehp, elp;                                    /* écho ping-pong */
  double fxo[4];                                                                            /* les espaces, après l'écrêteur */
  int nPas; double tPas[64]; int stPas[64];   /* journal des pas joués, pour l'image */
  struct JG *jg; int jungle;   /* make me dance : la jungle des années 90 (jg.inc) */
  Biquad lfe[2];
} HC;

static VoixHC *hcPlace(HC *h){
  for(int i = 0; i < NVHC; i++) if(!h->v[i].actif) return &h->v[i];
  long m = 0; int k = 0; for(int i = 0; i < NVHC; i++) if(h->v[i].debut < m || i == 0){ m = h->v[i].debut; k = i; }
  return &h->v[k];   /* la plus ancienne cède sa place */
}
static VoixHC *hcVoix(Syn *s, int type, int bus, double t, double duree){
  HC *h = s->son->hc; VoixHC *v = hcPlace(h); memset(v, 0, sizeof *v);
  v->actif = 1; v->type = type; v->bus = bus; v->debut = (long)(t*s->son->sr); v->fin = v->debut + (long)((duree + .03)*s->son->sr); v->angle = 0;
  return v;
}
static void hcAuto(HC *h, double t, int k, double v, double tau, int saut){
  if(h->nAu >= 160) return; int i = h->nAu++;
  while(i > 0 && h->au[i - 1].t > t){ h->au[i] = h->au[i - 1]; i--; }   /* rangés par heure */
  Evenement *q = &h->au[i]; q->t = t; q->k = k; q->v = v; q->tau = tau; q->saut = saut;
}
static inline double polyScie(double ph, double dt){
  double v = 2*ph - 1;
  if(ph < dt){ double x = ph/dt; v -= x + x - x*x - 1; } else if(ph > 1 - dt){ double x = (ph - 1)/dt; v -= x*x + x + x + 1; }
  return v;
}
static inline double pas01(double *ph, double f, double sr){ double o = *ph; *ph += f/sr; *ph -= floor(*ph); return o; }

/* ---------- la batterie ---------- */
static void hcKick(Syn *s, double t, double vel){   /* la batte qui claque, un corps court et serré */
  VoixHC *v = hcVoix(s, HV_KICK, HB_COLLE, t, .14); v->vel = vel; v->k1 = courbeHC(CB_SATUREE, 2, 0, 0); waNeuf(&v->q[0], WA_BANDE, 3800, 1.1, 0, s->son->sr);
}
static void hcCaisse(Syn *s, double t, double niv, double haut, double pan, int bus){
  VoixHC *v = hcVoix(s, HV_CAISSE, bus, t, .2); v->vel = niv; waNeuf(&v->q[0], WA_BANDE, 2200 + 900*haut, .6, 0, s->son->sr); v->angle = pan*90;
}
static void hcChina(Syn *s, double t, double niv, double pan){
  double sr = s->son->sr; VoixHC *v = hcVoix(s, HV_CHINA, HB_AIGU, t, .5); v->vel = niv; v->angle = pan*90;
  waNeuf(&v->q[0], WA_HAUT, 3200, 1, 0, sr); waNeuf(&v->q[1], WA_PIC, 5400, 3, 10, sr);
}
static void hcRide(Syn *s, double t, double niv, double haut, double pan){
  VoixHC *v = hcVoix(s, HV_RIDE, HB_AIGU, t, .3); v->vel = niv; v->f = 820 + 300*haut; v->angle = pan*90; waNeuf(&v->q[0], WA_BANDE, 2400, 1.5, 0, s->son->sr);
}
static void hcTom(Syn *s, double t, double niv, double haut, double pan){
  VoixHC *v = hcVoix(s, HV_TOM, HB_COLLE, t, .37); v->vel = niv; v->f = 72 + 150*haut; v->angle = pan*90;
}
static void hcCharleston(Syn *s, double t){
  double sr = s->son->sr; VoixHC *v = hcVoix(s, HV_HAT, HB_AIGU, t, .04); v->dur = .036; v->vel = .26; waNeuf(&v->q[0], WA_HAUT, 7200, 1, 0, sr); v->k1 = courbeHC(CB_DIODE, 70, 0, 0);
  v->angle = ((s->son->hc->nPas >> 1) & 1 ? 1 : -1)*(70 + 50*alea01(s));
}
/* le break de l'émeute, passé à l'échantillonneur */
static void hcBKick(Syn *s, double t, double niv){ VoixHC *v = hcVoix(s, HV_BKICK, HB_ATARI, t, .15); v->vel = niv; waNeuf(&v->q[0], WA_BANDE, 4200, 1.2, 0, s->son->sr); }
static void hcBHat(Syn *s, double t, double niv, double pan){ VoixHC *v = hcVoix(s, HV_BHAT, HB_ATARI, t, .05); v->vel = niv; v->angle = pan*90; waNeuf(&v->q[0], WA_HAUT, 6500, 1, 0, s->son->sr); }

/* ---------- la guitare : accords de puissance ou dissonants, sourdine, trémolo, harmoniques ---------- */
static void hcGuitare(Syn *s, double t, double f, int mode, double vol, double pan, double dur){
  Son *so = s->son; HC *h = so->hc; double sr = so->sr; int sourdine = mode == 'm', seule = mode == 's' || mode == 'h';
  dur = borne(dur, .03, 2.4);
  VoixHC *v = hcVoix(s, HV_GTR, h->emeute ? HB_ATARI : HB_GTR, t, dur + .02); v->mode = mode; v->dur = dur; v->pk = borne(vol, .01, .5); v->angle = pan*90; v->f = f;
  if(mode == 'h'){ v->n = 1; v->fr[0] = f*4; }
  else if(seule){ v->n = 1; v->fr[0] = f; }
  else { double i2 = mode == 'x' ? 6 : 7, i3 = mode == 'x' ? 13 : 12;
    v->n = 4; v->fr[0] = f*pow(2, -7/1200.0); v->fr[1] = f*pow(2, 7/1200.0); v->fr[2] = f*pow(2, i2/12); v->fr[3] = f*pow(2, i3/12); }
  for(int j = 0; j < v->n; j++) v->ph[j] = alea01(s);
  v->vib = mode == 'h' ? f*.12 : (mode == 's' && dur > .25) ? f*.025 : 0;
  waNeuf(&v->q[0], WA_HAUT, sourdine ? 95 : 70, 1, 0, sr);
  waNeuf(&v->q[1], WA_PIC, 720, .9, -5, sr);
  waNeuf(&v->q[2], WA_BAS, sourdine ? 1400 : 5200, .9, 0, sr);
  waNeuf(&v->q[3], WA_PIC, 2600, 1.4, 4, sr);
  v->k1 = courbeHC(CB_SATUREE, 22, 0, 0);
}
/* la basse électrique : une scie, une carrée, un médiator qui accroche, une saturation douce */
static void hcBasse(Syn *s, double t, double f, double dur){
  while(f < 38) f *= 2; dur = borne(dur, .06, 1.2);
  VoixHC *v = hcVoix(s, HV_BASSE, HB_COLLE, t, dur + .01); v->f = f; v->dur = dur; v->k1 = courbeHC(CB_SATUREE, 3, 0, 0); waNeuf(&v->q[0], WA_BAS, 2000, 1.2, 0, s->son->sr);
}
/* ---------- la forme : montée, descente, aspiration, explosion ---------- */
static void hcMontee(Syn *s, double t, double dur){
  double sr = s->son->sr; VoixHC *v = hcVoix(s, HV_MONTEE, HB_AIGU, t, dur + .01); v->dur = dur; waNeuf(&v->q[0], WA_BANDE, 350, 3.5, 0, sr); v->k1 = courbeHC(CB_SATUREE, 12, 0, 0);
}
static void hcImpact(Syn *s, double t){ VoixHC *v = hcVoix(s, HV_IMPACT, HB_AIGU, t, 1.8); waNeuf(&v->q[0], WA_HAUT, 2600, 1, 0, s->son->sr); }
static void hcDescente(Syn *s, double t, double dur){ VoixHC *v = hcVoix(s, HV_DESC, HB_COLLE, t, dur + .01); v->dur = dur; waNeuf(&v->q[0], WA_BAS, 5000, 0, 0, s->son->sr); }
static void hcAspiration(Syn *s, double t, double dur){
  VoixHC *v = hcVoix(s, HV_ASPI, HB_AIGU, t, dur + .01); v->dur = dur; v->sRev = 1; waNeuf(&v->q[0], WA_HAUT, 600, 1, 0, s->son->sr);
}
static void hcFoule(Syn *s, double t){   /* « Hé ! » : cinq voix, deux formants de voyelle ouverte, saturé */
  double sr = s->son->sr; VoixHC *v = hcVoix(s, HV_FOULE, HB_AIGU, t, .21); v->n = 5; v->k1 = courbeHC(CB_SATUREE, 8, 0, 0);
  for(int j = 0; j < 5; j++){ v->fr[j] = 170 + 110*alea01(s); v->ph[j] = alea01(s); }
  waNeuf(&v->q[0], WA_BANDE, 720, 4, 0, sr); waNeuf(&v->q[1], WA_BANDE, 1750, 5, 0, sr); v->angle = (alea01(s) - .5)*60;
}
static void hcExplosion(Syn *s, double t){
  Son *so = s->son; HC *h = so->hc; double sr = so->sr, dPas = so->dPas;
  { VoixHC *v = hcVoix(s, HV_EXN, HB_AIGU, t, 2.2); v->sRev = 1; v->k1 = courbeHC(CB_SATUREE, 5, 0, 0); waNeuf(&v->q[0], WA_BANDE, 9000, .7, 0, sr); }
  { VoixHC *v = hcVoix(s, HV_BOOM, HB_COLLE, t, 1.6); v->f = 140; v->f2 = 30; v->dur = 1.6; v->pk = .5; }
  { VoixHC *v = hcVoix(s, HV_BOOM, HB_COLLE, t, 2.2); v->f = 70; v->f2 = 24; v->dur = 2.2; v->pk = .35; }
  for(int k = 0; k < 14; k++){   /* les éclats : des cloches en modulation de fréquence, dispersées autour de l'auditeur */
    double tk = t + .02 + pow(alea01(s), 1.6)*1.4; VoixHC *v = hcVoix(s, HV_ECLAT, HB_AIGU, tk, .32);
    v->f = 900 + alea01(s)*5200; v->pk = .09*(1 - k/18.0); v->angle = (alea01(s)*2 - 1)*170; v->sEcho = 1; v->sRev = 1; }
  hcFoule(s, t + .01);
  /* les espaces s'ouvrent d'un coup, puis se referment en deux mesures */
  hcAuto(h, t, AU_REV, 1.1, 0, 1);  hcAuto(h, t + dPas*4, AU_REV, .22, dPas*7, 0);
  hcAuto(h, t, AU_ECHO, .7, 0, 1);  hcAuto(h, t + dPas*6, AU_ECHO, .08, dPas*8, 0);
  hcAuto(h, t, AU_FB, .62, 0, 1);   hcAuto(h, t + dPas*8, AU_FB, .42, dPas*8, 0);
  hcAuto(h, t, AU_GTR, 1.2, 0, 1);  hcAuto(h, t + dPas*6, AU_GTR, .4, dPas*8, 0);
  h->expl = t + dPas*28;
}
static void hcSirene(Syn *s, double t, double dur){ VoixHC *v = hcVoix(s, HV_SIRENE, HB_ATARI, t, dur + .01); v->dur = dur; }
static void hcPuce(Syn *s, double t, double f, double dur){ VoixHC *v = hcVoix(s, HV_PUCE, HB_ATARI, t, dur + .01); v->f = f; v->dur = dur; v->f2 = s->son->dPas/2; }

/* ---------- une voix, un échantillon ---------- */
static void hcJouer(Syn *s, VoixHC *v, double tl, double *b){
  Son *so = s->son; double sr = so->sr, x = 0, y;
  #define MAJ_FILTRE(Q, TYPE, F, QQ, G) do{ Biquad _a = (Q); waRegler(&(Q), TYPE, F, QQ, G, sr); memcpy((Q).z1, _a.z1, sizeof _a.z1); memcpy((Q).z2, _a.z2, sizeof _a.z2); }while(0)
  switch(v->type){
    case HV_KICK: {
      double f = tl < .028 ? rampeE(tl, 0, 210, .028, 62) : rampeE(tl, .028, 62, .12, 48);
      x = sin(2*PI*pas01(&v->ph[0], f, sr)); y = faconner(v->k1, x);
      double g = tl < .002 ? rampeL(tl, 0, .001, .002, .95*v->vel) : rampeE(tl, .002, .95*v->vel, .13, .001);
      b[HB_COLLE] += y*g;
      if(tl < .012) b[HB_COLLE] += biquad(&v->q[0], 0, blanc(s))*rampeE(tl, 0, .55*v->vel, .01, .001);
      break; }
    case HV_CAISSE: {
      double nz = biquad(&v->q[0], 0, blanc(s))*rampeE(tl, 0, .5*v->vel, .17, .001);
      double f = rampeE(tl, 0, 240, .08, 185), o = 4*fabs(pas01(&v->ph[0], f, sr) - .5) - 1;
      b[v->bus] += nz + o*rampeE(tl, 0, .45*v->vel, .1, .001); break; }
    case HV_CHINA: { x = biquad(&v->q[1], 0, biquad(&v->q[0], 0, blanc(s))); b[HB_AIGU] += x*rampeE(tl, 0, .3*v->vel, .5, .001); break; }
    case HV_RIDE: { double a = pas01(&v->ph[0], v->f, sr) < .5 ? 1 : -1, c = pas01(&v->ph[1], v->f*1.48, sr) < .5 ? 1 : -1;
      b[HB_AIGU] += biquad(&v->q[0], 0, a + c)*rampeE(tl, 0, .09*v->vel, .3, .001); break; }
    case HV_TOM: { double f = tl < .02 ? rampeE(tl, 0, v->f*1.6, .02, v->f) : rampeE(tl, .02, v->f, .35, v->f*.82);
      b[HB_COLLE] += sin(2*PI*pas01(&v->ph[0], f, sr))*rampeE(tl, 0, .5*v->vel, .36, .001); break; }
    case HV_HAT: { x = faconner(v->k1, biquad(&v->q[0], 0, blanc(s))); b[HB_AIGU] += x*rampeE(tl, 0, v->vel, v->dur, .001); break; }
    case HV_BHAT: { b[HB_ATARI] += biquad(&v->q[0], 0, blanc(s))*rampeE(tl, 0, .2*v->vel, .04, .001); break; }
    case HV_BKICK: { double f = rampeE(tl, 0, 190, .03, 58);
      b[HB_ATARI] += sin(2*PI*pas01(&v->ph[0], f, sr))*rampeE(tl, 0, .55*v->vel, .14, .001);
      if(tl < .012) b[HB_ATARI] += biquad(&v->q[0], 0, blanc(s))*rampeE(tl, 0, .35*v->vel, .008, .001); break; }
    case HV_GTR: {
      double vib = v->vib ? sin(2*PI*pas01(&v->phl, 6.2, sr))*rampeL(tl, 0, 0, v->dur*.5, v->vib) : 0;
      for(int j = 0; j < v->n; j++){ double f = v->fr[j] + vib*(v->fr[j]/v->fr[0]), dt = f/sr;
        x += v->mode == 'h' ? 4*fabs(v->ph[j] - .5) - 1 : polyScie(v->ph[j], dt); v->ph[j] += dt; v->ph[j] -= floor(v->ph[j]); }
      if(v->mode == 'm' && (v->compte++ & 15) == 0) MAJ_FILTRE(v->q[2], WA_BAS, rampeE(tl, 0, 1400, v->dur, 380), .9, 0);
      x = biquad(&v->q[0], 0, x)*7; x = faconner(v->k1, x); x = biquad(&v->q[1], 0, x); x = biquad(&v->q[2], 0, x); x = biquad(&v->q[3], 0, x);
      double g = tl < .003 ? rampeL(tl, 0, .0001, .003, v->pk) : tl < v->dur*.75 ? v->pk : rampeE(tl, v->dur*.75, v->pk*.8, v->dur, .0001);
      b[v->bus] += x*g; break; }
    case HV_BASSE: {
      x = polyScie(v->ph[0], v->f/sr) + (pas01(&v->ph[1], v->f*1.004, sr) < .5 ? 1 : -1); v->ph[0] += v->f/sr; v->ph[0] -= floor(v->ph[0]);
      if((v->compte++ & 15) == 0) MAJ_FILTRE(v->q[0], WA_BAS, rampeE(tl, 0, 2000, mn2(.15, v->dur), 500), 1.2, 0);
      x = faconner(v->k1, biquad(&v->q[0], 0, x));
      double g = tl < .004 ? rampeL(tl, 0, .001, .004, .32) : tl < v->dur*.8 ? .26 : rampeE(tl, v->dur*.8, .26, v->dur, .001);
      b[HB_COLLE] += x*g; break; }
    case HV_MONTEE: {
      double d = v->dur;
      if((v->compte++ & 15) == 0) MAJ_FILTRE(v->q[0], WA_BANDE, rampeE(tl, 0, 350, d, 9000), 3.5, 0);
      double g = tl < d*.5 ? rampeE(tl, 0, .001, d*.5, .06) : tl < d*.98 ? rampeE(tl, d*.5, .06, d*.98, .42) : rampeL(tl, d*.98, .42, d, 0);
      x = biquad(&v->q[0], 0, blanc(s))*g;
      if(tl >= d*.75){ double f1 = rampeE(tl, d*.75, 110, d, 1320), f2 = rampeE(tl, d*.75, 111.5, d, 1330);
        double sc = (2*pas01(&v->ph[0], f1, sr) - 1) + (pas01(&v->ph[1], f2, sr) < .5 ? 1 : -1);
        double g2 = tl < d*.97 ? rampeE(tl, d*.75, .001, d*.97, .14) : rampeL(tl, d*.97, .14, d, 0);
        x += faconner(v->k1, sc)*g2; }
      v->angle = fmod(tl*90, 360) - 180;   /* la montée tourne autour de la pièce */
      b[HB_AIGU] += x; break; }
    case HV_IMPACT: {
      b[HB_AIGU] += biquad(&v->q[0], 0, blanc(s))*rampeE(tl, 0, .55, 1.7, .001);
      double f = rampeE(tl, 0, 110, .9, 32); b[HB_COLLE] += sin(2*PI*pas01(&v->ph[0], f, sr))*(tl < .01 ? rampeL(tl, 0, .001, .01, .4) : rampeE(tl, .01, .4, 1, .001)); break; }
    case HV_DESC: { double d = v->dur, f1 = rampeE(tl, 0, 1400, d, 45), f2 = rampeE(tl, 0, 1412, d, 44.4);
      x = sin(2*PI*pas01(&v->ph[0], f1, sr)) + 4*fabs(pas01(&v->ph[1], f2, sr) - .5) - 1;
      if((v->compte++ & 15) == 0) MAJ_FILTRE(v->q[0], WA_BAS, rampeE(tl, 0, 5000, d, 220), 0, 0);
      double g = tl < .1 ? rampeE(tl, 0, .001, .1, .15) : tl < d*.6 ? .15 : rampeE(tl, d*.6, .15, d, .001);
      b[HB_COLLE] += biquad(&v->q[0], 0, x)*g; break; }
    case HV_ASPI: { double d = v->dur;
      if((v->compte++ & 15) == 0) MAJ_FILTRE(v->q[0], WA_HAUT, rampeE(tl, 0, 600, d, 5000), 1, 0);
      double g = tl < d*.98 ? rampeE(tl, 0, .001, d*.98, .22) : rampeL(tl, d*.98, .22, d, 0);
      x = biquad(&v->q[0], 0, blanc(s))*g; b[HB_AIGU] += x; b[HB_REV] += x; break; }
    case HV_EXN: {
      if((v->compte++ & 15) == 0) MAJ_FILTRE(v->q[0], WA_BANDE, rampeE(tl, 0, 9000, 1.8, 250), .7, 0);
      x = faconner(v->k1, biquad(&v->q[0], 0, blanc(s)))*rampeE(tl, 0, .9, 2.1, .001); b[HB_AIGU] += x; b[HB_REV] += x; break; }
    case HV_BOOM: { double f = rampeE(tl, 0, v->f, v->dur*.8, v->f2);
      b[HB_COLLE] += sin(2*PI*pas01(&v->ph[0], f, sr))*(tl < .008 ? rampeL(tl, 0, .001, .008, v->pk) : rampeE(tl, .008, v->pk, v->dur, .001)); break; }
    case HV_ECLAT: { double mg = rampeE(tl, 0, v->f*2, .25, v->f*.05), m = sin(2*PI*pas01(&v->ph[1], v->f*1.41, sr));
      x = sin(2*PI*pas01(&v->ph[0], v->f + m*mg, sr))*(tl < .002 ? rampeL(tl, 0, .001, .002, v->pk) : rampeE(tl, .002, v->pk, .3, .001));
      b[HB_AIGU] += x; b[HB_ECHO] += x; b[HB_REV] += x; break; }
    case HV_FOULE: { double fr = tl < .2 ? tl/.2 : 1;
      for(int j = 0; j < 5; j++){ double f = v->fr[j]*1.1*pow(.82/1.1, fr); x += polyScie(v->ph[j], f/sr); v->ph[j] += f/sr; v->ph[j] -= floor(v->ph[j]); }
      x += blanc(s)*.5; y = biquad(&v->q[0], 0, x) + biquad(&v->q[1], 0, x); y = faconner(v->k1, y);
      b[HB_AIGU] += y*(tl < .02 ? rampeL(tl, 0, .001, .02, .42) : rampeE(tl, .02, .42, .2, .001)); break; }
    case HV_SIRENE: { double d = v->dur, lf = rampeE(tl, 0, .9, d, 7), tri = 4*fabs(pas01(&v->phl, lf, sr) - .5) - 1;
      x = pas01(&v->ph[0], 820 + 260*tri, sr) < .5 ? 1 : -1;
      double g = tl < d*.3 ? rampeE(tl, 0, .001, d*.3, .05) : tl < d*.97 ? rampeE(tl, d*.3, .05, d*.97, .11) : rampeL(tl, d*.97, .11, d, 0);
      b[HB_ATARI] += x*g; break; }
    case HV_PUCE: { static const double AR[4] = {1, 1.189, 1.498, 2}; double f = v->f*AR[(int)(tl/v->f2) % 4];
      x = pas01(&v->ph[0], f, sr) < .5 ? 1 : -1; b[HB_ATARI] += x*(tl < v->dur*.85 ? .075 : rampeL(tl, v->dur*.85, .075, v->dur, 0)); break; }
  }
  #undef MAJ_FILTRE
}

/* ================= le moteur : création, partition, synthèse ================= */
static void hcCreer(Syn *s){
  Son *so = s->son; HC *h = calloc(1, sizeof(HC)); so->hc = h; double sr = so->sr;
  h->preset = HP_A; h->transpo = 1; h->graine = 7.3; h->press = .6; h->colleGain = 1; h->hpF = h->hpCible = 18;
  h->kSat = courbeHC(CB_SATUREE, 6, 0, 0);
  for(int c = 0; c < 4; c++) waNeuf(&h->ahp[c], WA_HAUT, 180, 1, 0, sr);
  for(int c = 0; c < 6; c++) waNeuf(&h->hpM[c], WA_HAUT, 18, .9, 0, sr);
  for(int c = 0; c < 2; c++) waNeuf(&h->lfe[c], WA_BAS, 90, -3, 0, sr);
  h->lpCour = 20000; waNeuf(&h->lpB, WA_BAS, 20000, -6, 0, sr); for(int c = 0; c < 4; c++){ waNeuf(&h->lpA[c], WA_BAS, 20000, -6, 0, sr); waNeuf(&h->lpF[c], WA_BAS, 20000, -6, 0, sr); }
  static const double AU0[NAU] = { .12, 0, .5, .42, 20000, .3 };
  for(int k = 0; k < NAU; k++){ h->auV[k] = h->auC[k] = AU0[k]; h->auT[k] = .05; }
  static const double RL[4] = { .0433, .0517, .0601, .0689 };   /* longueurs premières entre elles, à peu près */
  for(int i = 0; i < 4; i++){ h->rn[i] = (int)(RL[i]*sr); h->rv[i] = calloc(h->rn[i], sizeof(float)); }
  waNeuf(&h->rhp, WA_HAUT, 220, 1, 0, sr);
  h->en = (int)(sr*2.2); h->ec[0] = calloc(h->en, sizeof(float)); h->ec[1] = calloc(h->en, sizeof(float));
  waNeuf(&h->ehp, WA_HAUT, 320, 1, 0, sr); waNeuf(&h->elp, WA_BAS, 3800, 1, 0, sr);
}
static void hcDetruire(HC *h){ if(!h) return; for(int i = 0; i < 4; i++) free(h->rv[i]); free(h->ec[0]); free(h->ec[1]); free(h); }

/* un pas de la partition */
static void grooveHardcore(Syn *s, int st, double t){
  Son *so = s->son; HC *h = so->hc; double dPas = so->dPas;
  h->transpo = pow(2, h->transpoDemi/12.0);
  if(st > 0 && st % 512 == 384){ h->preset = (h->preset + 1) % NHP; h->graine = alea01(s)*999; }   /* à chaque chute de deuxième arc, un autre riff */
  PasHC e = hcSeq(st, h->preset, h->graine); int p = e.p;
  h->nPas++; { int k = h->nPas % 64; h->tPas[k] = t; h->stPas[k] = st; }
  h->press = e.pression; h->montee = e.montee; h->emeute = e.emeute;
  if(h->nHp < 64){ Evenement *q = &h->hp[h->nHp++]; q->t = t; q->v = e.montee ? 20*pow(21, mn2(1, e.prog*1.08)) : 18; q->tau = e.montee ? .03 : 0; }
  if(e.riser) hcMontee(s, t, dPas*120);   /* elle s'arrête au début de la demi-mesure de silence */
  /* les espaces suivent la forme ; l'explosion les ouvre tous */
  hcAuto(h, t, AU_ETIME, dPas*3, .05, 0);
  hcAuto(h, t, AU_LP, e.descente ? 18000*pow(350.0/18000, mn2(1, e.dprog*1.05)) : 20000, e.descente ? .04 : .015, 0);
  if(e.impact){ hcExplosion(s, t); hcImpact(s, t); }
  else if(!(h->expl > t)){
    int relance = e.sorte == HM_RELANCE && p >= 8;
    double rv = relance ? .5 : e.montee ? .12 + .55*e.prog : e.descente ? .25 + .3*e.dprog : e.phase == PH_SURPRISE ? .3 : e.section == HS_CHUTE ? .2 : .1;
    double ec = relance ? .75 : e.descente ? .12 + .4*e.dprog : e.phase == PH_SURPRISE ? .35 : e.sorte == HM_MOTIFFIN && p >= 12 ? .4 : e.section == HS_CHUTE ? .08 : 0;
    hcAuto(h, t, AU_REV, rv, .04, 0); hcAuto(h, t, AU_ECHO, ec, .03, 0); hcAuto(h, t, AU_GTR, e.montee || e.descente ? .7 : .4, .05, 0);
    hcAuto(h, t, AU_FB, relance ? .58 : e.descente ? .5 : .42, .05, 0);
    if(e.sorte == HM_RELANCE && p == 8) hcAspiration(s, t, dPas*8);
  }
  if(e.descDebut) hcDescente(s, t, dPas*32);
  /* l'émeute : sirène, foule, puce, et le break */
  if(e.emeute){
    if(e.riser) hcSirene(s, t, dPas*128);
    if(e.section == HS_CHUTE && !e.descente && p == 0 && (e.b & 1) == 0) hcFoule(s, t);
    if(e.section == HS_FOND && p == 12 && (e.b & 3) == 3) hcFoule(s, t);
    if(e.section == HS_CHUTE && !e.descente && p % 4 == 0){ static const int PU[8] = {0, 0, 6, 5, 0, 1, 6, 7};
      hcPuce(s, t, GAMME_HC[0]*8*h->transpo*pow(2, PU[(e.b & 1)*4 + (p >> 2)]/12.0), dPas*4); }
    if(e.breakbeat){
      if(e.kick){ hcBKick(s, t, 1.25); hcKick(s, t, .7); }
      if(e.caisse){ int n = e.rat ? e.rat : 1; for(int r = 0; r < n; r++) hcCaisse(s, t + r*dPas/n, n > 1 ? .7 + .3*r/n : 1.15, .55, (r & 1) ? .25 : -.25, HB_ATARI); }
      if(e.ghost) hcCaisse(s, t, .38, .3, .3, HB_ATARI);
      if(e.ride) hcBHat(s, t, p % 4 == 2 ? 1 : .6, -.4);
      e.kick = e.caisse = 0;
    }
  }
  /* la batterie */
  if(!e.breakbeat){
    if(e.kick) hcKick(s, t, e.accent ? 1 : .72);
    if(e.caisse){ int n = e.rat ? e.rat : 1; for(int r = 0; r < n; r++) hcCaisse(s, t + r*dPas/n, n > 1 ? .8 + .2*r/n : 1.3, .6, (r & 1) ? .2 : -.2, HB_AIGU); }
    if(e.toms) hcTom(s, t, 1.1, 1 - (p - 12)*.25, .6 - .3*(p - 12));
  }
  if(e.china) hcChina(s, t, 1.05, .45);
  if(e.ride) hcRide(s, t, .5, .55, -.4);
  if(e.hat) hcCharleston(s, t);
  /* la guitare et la basse, à l'unisson de la grosse caisse */
  double rf = GAMME_HC[0]*2*h->transpo;
  if(e.impact) hcGuitare(s, t, rf, 'o', .3, 0, dPas*8);
  if(e.gM && !e.pause && !e.impact){
    double f = rf*pow(2, e.gD/12.0), v = e.gM == 'm' ? .19 : e.gM == 's' ? .14 : .23;
    if(e.hache){ for(int r = 0; r < 2; r++) hcGuitare(s, t + r*dPas/2, f*pow(2, ((p - 12)*2 + r)/12.0), 'o', .2, r ? .4 : -.4, dPas*.42); }
    else if(e.gTrem){ hcGuitare(s, t, f, e.gM, v, -.35, dPas*.48); hcGuitare(s, t + dPas/2, f, e.gM, v*.9, .35, dPas*.48); }
    else hcGuitare(s, t, f, e.gM, v, e.gM == 'm' ? -.3 : ((e.b + p) & 1) ? .3 : -.3, dPas*e.gDur);
  }
  if(e.basse && !e.pause) hcBasse(s, t, rf*.5*pow(2, e.bD/12.0), dPas*(e.gDur > 0 ? mx2(1, e.gDur) : 2));
}
/* le pas qui sonne en ce moment (pour l'image) : le plus récent dont l'heure est passée */
static int hcPasJoue(Son *so, double t, double *frac){
  HC *h = so->hc; int best = -1; double bt = -1;
  for(int i = 0; i < 64; i++) if(h->tPas[i] <= t && h->tPas[i] > bt && h->tPas[i] > 0){ bt = h->tPas[i]; best = h->stPas[i]; }
  if(frac) *frac = best >= 0 ? borne((t - bt)/so->dPas, 0, 1) : 0;
  return best;
}

/* place un son sur le cercle des enceintes : gains pour FL, FR, C, SL, SR (angle en degrés, 0 devant, positif à droite) */
static void placer(double angle, double etendue, double *g);
static inline void hcPlacer(double angle, int surround, double g[4]){
  if(surround){ double q[5]; placer(angle, .3, q); g[0] = q[0] + q[2]*.7; g[1] = q[1] + q[2]*.7; g[2] = q[3]; g[3] = q[4]; }
  else { double x = (borne(angle/90, -1, 1)*.6 + 1)/2; g[0] = cos(x*PI/2)*1.41; g[1] = sin(x*PI/2)*1.41; g[2] = g[3] = 0; }
}

/* un échantillon du moteur : sortie mono grave (colle), et dans quatre directions (FL, FR, SL, SR ; en stéréo, G et D)
   les aigus, la guitare et l'Atari ; les espaces (réverbération, écho) à part dans h->fxo, ajoutés après l'écrêteur */
static void hcEchantillon(Syn *s, double t, double *grave, double aigu[4], int surround){
  Son *so = s->son; HC *h = so->hc; double sr = so->sr; long ech = so->nEch + so->iBloc;
  double colle = 0, a4[4] = {0}, g4[4] = {0}, at4[4] = {0}, revDir = 0, echoDir = 0;
  for(int i = 0; i < NVHC; i++){
    VoixHC *v = &h->v[i]; if(!v->actif) continue;
    if(ech < v->debut) continue; if(ech >= v->fin){ v->actif = 0; continue; }
    double b[NHB] = {0}; hcJouer(s, v, (ech - v->debut)/sr, b);
    colle += b[HB_COLLE]; revDir += b[HB_REV]; echoDir += b[HB_ECHO];
    if(b[HB_AIGU] != 0 || b[HB_GTR] != 0 || b[HB_ATARI] != 0){
      double g[4]; hcPlacer(v->angle, surround, g);
      for(int c = 0; c < 4; c++){ a4[c] += b[HB_AIGU]*g[c]; g4[c] += b[HB_GTR]*g[c]; at4[c] += b[HB_ATARI]*g[c]; }
    }
  }
  /* les automations : leurs événements, puis un lissage */
  while(h->nAu > 0 && h->au[0].t <= t){ Evenement *q = &h->au[0]; h->auC[q->k] = q->v; h->auT[q->k] = q->tau > 0 ? q->tau : .002; if(q->saut) h->auV[q->k] = q->v;
    memmove(h->au, h->au + 1, sizeof(Evenement)*(--h->nAu)); }
  for(int k = 0; k < NAU; k++) h->auV[k] += (h->auC[k] - h->auV[k])*(1 - exp(-1/(sr*h->auT[k])));
  double rG = h->auV[AU_REV], eG = h->auV[AU_ECHO], gG = h->auV[AU_GTR];
  /* l'échantillonneur 8 bits : un échantillon sur quatre, 64 niveaux, sans lissage */
  if(h->atC <= 0){ for(int c = 0; c < 4; c++) h->atH[c] = round(at4[c]*64)/64; h->atC = (int)lround(sr/12000.0); } h->atC--;
  for(int c = 0; c < 4; c++) at4[c] = h->atH[c]*.9;
  /* la colle : seuil -12 dB, genou 3 dB, rapport 8, attaque 12 ms, relâche 45 ms (la guitare et l'Atari y passent aussi) */
  double gs = 0, as = 0; for(int c = 0; c < 4; c++){ gs += g4[c]; as += at4[c]; } gs *= .5; as *= .5;
  { double a = fabs(colle + gs + as), dB = a > 1e-6 ? 20*log10(a) : -120, o;
    if(dB < -13.5) o = dB; else if(dB < -10.5){ double u = dB + 13.5; o = dB + (1.0/8 - 1)*u*u/6; } else o = -12 + (dB + 12)/8;
    double cible = pow(10, (o - dB)/20); if(cible > 1) cible = 1;
    double tau = cible < h->colleGain ? .012 : .045; h->colleGain += (cible - h->colleGain)*(1 - exp(-1/(sr*tau))); }
  double pousse = h->colleGain*2.07*1.35;
  /* les envois : les aigus, la guitare (dosée), l'Atari ; plus les envois directs */
  double sendBase = 0; for(int c = 0; c < 4; c++) sendBase += a4[c] + g4[c]*gG + at4[c]; sendBase *= .5;
  /* l'écho ping-pong : gauche, droite, gauche… à la croche pointée */
  double eL, eR;
  { double dt = borne(h->auV[AU_ETIME], .02, 1)*sr; double lu = h->ei - dt; while(lu < 0) lu += h->en; int i0 = (int)lu, i1 = (i0 + 1) % h->en; double fr = lu - i0;
    eL = h->ec[0][i0]*(1 - fr) + h->ec[0][i1]*fr; eR = h->ec[1][i0]*(1 - fr) + h->ec[1][i1]*fr;
    double in = biquad(&h->ehp, 0, (sendBase + echoDir)*eG);
    h->ec[0][h->ei] = (float)(in + biquad(&h->elp, 0, eR)*h->auV[AU_FB]); h->ec[1][h->ei] = (float)eL;
    if(++h->ei >= h->en) h->ei = 0; }
  /* la réverbération : quatre lignes, matrice de Householder, l'aigu qui s'éteint d'abord (environ 3,8 s) */
  double r4[4];
  { double in = biquad(&h->rhp, 0, (sendBase + revDir + (eL + eR)*.5)*rG)*.35, o[4], sum = 0;
    for(int i = 0; i < 4; i++){ o[i] = h->rv[i][h->ri[i]]; sum += o[i]; }
    for(int i = 0; i < 4; i++){ double gfb = pow(10, -3.0*h->rn[i]/(sr*3.8)), y = (o[i] - sum*.5)*gfb;
      h->rlp[i] += (y - h->rlp[i])*.55; h->rv[i][h->ri[i]] = (float)(h->rlp[i] + in*(i & 1 ? -1 : 1)); if(++h->ri[i] >= h->rn[i]) h->ri[i] = 0; r4[i] = o[i]; } }
  double fx[4] = { (r4[0] + r4[2])*.55 + eL*.5, (r4[1] + r4[3])*.55 + eR*.5, (r4[2] - r4[1])*.55 + eR*.3, (r4[3] - r4[0])*.55 + eL*.3 };
  if(!surround){ fx[2] = fx[3] = 0; }
  /* le passe-bas de la descente : sur tout, espaces compris */
  { double lf = h->auV[AU_LP];
    if(fabs(lf - h->lpCour) > lf*.01 && (h->hpCompte & 15) == 0){ h->lpCour = lf;
      #define MAJLP(Q) do{ Biquad _a = (Q); waRegler(&(Q), WA_BAS, lf, -6, 0, sr); memcpy((Q).z1, _a.z1, sizeof _a.z1); memcpy((Q).z2, _a.z2, sizeof _a.z2); }while(0)
      MAJLP(h->lpB); for(int c = 0; c < 4; c++){ MAJLP(h->lpA[c]); MAJLP(h->lpF[c]); }
      #undef MAJLP
    } }
  int lpOn = h->lpCour < 19000;
  double B = colle*pousse; *grave = lpOn ? biquad(&h->lpB, 0, B) : B;
  for(int c = 0; c < 4; c++){
    double a = faconner(h->kSat, biquad(&h->ahp[c], 0, a4[c]*2.2))*.8 + (g4[c] + at4[c])*pousse;
    aigu[c] = lpOn ? biquad(&h->lpA[c], 0, a) : a;
    h->fxo[c] = lpOn ? biquad(&h->lpF[c], 0, fx[c]) : fx[c];
  }
  /* le passe-haut de la montée : ses événements, puis un lissage */
  while(h->nHp > 0 && h->hp[0].t <= t){ h->hpCible = h->hp[0].v; h->hpTau = h->hp[0].tau; if(h->hpTau <= 0) h->hpF = h->hpCible; memmove(h->hp, h->hp + 1, sizeof(Evenement)*(--h->nHp)); }
  if(h->hpTau > 0) h->hpF += (h->hpCible - h->hpF)*(1 - exp(-1/(sr*h->hpTau)));
  h->hpCompte++;
}

#include "jg.inc"
