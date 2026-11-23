/* Synchromy — moteur sonore natif. Port du module SON de la version web : mêmes recettes, mêmes styles, même voyage.
   Web Audio y est remplacé par un petit synthétiseur : oscillateurs, bruits filtrés, enveloppes, panoramique,
   trois bus (sec, réverbération, écho), une réverbération algorithmique, un écho bouclé, un compresseur.
   Le son est calculé sur le fil principal, un peu en avance, dans un anneau que lit le fil audio. */
#include <stdatomic.h>

enum { BUS_SEC, BUS_REV, BUS_ECHO };
enum { ON_SINUS, ON_TRIANGLE, ON_CARRE, ON_SCIE };
enum { FT_DEFAUT, FT_BAS, FT_HAUT, FT_BANDE };
enum { G_AUCUN = -1, G_BOUM, G_CLAIRE, G_TIC, G_BOIS, G_CLAVE, G_TOM, G_CLOCHE, G_RIDE, G_CONGA, G_BASSE, G_RONDE, G_GLISSE, G_MORSE,
       G_PLUIE, G_ACCORD, G_BOL, G_SCINTILLE, G_TOC, G_MAINS, G_SHAKER, G_EPINES, G_MARIMBA, G_GONG, G_TRANCHE,
       G_ESCALIER, G_TOURNE, G_CRISTAL, G_GROUILLE, G_ECLAT, G_CLOCHETTE,
       G_REMOUS, G_ARCHE, G_BASCULE, G_FANFARE, G_DEPHASE, G_GOUFFRE, G_CARREE, G_CRECELLE, G_PIZZ, NGENRES };   /* les voix des motifs d'op art */
enum { OR_SYNTHE, OR_OBJETS, OR_CORDES, OR_CHOEUR, OR_SOUFFLES, OR_GAMELAN, OR_CHAMBRE, OR_ECOSYSTEME, OR_RADIO, OR_SONAR, OR_METAL, OR_BANDE, OR_AUTOMATES, OR_PEAUX, NORCH };
enum { ST_LIBRE, ST_MINIMAL, ST_DNB, ST_JUNGLE, ST_CHIP, ST_HOUSE, ST_DUB, ST_HIPHOP, ST_AFRO, ST_GAMELAN, ST_BOSSA, ST_AMBIENT, ST_CONCRET, NSTYLES };

/* ---------- options d'un coup ou d'un souffle (comme l'objet « o » de la version web) ---------- */
typedef struct {
  double f, vol, dur, att, t, pan, glisse, rap2, niv2, det, fmR, fmI, lfoF, lfoD, ff, fvers, fq, q, vers, clair;
  int bus, onde, aFm, lfo /* 0, 1 trem, 2 vib */, aFiltre, ftype, arp, pur, rauque, type, brut /* déjà joué par l'orchestre */, ks /* corde de Karplus-Strong */;
} Opt;

/* ---------- voix brève ---------- */
typedef struct {
  int actif, bruit, onde, bus, part, narp, lfo, ftype, aFiltre;
  long debut, fin, pos;                    /* en échantillons */
  double att, dur, vol, env, kEnv, f, kF, f2, niv2, fmR, depFm, kFm, lfoF, lfoD, ff, kFf, fq, gl, gr; float g5[5];   /* g5 : gains vers FL, FR, C, SL, SR (surround) */
  double arp[3], ph1, ph2, phm, phl; int iarp, decroit;
  double b0, b1, b2, a1, a2, z1, z2; int fcompte;
  long lecture;
  const float *ks; long ksN; double ksPos, ksPas;   /* corde : lecture d'une corde calculée d'avance */
} Coup;

typedef struct { double v, c, tau, k, tauK, k1K; } Lisse;
static inline void viser(Lisse *l, double c, double tau){ l->c = c; l->tau = tau; }
/* lissage exponentiel ; le coefficient n'est recalculé que si la constante de temps change, pas à chaque échantillon */
static inline double suivreL(Lisse *l, double k1){
  if(l->tau <= 0){ l->v = l->c; return l->v; }
  if(l->tau != l->tauK || k1 != l->k1K){ l->k = 1 - exp(-k1/l->tau); l->tauK = l->tau; l->k1K = k1; }
  l->v += (l->c - l->v)*l->k; return l->v;
}

typedef struct { double b0, b1, b2, a1, a2, z1[2], z2[2]; } Biquad;

/* réverbération : peigne et passe-tout, à la manière de Freeverb */
#define NPEIGNE 8
#define NPASSE 4
typedef struct { float *buf; int n, i; float memo; } Peigne;
typedef struct { float *buf; int n, i; } PasseTout;

typedef struct Son {
  int actif; double sr, k1;
  /* anneau : écrit par le fil principal, lu par le fil audio */
  float *anneau; int capa; _Atomic long ecrit, lu;
  long nEch;                       /* échantillons calculés : l'horloge du son */
  Coup coups[128]; int nbCoups;
  int orch, pasTotal; double niveau;   /* orchestre : famille d'instruments ; pas joués depuis le début (figures polymétriques) */
  float *bruit, *bruit8; int nBruit;
  /* bus */
  double busL[2][3], busR[2][3];   /* [bus/partition][sec, rév, écho] pour l'échantillon courant */
  Lisse gPart[3];
  /* réverbération */
  Peigne pg[4][NPEIGNE]; PasseTout pt[4][NPASSE]; double revFb, revAmort; Lisse revRetour;   /* quatre réverbérations décorrélées en surround, deux en stéréo */
  /* écho */
  float *ligne[2]; int nLigne, iLigne; Lisse retard, retBoucle, retRetour; Biquad retFiltre;
  /* compresseur, adoucisseur, maître */
  double compGain; Biquad doux, douxS[3], lfeF[2]; Lisse maitre;
  /* sorties : 2 (stéréo) ou 6 (5.1 : FL, FR, C, LFE, SL, SR) ; le Hardcore, son moteur et son état */
  int nc, hardcore, iBloc, hcDebut; struct HC *hc; double hcSortie; Lisse hcG; double gVoix[5], gNappe[5];
  /* nappe : ruissellement du liquide */
  Lisse nappeG, nappeF, nappePan; Biquad nappeFiltre; long nappeLecture; int nappeCompte;
  /* voix continue du dessin automatique */
  struct { int ouverte, genre, motif, fermeture; double vu, tFin; Forme *forme; double deg, angle, tours, tAccent;
           Lisse f1, det1, f2, g2, ff, vca, pan, fm, gfm, gvib; double ph1, ph2, phm, phl; Biquad filtre; int fcompte; long lecture; int fqBas; double q;
           int onde, formants; double clair, souffle; Biquad pk[3], sf; long sfLecture; } voix;
  struct { int actif; Forme *forme; int i; double vu, vit, x, y; } roul;
  /* musique */
  int etape, tours, derive, mesures, pasSeq, style; double avance, racine, dPas, tProchain, tempo;
  double tBulle, tGel, tChoc, tChocS, tDisp, eauLisse; int compteTic;
  int enPartition, dynamique;   /* scène rythmée : un groove continu, doux et rond, sous la partition */
  /* événements de la physique */
  int evGel, evChocL, evChocS, evEauN; double evGelX, evChocLX, evChocLV, evChocSX, evChocSV, evEnergie, evEauX;
} Son;

/* ---------- les sept étapes du voyage ---------- */
typedef struct { int g[7], n, racine; double tempo, d, sombre, rev, echo, boucle; int puls; double clair; } Etape;
static const Etape ETAPES[7] = {
  { {0,2,4,7,9}, 5, 0, 1, 1.0, .25, .24, .2, .25, -1, 1 },
  { {0,2,3,5,7,9,10}, 7, 5, 1.06, 1.3, .35, .26, .46, .48, 0, 1.1 },
  { {0,2,4,6,7,9,11}, 7, -2, 1.1, 1.8, .3, .36, .34, .4, 1, 1.3 },
  { {0,3,5,7,10}, 5, -5, .84, 3.4, .84, .52, .2, .3, -2, .6 },
  { {0,2,4,6,8,10}, 6, 3, 1.16, .7, .2, .16, .5, .55, 4, 1.2 },
  { {0,1,5,7,8}, 5, -7, .74, 4.2, .9, .6, .3, .5, -3, .45 },
  { {0,2,4,7,9}, 5, 7, 1, 2.2, .3, .4, .28, .35, 0, 1.5 } };
static const int TON_PAL[NPAL] = { 0, -2, 3, 5, 7, -5 };
static const int RACINES[4] = { 0, 5, 3, -2 };
static const int TEINTE_DEG[7] = { 0, 2, 4, 1, 3, 5, 6 };

/* ---------- un timbre par motif ---------- */
typedef struct { int onde; double rap2, niv2, q, vol, dec; int bus, rythme; double fmR, fmI, oct, att; int vib; double vibF, vibD; int shepard, arp, det, trem; } Recette;
static Recette RECETTES[NMOTIFS + 1];
static void preparerRecettes(void){
  #define R_(m, ...) RECETTES[m] = (Recette){ __VA_ARGS__ }
  for(int k = 0; k <= NMOTIFS; k++) RECETTES[k] = (Recette){ .onde = ON_SINUS, .rap2 = 1.5, .niv2 = .4, .q = .8, .vol = .15, .dec = 1.3, .bus = BUS_REV, .rythme = G_AUCUN, .att = .14 };
  R_(M_BANDES,   .onde = ON_TRIANGLE, .rap2 = 2, .niv2 = .2, .q = .8, .vol = .16, .dec = .3, .bus = BUS_ECHO, .rythme = G_AUCUN, .arp = 1);
  R_(M_RAYURES,  .onde = ON_SINUS, .rap2 = 1.5, .niv2 = .15, .q = .8, .vol = .12, .dec = .2, .bus = BUS_ECHO, .rythme = G_TIC);
  R_(M_RAYONS,   .onde = ON_TRIANGLE, .rap2 = 1.004, .niv2 = .5, .q = .8, .vol = .12, .dec = .35, .bus = BUS_REV, .rythme = G_RIDE);
  R_(M_CERCLES,  .onde = ON_SINUS, .rap2 = 2, .niv2 = .12, .q = .8, .vol = .18, .dec = .6, .bus = BUS_ECHO, .fmR = 2, .fmI = .7, .oct = 2, .rythme = G_TOM);
  R_(M_DAMIER,   .onde = ON_TRIANGLE, .rap2 = 2, .niv2 = .3, .q = .8, .vol = .12, .dec = .18, .bus = BUS_SEC, .rythme = G_BOUM);
  R_(M_APLAT,    .onde = ON_SINUS, .rap2 = 1.5, .niv2 = .4, .q = .8, .vol = .15, .dec = 1.3, .bus = BUS_REV, .att = .14, .rythme = G_AUCUN);
  R_(M_SPIRALE,  .onde = ON_SINUS, .rap2 = 2, .niv2 = .4, .q = .8, .vol = .16, .dec = 1.1, .bus = BUS_REV, .vib = 1, .vibF = 3, .vibD = 12, .shepard = 1, .rythme = G_AUCUN);
  R_(M_HELICE,   .onde = ON_TRIANGLE, .rap2 = .5, .niv2 = .4, .q = .8, .vol = .13, .dec = .4, .bus = BUS_REV, .rythme = G_RONDE);
  R_(M_MOIRE,    .onde = ON_SINUS, .det = 1, .niv2 = 1, .q = .8, .vol = .14, .dec = 1.5, .bus = BUS_REV, .att = .05, .rythme = G_AUCUN);
  R_(M_TUNNEL,   .onde = ON_SINUS, .rap2 = .5, .niv2 = .3, .q = .8, .vol = .15, .dec = .5, .bus = BUS_ECHO, .rythme = G_BASSE);
  R_(M_CIBLE,    .onde = ON_SINUS, .rap2 = 2, .niv2 = .2, .q = .8, .vol = .16, .dec = .5, .bus = BUS_ECHO, .fmR = 3, .fmI = .5, .oct = 2, .rythme = G_CLOCHE);
  R_(M_ONDES,    .onde = ON_SINUS, .rap2 = 1.5, .niv2 = .12, .q = .8, .vol = .11, .dec = .6, .bus = BUS_REV, .rythme = G_SHAKER);
  R_(M_CHEVRONS, .onde = ON_TRIANGLE, .rap2 = 1.335, .niv2 = .3, .q = .8, .vol = .13, .dec = .2, .bus = BUS_ECHO, .rythme = G_CLAVE);
  R_(M_BULLE,    .onde = ON_SINUS, .rap2 = 2, .niv2 = .2, .q = 1, .vol = .15, .dec = .45, .bus = BUS_REV, .rythme = G_CONGA);
  R_(M_POIS,     .onde = ON_SINUS, .rap2 = 4, .niv2 = .08, .q = .8, .vol = .18, .dec = .14, .bus = BUS_ECHO, .fmR = 2, .fmI = .7, .oct = 2, .rythme = G_BOIS);
  R_(M_ZEBRE,    .onde = ON_SINUS, .rap2 = 2, .niv2 = .1, .q = .8, .vol = .15, .dec = .3, .bus = BUS_REV, .rythme = G_GLISSE);
  R_(M_LABYRINTHE, .onde = ON_SINUS, .rap2 = 2, .niv2 = .1, .q = .8, .vol = .15, .dec = .1, .bus = BUS_SEC, .rythme = G_MORSE);
  R_(M_ECAILLES, .onde = ON_SINUS, .rap2 = 2, .niv2 = .1, .q = .8, .vol = .15, .dec = .2, .bus = BUS_REV, .rythme = G_PLUIE);
  R_(M_HEXAGONES, .onde = ON_SINUS, .rap2 = 1.5, .niv2 = .3, .q = .8, .vol = .15, .dec = .3, .bus = BUS_REV, .rythme = G_ACCORD);
  R_(M_POLAIRE,  .onde = ON_SINUS, .rap2 = 2, .niv2 = .1, .q = .8, .vol = .15, .dec = 1.2, .bus = BUS_REV, .rythme = G_BOL);
  R_(M_ETOILE,   .onde = ON_SINUS, .rap2 = 2, .niv2 = .1, .q = .8, .vol = .15, .dec = .2, .bus = BUS_ECHO, .oct = 2, .rythme = G_SCINTILLE);
  R_(M_BRIQUES,  .onde = ON_SINUS, .rap2 = 2, .niv2 = .1, .q = .8, .vol = .15, .dec = .1, .bus = BUS_SEC, .rythme = G_TOC);
  R_(M_CROIX,    .onde = ON_SINUS, .rap2 = 2, .niv2 = .1, .q = .8, .vol = .15, .dec = .1, .bus = BUS_REV, .rythme = G_MAINS);
  R_(M_POINTES,  .onde = ON_SINUS, .rap2 = 2, .niv2 = .1, .q = .8, .vol = .15, .dec = .1, .bus = BUS_SEC, .rythme = G_EPINES);
  R_(M_PASTILLES, .onde = ON_TRIANGLE, .rap2 = 4, .niv2 = .15, .q = .8, .vol = .15, .dec = .2, .bus = BUS_SEC, .rythme = G_MARIMBA);
  R_(M_LOBES,    .onde = ON_SINUS, .rap2 = 2, .niv2 = .1, .q = .8, .vol = .15, .dec = 1.1, .bus = BUS_REV, .rythme = G_GONG);
  R_(M_CONTOURS, .onde = ON_SINUS, .rap2 = 2, .niv2 = .25, .q = .8, .vol = .15, .dec = 1.7, .bus = BUS_REV, .att = .22, .vib = 1, .vibF = 5, .vibD = 9, .rythme = G_AUCUN);
  R_(M_MEANDRE,  .onde = ON_TRIANGLE, .rap2 = 2, .niv2 = .15, .q = .8, .vol = .14, .dec = .12, .bus = BUS_SEC, .rythme = G_ESCALIER);
  R_(M_VERTIGE,  .onde = ON_SINUS, .rap2 = 1.5, .niv2 = .2, .q = .8, .vol = .14, .dec = .2, .bus = BUS_ECHO, .rythme = G_TOURNE);
  R_(M_ROSACE,   .onde = ON_SINUS, .rap2 = 3.01, .niv2 = .25, .q = .8, .vol = .14, .dec = .6, .bus = BUS_ECHO, .rythme = G_CRISTAL);
  R_(M_CORAIL,   .onde = ON_SINUS, .rap2 = 2, .niv2 = .1, .q = .8, .vol = .14, .dec = .05, .bus = BUS_REV, .rythme = G_GROUILLE);
  R_(M_SOLEILS,  .onde = ON_TRIANGLE, .rap2 = 2, .niv2 = .2, .q = .8, .vol = .14, .dec = .25, .bus = BUS_REV, .rythme = G_ECLAT);
  R_(M_FLEURS,   .onde = ON_SINUS, .rap2 = 2.4, .niv2 = .3, .q = .8, .vol = .14, .dec = .35, .bus = BUS_ECHO, .rythme = G_CLOCHETTE);
  R_(M_SPHERES,  .onde = ON_SINUS, .rap2 = 1.5, .niv2 = .3, .q = .8, .vol = .15, .dec = .9, .bus = BUS_REV, .trem = 1, .vibF = 5.5, .vibD = .55, .rythme = G_AUCUN);   /* la note tourne, comme un haut-parleur rotatif */
  R_(M_DECALAGE, .onde = ON_TRIANGLE, .rap2 = 2.01, .niv2 = .25, .q = .8, .vol = .15, .dec = .5, .bus = BUS_ECHO, .rythme = G_AUCUN);
  R_(M_TOURBILLON, .onde = ON_SINUS, .rap2 = 2, .niv2 = .2, .q = .8, .vol = .14, .dec = .25, .bus = BUS_ECHO, .rythme = G_REMOUS);
  R_(M_ARCHES,    .onde = ON_TRIANGLE, .rap2 = 2, .niv2 = .15, .q = .8, .vol = .14, .dec = .3, .bus = BUS_REV, .rythme = G_ARCHE);
  R_(M_PRISMES,   .onde = ON_CARRE, .rap2 = 1.5, .niv2 = .2, .q = .8, .vol = .1, .dec = .15, .bus = BUS_SEC, .rythme = G_BASCULE);
  R_(M_SOMMETS,   .onde = ON_SCIE, .rap2 = 2, .niv2 = .15, .q = .8, .vol = .1, .dec = .2, .bus = BUS_ECHO, .rythme = G_FANFARE);
  R_(M_FAILLES,   .onde = ON_SINUS, .rap2 = 1.5, .niv2 = .15, .q = .8, .vol = .12, .dec = .1, .bus = BUS_SEC, .rythme = G_DEPHASE);
  R_(M_ABIME,     .onde = ON_SINUS, .rap2 = .5, .niv2 = .4, .q = .8, .vol = .14, .dec = .6, .bus = BUS_REV, .rythme = G_GOUFFRE);
  R_(M_CADRES,    .onde = ON_CARRE, .rap2 = .5, .niv2 = .4, .q = .8, .vol = .1, .dec = .12, .bus = BUS_SEC, .rythme = G_CARREE);
  R_(M_LOSANGES,  .onde = ON_SINUS, .rap2 = 2, .niv2 = .1, .q = .8, .vol = .12, .dec = .05, .bus = BUS_REV, .rythme = G_CRECELLE);
  R_(M_FRISE,     .onde = ON_TRIANGLE, .rap2 = 2, .niv2 = .15, .q = .8, .vol = .14, .dec = .1, .bus = BUS_SEC, .rythme = G_PIZZ);
  #undef R_
}
static inline const Recette *recette(int motif){ return &RECETTES[motif >= 0 && motif < NMOTIFS ? motif : M_APLAT]; }
static inline double recOct(const Recette *R){ return R->oct ? R->oct : 1; }

/* ---------- styles de la partition : figures de seize pas ---------- */
typedef struct { const char *nom; double bpm, swing, rev, echo; const char *fig[NGENRES][2]; int tern; double humain; int orch[6], nOrch; } Style;
static Style STYLES[NSTYLES];
static void preparerStyles(void){
  STYLES[ST_LIBRE] = (Style){ "libre", 0, 0, 1, 1 };
  Style *m = &STYLES[ST_MINIMAL]; *m = (Style){ "minimal", 124, .14, .45, 1.1 };
  m->fig[G_BOUM][0] = "x---x---x---x---"; m->fig[G_BOUM][1] = "x---x---x--ox---"; m->fig[G_CLAIRE][0] = "----o-------o---"; m->fig[G_CLAIRE][1] = "----o--.----o---";
  m->fig[G_TIC][0] = "--o---o---o---o-"; m->fig[G_TIC][1] = "-.o.-.o.-.o.-.o."; m->fig[G_SHAKER][0] = "o.o.o.o.o.o.o.o."; m->fig[G_BOIS][0] = "---o--o----o--o-"; m->fig[G_BOIS][1] = "-o----o--o----o-";
  m->fig[G_CLAVE][0] = "--x----x--x-----"; m->fig[G_TOM][0] = "------o-------o."; m->fig[G_CLOCHE][0] = "x---------------"; m->fig[G_CLOCHE][1] = "x-------o-------";
  m->fig[G_RIDE][0] = "--------x-------"; m->fig[G_CONGA][0] = "-o---o.--o---o--"; m->fig[G_BASSE][0] = "x-----o---x-----"; m->fig[G_BASSE][1] = "x---------x--o--"; m->fig[G_RONDE][0] = "--x---x---x---x-";
  m = &STYLES[ST_DNB]; *m = (Style){ "dnb", 172, 0, .8, .8 };
  m->fig[G_BOUM][0] = "x---------x-----"; m->fig[G_BOUM][1] = "x-----o---x-----"; m->fig[G_CLAIRE][0] = "----x-------x---"; m->fig[G_CLAIRE][1] = "----x--.-.--x---";
  m->fig[G_TIC][0] = "o-o-o-o-o-o-o-o-"; m->fig[G_TIC][1] = "o.o-o.o-o.o-o.o."; m->fig[G_SHAKER][0] = "-.-.-.-.-.-.-.-."; m->fig[G_BOIS][0] = "--o--o----o--o--";
  m->fig[G_CLAVE][0] = "------x---------"; m->fig[G_CLAVE][1] = "------x------x--"; m->fig[G_TOM][0] = "-------------o.o"; m->fig[G_CLOCHE][0] = "x-------x-------";
  m->fig[G_RIDE][0] = "x---------------"; m->fig[G_CONGA][0] = "---o--o----o-o--"; m->fig[G_BASSE][0] = "x-----x---o-----"; m->fig[G_BASSE][1] = "x--o------x-----"; m->fig[G_RONDE][0] = "--o--o----o--o--";
  m = &STYLES[ST_JUNGLE]; *m = (Style){ "jungle", 160, 0, .85, 1.6 };
  m->fig[G_BOUM][0] = "x-x-------xx----"; m->fig[G_BOUM][1] = "x-x-------x-----"; m->fig[G_CLAIRE][0] = "----x--.-.--x--."; m->fig[G_CLAIRE][1] = "----x--.-.----x.";
  m->fig[G_TIC][0] = "o-o-o-o-o-o-o-o-"; m->fig[G_TIC][1] = "o.o-o.o-o.o-o.o-"; m->fig[G_SHAKER][0] = "..o...o...o...o."; m->fig[G_BOIS][0] = "--o---o--o----o-";
  m->fig[G_CLAVE][0] = "---x------x--x--"; m->fig[G_TOM][0] = "-------------ooo"; m->fig[G_CLOCHE][0] = "x---------------"; m->fig[G_RIDE][0] = "x-------x-------";
  m->fig[G_CONGA][0] = "-o---o.--o---o--"; m->fig[G_BASSE][0] = "x---------o-----"; m->fig[G_BASSE][1] = "x-------x-----o-"; m->fig[G_RONDE][0] = "--o---o---o---o-";
  m = &STYLES[ST_CHIP]; *m = (Style){ "chip", 150, 0, .12, .3 };
  m->fig[G_BOUM][0] = "x---x---x---x-o-"; m->fig[G_BOUM][1] = "x--ox---x--ox---"; m->fig[G_CLAIRE][0] = "----x-------x---"; m->fig[G_CLAIRE][1] = "----x--o----x-oo";
  m->fig[G_TIC][0] = "o-o-o-o-o-o-o-o-"; m->fig[G_TIC][1] = "oooooooooooooooo"; m->fig[G_SHAKER][0] = "--o---o---o---o-"; m->fig[G_BOIS][0] = "o--o--o-o--o--o-";
  m->fig[G_CLAVE][0] = "x--x--x---x-x---"; m->fig[G_TOM][0] = "------------oooo"; m->fig[G_CLOCHE][0] = "x---o---x---o---"; m->fig[G_RIDE][0] = "x-------x-------";
  m->fig[G_CONGA][0] = "-o-o--o--o-o--o-"; m->fig[G_BASSE][0] = "x-ox-ox-x-ox-ox-"; m->fig[G_BASSE][1] = "x--x--x-x--x-xo-"; m->fig[G_RONDE][0] = "o-o-o-o-o-o-o-o-"; m->fig[G_RONDE][1] = "ooo-ooo-ooo-ooo-";
  #define FIG(g, a, b) do{ m->fig[g][0] = a; m->fig[g][1] = b; }while(0)
  #define ORCH(...) do{ static const int o_[] = { __VA_ARGS__ }; m->nOrch = (int)(sizeof o_/sizeof *o_); memcpy(m->orch, o_, sizeof o_); }while(0)
  m = &STYLES[ST_MINIMAL]; ORCH(OR_SYNTHE, OR_OBJETS, OR_AUTOMATES, OR_METAL, OR_RADIO);
  m = &STYLES[ST_DNB]; ORCH(OR_SYNTHE, OR_METAL, OR_SONAR, OR_BANDE);
  m = &STYLES[ST_JUNGLE]; ORCH(OR_SYNTHE, OR_BANDE, OR_PEAUX, OR_RADIO);
  m = &STYLES[ST_CHIP]; ORCH(OR_SYNTHE, OR_AUTOMATES, OR_RADIO);
  /* House : grosse caisse à chaque temps, charleston ouvert sur le contretemps, basse décalée */
  m = &STYLES[ST_HOUSE]; *m = (Style){ "house", 124, .06, .7, .9 }; ORCH(OR_SYNTHE, OR_METAL, OR_OBJETS, OR_BANDE, OR_CHAMBRE);
  FIG(G_BOUM, "x---x---x---x---", NULL); FIG(G_CLAIRE, "----x-------x---", "----x-------x--."); FIG(G_TIC, "--x---x---x---x-", "--x---x---x---xo"); FIG(G_SHAKER, ".o.o.o.o.o.o.o.o", NULL);
  FIG(G_BOIS, "---o------o-----", NULL); FIG(G_CLAVE, "------x-----x---", NULL); FIG(G_TOM, "--------------o.", NULL); FIG(G_CLOCHE, "x---------------", "----------x-----"); FIG(G_RIDE, "--o---o---o---o-", NULL);
  FIG(G_CONGA, "---o--o----o-o--", NULL); FIG(G_BASSE, "--x---x---x---x-", "--x--ox---x--o-x"); FIG(G_RONDE, "o---o---o---o---", NULL);
  /* Dub techno : accords à contretemps noyés d'écho */
  m = &STYLES[ST_DUB]; *m = (Style){ "dub", 118, .04, .9, 1.9 }; ORCH(OR_SYNTHE, OR_SONAR, OR_BANDE, OR_RADIO, OR_METAL);
  FIG(G_BOUM, "x---x---x---x---", NULL); FIG(G_CLAIRE, "----------------", "------------x---"); FIG(G_TIC, "..o...o...o...o.", NULL); FIG(G_SHAKER, "-.-.-.-.-.-.-.-.", NULL);
  FIG(G_BOIS, "--------o-------", NULL); FIG(G_CLAVE, "---x------x-----", NULL); FIG(G_TOM, "-------------o--", NULL); FIG(G_CLOCHE, "--x-----------x-", "------x---------"); FIG(G_RIDE, "--o---o---o---o-", NULL);
  FIG(G_CONGA, "----o-------o---", NULL); FIG(G_BASSE, "x-----x---x-----", "x---------x--o--"); FIG(G_RONDE, "--x---x---x---x-", NULL);
  /* Hip-hop des origines : boom bap lent, très balancé, joué à la main */
  m = &STYLES[ST_HIPHOP]; *m = (Style){ "hiphop", 90, .2, .6, .7 }; m->humain = .15; ORCH(OR_BANDE, OR_CHAMBRE, OR_CORDES, OR_PEAUX, OR_CHOEUR);
  FIG(G_BOUM, "x------x--x-----", "x------x-x---x--"); FIG(G_CLAIRE, "----x-------x---", "----x-------x-.-"); FIG(G_TIC, "o.o.o.o.o.o.o.o.", "o.o.o.o.o.ooo.o."); FIG(G_SHAKER, "----o-------o---", NULL);
  FIG(G_BOIS, "--o-----o--o----", NULL); FIG(G_CLAVE, "------x---------", NULL); FIG(G_TOM, "-------------o--", NULL); FIG(G_CLOCHE, "x---------------", NULL); FIG(G_RIDE, "x-------x-------", NULL);
  FIG(G_CONGA, "-----o-----o----", NULL); FIG(G_BASSE, "x------x--x-----", "x------x-x---x--"); FIG(G_RONDE, "--o-------o-----", NULL);
  /* Afro 12/8 : tout bat en triolets ; la cloche joue la formule standard de sept coups sur douze */
  m = &STYLES[ST_AFRO]; *m = (Style){ "afro", 112, 0, .8, .8 }; m->tern = 1; m->humain = .08; ORCH(OR_PEAUX, OR_OBJETS, OR_CORDES, OR_CHOEUR);
  FIG(G_BOUM, "x-----x--x--", NULL); FIG(G_CLAIRE, "---x-----x--", "---x--x--x--"); FIG(G_TIC, "o-oo-oo-oo-o", NULL); FIG(G_SHAKER, "oo.oo.oo.oo.", NULL);
  FIG(G_BOIS, "x-x-xx-x-x-x", NULL); FIG(G_CLAVE, "x-x-xx-x-x-x", NULL); FIG(G_TOM, "------o--o-o", NULL); FIG(G_CLOCHE, "x-x-xx-x-x-x", NULL); FIG(G_RIDE, "x--x--x--x--", NULL);
  FIG(G_CONGA, "--o-oo--o-oo", "-oo--o-oo--o"); FIG(G_BASSE, "x-----x-----", "x--x--x-----"); FIG(G_RONDE, "-o--o--o--o-", NULL);
  /* Gamelan : deux parties qui s'imbriquent, le gong qui ferme le cycle, le kendang qui mène */
  m = &STYLES[ST_GAMELAN]; *m = (Style){ "gamelan", 96, 0, 1.2, .8 }; ORCH(OR_GAMELAN, OR_OBJETS, OR_AUTOMATES, OR_METAL);
  FIG(G_BOUM, "----------------", "---------------x"); FIG(G_CLAIRE, "--------x-------", NULL); FIG(G_TIC, "x-x-x-x-x-x-x-x-", NULL); FIG(G_SHAKER, "-x-x-x-x-x-x-x-x", NULL);
  FIG(G_BOIS, "x-xx-x-xx-x-xx-x", NULL); FIG(G_CLAVE, "-x--x-x--x-x--x-", NULL); FIG(G_TOM, "--o--o-o--o--o-o", NULL); FIG(G_CLOCHE, "---x---x---x---x", NULL); FIG(G_RIDE, "-------x-------x", NULL);
  FIG(G_CONGA, "o--o-o--o--o-o--", NULL); FIG(G_BASSE, "x-------x-------", NULL); FIG(G_RONDE, "--x---x---x---x-", NULL);
  /* Bossa nova : la clave 3-2, le surdo, la guitare en contretemps */
  m = &STYLES[ST_BOSSA]; *m = (Style){ "bossa", 128, .03, .7, .6 }; m->humain = .1; ORCH(OR_CORDES, OR_CHAMBRE, OR_SOUFFLES, OR_PEAUX);
  FIG(G_BOUM, "x--xx---x--xx---", NULL); FIG(G_CLAIRE, "x--x--x---x-x---", "--x-x--x--x--x--"); FIG(G_TIC, "o.o.o.o.o.o.o.o.", NULL); FIG(G_SHAKER, "oooooooooooooooo", NULL);
  FIG(G_BOIS, "x--x--x---x-x---", NULL); FIG(G_CLAVE, "x--x--x---x-x---", "--x-x---x--x--x-"); FIG(G_TOM, "-------------o--", NULL); FIG(G_CLOCHE, "x---------------", NULL); FIG(G_RIDE, "o-o-o-o-o-o-o-o-", NULL);
  FIG(G_CONGA, "--o--oo---o--oo-", NULL); FIG(G_BASSE, "x--x----x--x----", "x--x----x---x-x-"); FIG(G_RONDE, "--x--x----x--x--", NULL);
  /* Ambient : presque pas de battue ; des notes longues, rares, beaucoup d'espace */
  m = &STYLES[ST_AMBIENT]; *m = (Style){ "ambient", 72, 0, 1.7, 1.3 }; ORCH(OR_CHOEUR, OR_SOUFFLES, OR_ECOSYSTEME, OR_GAMELAN, OR_SONAR, OR_CORDES);
  FIG(G_BOUM, "----------------", "x---------------"); FIG(G_CLAIRE, "----------------", NULL); FIG(G_TIC, "--------o-------", "----------------"); FIG(G_SHAKER, "o---------------", NULL);
  FIG(G_BOIS, "----o-----------", "----------o-----"); FIG(G_CLAVE, "----------------", "-------x--------"); FIG(G_TOM, "----------------", NULL); FIG(G_CLOCHE, "x---------------", "--------o-------"); FIG(G_RIDE, "--------x-------", NULL);
  FIG(G_CONGA, "----------------", "------o---------"); FIG(G_BASSE, "x---------------", "----------------"); FIG(G_RONDE, "x-------o-------", NULL);
  /* Concret : chaque instrument tourne sur un cycle de longueur différente ; jeu humain, irrégulier */
  m = &STYLES[ST_CONCRET]; *m = (Style){ "concret", 104, 0, .9, 1.1 }; m->humain = .35; ORCH(OR_OBJETS, OR_ECOSYSTEME, OR_RADIO, OR_AUTOMATES, OR_CORDES);
  FIG(G_BOUM, "x--x-o-", NULL); FIG(G_CLAIRE, "--x-.", NULL); FIG(G_TIC, "o.o..o.oo", NULL); FIG(G_SHAKER, ".o..o.o..o.", NULL);
  FIG(G_BOIS, "x---o--x-----", NULL); FIG(G_CLAVE, "x--x--x-x-", NULL); FIG(G_TOM, "---o--", NULL); FIG(G_CLOCHE, "x--------------o-", NULL); FIG(G_RIDE, "--x----", NULL);
  FIG(G_CONGA, "o.-o--o", NULL); FIG(G_BASSE, "x------x--", NULL); FIG(G_RONDE, "-o--o", NULL);
  #undef FIG
  #undef ORCH
}
/* une figure de seize pas suit la mesure ; une figure d'une autre longueur tourne sur son propre cycle (polymétrie) */
static inline char car(const Son *so, const char *fig, int st){ size_t L = strlen(fig); return L == 16 ? fig[st % 16] : fig[so->pasTotal % (int)L]; }
static inline const char *figure(int style, int g, int droite){ const Style *S = &STYLES[style]; if(!S->fig[g][0]) return NULL; return droite && S->fig[g][1] ? S->fig[g][1] : S->fig[g][0]; }

/* ================= outils ================= */
static inline double tA(Syn *s);
static inline const Etape *E_(Syn *s);
static void accorder(Syn *s);
static double hz(Syn *s, double d){ const Etape *e = E_(s); int n = e->n, di = (int)floor(d + 1e-9), o = (int)floor((double)di/n), k = ((di % n) + n) % n;
  return 110*pow(2, (s->son->racine + e->g[k] + 12*o)/12.0); }
static inline int degreY(Syn *s, double y){ return (int)lround(borne(1 - y/s->H, 0, 1)*lround(E_(s)->n*2.8)); }
static inline double panX(Syn *s, double x){ return borne(x/s->W*1.7 - .85, -1, 1); }
static inline int euclide(int k, int n, int st){ return k > 0 && ((st*k) % n) < k; }
static inline int decalage(const Forme *f){ return f && f->ton ? TEINTE_DEG[f->ton % 7] : 0; }

static void biquadRegler(Biquad *b, int type, double f, double q, double sr){
  if(f > sr*.49) f = sr*.49; if(f < 10) f = 10;
  double w = 2*PI*f/sr, cw = cos(w), sw = sin(w), a0, al;
  if(type == FT_BANDE){ al = sw/(2*mx2(q, 1e-4)); a0 = 1 + al; b->b0 = al/a0; b->b1 = 0; b->b2 = -al/a0; }
  else { al = sw/(2*pow(10, q/20)); a0 = 1 + al;   /* passe-bas et passe-haut : Q en décibels, comme Web Audio */
    if(type == FT_HAUT){ b->b0 = (1 + cw)/2/a0; b->b1 = -(1 + cw)/a0; b->b2 = (1 + cw)/2/a0; }
    else { b->b0 = (1 - cw)/2/a0; b->b1 = (1 - cw)/a0; b->b2 = (1 - cw)/2/a0; } }
  b->a1 = -2*cw/a0; b->a2 = (1 - al)/a0;
}
static void biquadPic(Biquad *b, double f, double q, double gdb, double sr){   /* filtre en cloche (formants) */
  double A = pow(10, gdb/40), w = 2*PI*f/sr, al = sin(w)/(2*q), a0 = 1 + al/A;
  b->b0 = (1 + al*A)/a0; b->b1 = -2*cos(w)/a0; b->b2 = (1 - al*A)/a0; b->a1 = -2*cos(w)/a0; b->a2 = (1 - al/A)/a0;
}
static inline double biquad(Biquad *b, int c, double x){ double y = b->b0*x + b->z1[c]; b->z1[c] = b->b1*x - b->a1*y + b->z2[c]; b->z2[c] = b->b2*x - b->a2*y; return y; }

static inline double onde(int type, double ph, double dph){
  if(type == ON_SINUS) return sin(2*PI*ph);
  if(type == ON_TRIANGLE) return 1 - 4*fabs(ph - .5);   /* commence à -1 : sans importance ici */
  if(type == ON_SCIE){ double v = 2*ph - 1, t = ph;      /* dent de scie, adoucie au saut (polyBLEP) */
    if(t < dph){ t /= dph; v -= t + t - t*t - 1; } else if(t > 1 - dph){ t = (t - 1)/dph; v -= t*t + t + t + 1; } return v; }
  double v = ph < .5 ? 1 : -1;                          /* carré, adouci aux sauts (polyBLEP) */
  #define BLEP(t) ((t) < dph ? ((t)/dph)*2 - ((t)/dph)*((t)/dph) - 1 : (t) > 1 - dph ? (((t) - 1)/dph)*(((t) - 1)/dph) + (((t) - 1)/dph)*2 + 1 : 0)
  double t2 = ph + .5; if(t2 >= 1) t2 -= 1;
  return v + BLEP(ph) - BLEP(t2);
}

/* ================= espace surround =================
   Cinq enceintes autour de l'auditeur : avant gauche (-30°), centre (0°), avant droite (30°), surround droite (110°),
   surround gauche (-110°). Un son placé à un angle se partage entre les deux enceintes voisines à puissance constante ;
   l'étendue (0 à 1) le répand sur toutes, pour qu'il enveloppe au lieu de venir d'un point. */
static void placer(double angle, double etendue, double *g){
  static const double A[5] = { -110, -30, 0, 30, 110 }; static const int IDX[5] = { 3, 0, 2, 1, 4 };   /* SL, FL, C, FR, SR */
  double a = fmod(angle + 180, 360); if(a < 0) a += 360; a -= 180;
  double gg[5] = {0};
  int i = 0; while(i < 5 && A[i] <= a) i++;
  int k0, k1; double a0, a1;
  if(i == 0 || i == 5){ k0 = 4; k1 = 0; a0 = A[4]; a1 = A[0] + 360; if(a < A[0]) a += 360; }   /* derrière : entre SR et SL */
  else { k0 = i - 1; k1 = i; a0 = A[k0]; a1 = A[k1]; }
  double u = borne((a - a0)/(a1 - a0), 0, 1);
  gg[k0] = cos(u*PI/2); gg[k1] = sin(u*PI/2);
  double e = borne(etendue, 0, 1), uni = sqrt(.2), n2 = 0;
  for(int k = 0; k < 5; k++){ gg[k] = gg[k]*(1 - e) + uni*e; n2 += gg[k]*gg[k]; }
  n2 = 1/sqrt(n2 + 1e-12);
  for(int k = 0; k < 5; k++) g[IDX[k]] = gg[k]*n2;
  g[2] *= .8;   /* le centre, un peu en retrait : la musique se tient sur les côtés */
}
#define JG_NIVEAU .85   /* la jungle dans le mélange final */
#include "hc.inc"

/* ================= cordes de Karplus-Strong =================
   Un bruit bref circule dans une boucle qui s'amortit. Calculée une fois, gardée ; jouée à la bonne hauteur par interpolation. */
typedef struct { int N, kd, kc; long n; float *d; } CordeMem;
static CordeMem CORDES[96]; static int nCordes = 0;
static const float *cordeBuf(Syn *s, double f, double d, double clair, long *n, double *pas){
  double sr = s->son->sr, per = sr/borne(f, 30, 4000); int N = imx(2, (int)floor(per - .5)), kd = (int)lround(d*8), kc = (int)lround(clair*8);
  *pas = f*(N + .5)/sr;
  for(int i = 0; i < nCordes; i++) if(CORDES[i].N == N && CORDES[i].kd == kd && CORDES[i].kc == kc){ *n = CORDES[i].n; return CORDES[i].d; }
  if(nCordes >= 96){ for(int i = 0; i < nCordes; i++) free(CORDES[i].d); nCordes = 0; }
  long len = (long)ceil(sr*d); float *x = malloc(sizeof(float)*len), *boucle = malloc(sizeof(float)*N); double m = 0, crete = 1e-6;
  for(int i = 0; i < N; i++){ m += ((alea01(s)*2 - 1) - m)*(.2 + .8*clair); boucle[i] = (float)m; }
  double perte = pow(.001, 1/mx2(1, d*sr/N)), lis = .5 - .35*clair; int p = 0;
  for(long i = 0; i < len; i++){ double a = boucle[p], c = boucle[(p + 1) % N]; x[i] = (float)a; boucle[p] = (float)(perte*(a*(1 - lis) + c*lis)); p = (p + 1) % N; if(fabs(a) > crete) crete = fabs(a); }
  for(long i = 0; i < len; i++) x[i] /= (float)crete;
  free(boucle);
  CORDES[nCordes] = (CordeMem){ N, kd, kc, len, x }; nCordes++;
  *n = len; return x;
}

/* ================= coups et souffles ================= */
/* une place libre ; à défaut, la voix la plus éteinte cède la sienne (une queue presque muette plutôt qu'une note perdue) */
static Coup *place(Son *so){
  if(so->nbCoups < 96) for(int i = 0; i < 128; i++) if(!so->coups[i].actif) return &so->coups[i];
  Coup *m = NULL; for(int i = 0; i < 128; i++){ Coup *c = &so->coups[i]; if(c->actif && c->decroit && (!m || c->env < m->env)) m = c; }
  if(m && m->env < .02){ m->actif = 0; so->nbCoups--; return m; }
  return NULL;
}
static void poser(Syn *s, Coup *c, double t, double att, double dur, double vol, double pan, int bus){
  Son *so = s->son; double sr = so->sr;
  c->actif = 1; so->nbCoups++;
  c->debut = (long)(t*sr); c->pos = 0; c->att = att; c->dur = dur; c->vol = vol;
  c->fin = c->debut + (long)((att + dur + .05)*sr);
  c->env = 0; c->kEnv = pow(.0006/vol, 1/(dur*sr));
  double x = (borne(pan, -1, 1) + 1)/2; c->gl = cos(x*PI/2); c->gr = sin(x*PI/2);
  c->bus = bus; c->part = so->enPartition;
  if(so->nc > 2){   /* en surround : la profondeur suit le bus ; le sec devant, la réverbération sur les côtés, l'écho derrière */
    double prof = bus == BUS_SEC ? .12 : bus == BUS_REV ? .45 : .75, p = borne(pan, -1, 1), ang = p*(35 + prof*125);
    if(prof > .6 && fabs(p) < .15) ang = (alea01(s) < .5 ? -1 : 1)*(140 + alea01(s)*40);
    double g[5]; placer(ang, bus == BUS_SEC ? .12 : .3, g); for(int k = 0; k < 5; k++) c->g5[k] = (float)g[k];
  }
}

/* chaque style a son grain : retouche de toute note et de tout souffle qui ne sont pas des pièces de la batterie */
static void kitNote(Syn *s, Opt *o){
  switch(s->son->style){
    case ST_MINIMAL: o->dur = mn2(.45, (o->dur ? o->dur : .2)*.55); o->att = mn2(o->att ? o->att : .004, .003); o->onde = ON_SINUS; o->niv2 = 0; o->aFm = 0; if(o->bus == BUS_REV) o->bus = BUS_SEC; break;
    case ST_DNB: o->dur = (o->dur ? o->dur : .2)*.85; if(o->f < 160 && !o->niv2){ o->det = 1.4; o->niv2 = .8; } break;
    case ST_JUNGLE: o->dur = (o->dur ? o->dur : .2)*1.1; if(o->f > 250 && o->bus == BUS_SEC) o->bus = BUS_ECHO; break;
    case ST_CHIP: { int aigu = o->f >= 180; o->onde = aigu ? ON_CARRE : ON_TRIANGLE; o->niv2 = 0; o->aFm = 0; o->lfo = 0; o->aFiltre = 0; o->att = .002;
      o->dur = mn2(.35, (o->dur ? o->dur : .2)*.6); o->vol = (o->vol ? o->vol : .1)*(aigu ? .42 : 1.2); o->bus = BUS_SEC;
      if(aigu && o->dur >= .1){ o->arp = 1; o->glisse = 0; } break; }
    case ST_AMBIENT: o->dur = (o->dur ? o->dur : .2)*1.8; o->att = mx2(o->att ? o->att : .004, .03); o->vol = (o->vol ? o->vol : .1)*1.5; if(o->bus == BUS_SEC) o->bus = BUS_REV; break;
    case ST_DUB: if(o->bus == BUS_SEC && o->f > 200) o->bus = BUS_ECHO; break;
  }
}
static void kitSouffle(Syn *s, Opt *o){
  switch(s->son->style){
    case ST_MINIMAL: o->dur = (o->dur ? o->dur : .08)*.5; o->q = mx2(o->q ? o->q : 1.5, 2.2); o->att = mn2(o->att ? o->att : .003, .002); if(o->bus == BUS_REV) o->bus = BUS_SEC; break;
    case ST_DNB: o->f = (o->f ? o->f : 1000)*1.25; if(o->vers) o->vers *= 1.25; o->vol = (o->vol ? o->vol : .1)*1.1; break;
    case ST_JUNGLE: o->dur = (o->dur ? o->dur : .08)*1.2; if(o->bus == BUS_SEC) o->bus = BUS_ECHO; break;
    case ST_CHIP: o->rauque = 1; o->type = FT_HAUT; o->q = .5; o->vers = 0; o->dur = mn2(.09, (o->dur ? o->dur : .08)*.7); o->att = .001; o->vol = (o->vol ? o->vol : .1)*.7; o->bus = BUS_SEC; break;
    case ST_AMBIENT: o->dur = (o->dur ? o->dur : .08)*1.6; o->att = mx2(o->att ? o->att : .003, .02); if(o->bus == BUS_SEC) o->bus = BUS_REV; break;
    case ST_DUB: if(o->bus == BUS_SEC) o->bus = BUS_ECHO; break;
  }
}

static void jouerTonOrch(Syn *s, Opt o);
static void coup(Syn *s, Opt o){
  Son *so = s->son; if(!so->actif) return;
  if(!o.pur) kitNote(s, &o);
  if(!o.brut && so->orch){ jouerTonOrch(s, o); return; }   /* l'orchestre fait sonner la note avec son propre instrument */
  if(o.brut && o.vol) o.vol *= so->niveau;
  Coup *c = place(so); if(!c) return;
  double t = mx2(tA(s) + .002, o.t), att = o.att ? o.att : .004, dur = mx2(.02, o.dur ? o.dur : .2), vol = mn2(.5, o.vol ? o.vol : .1);
  memset(c, 0, sizeof *c);
  poser(s, c, t, att, dur, vol, o.pan, o.bus);
  double sr = so->sr, n = (att + dur)*sr;
  c->bruit = 0; c->onde = o.onde; c->f = o.f;
  c->kF = o.glisse && !o.arp ? pow(mx2(20, o.f*o.glisse)/o.f, 1/n) : 1;
  if(o.arp){ c->narp = 3; c->arp[0] = 1; c->arp[1] = 1.5; c->arp[2] = 2; }
  if(o.niv2){ c->niv2 = o.niv2; c->f2 = o.det ? o.f + o.det : o.f*(o.rap2 ? o.rap2 : 2); }
  if(o.aFm){ c->fmR = o.fmR; c->depFm = o.f*o.fmI; c->kFm = pow(mx2(.01, o.f*o.fmI*.08)/(o.f*o.fmI), 1/n); }
  c->lfo = o.lfo; c->lfoF = o.lfoF; c->lfoD = o.lfo == 2 ? o.f*(pow(2, o.lfoD/1200) - 1) : o.lfoD;
  if(o.aFiltre){ c->aFiltre = 1; c->ftype = o.ftype ? o.ftype : FT_BAS; c->ff = o.ff; c->fq = o.fq ? o.fq : 1;
    c->kFf = o.fvers ? pow(o.fvers/o.ff, 1/n) : 1; }
  c->ph1 = c->ph2 = c->phm = c->phl = 0;
  if(o.ks){ c->ks = cordeBuf(s, o.f, dur, o.clair, &c->ksN, &c->ksPas); c->ksPos = 0; }
}
static void souffle(Syn *s, Opt o){
  Son *so = s->son; if(!so->actif) return;
  if(!o.pur) kitSouffle(s, &o);
  if(o.brut && o.vol) o.vol *= so->niveau;
  Coup *c = place(so); if(!c) return;
  double t = mx2(tA(s) + .002, o.t), att = o.att ? o.att : .003, dur = mx2(.01, o.dur ? o.dur : .08);
  memset(c, 0, sizeof *c);
  poser(s, c, t, att, dur, mn2(.6, o.vol ? o.vol : .1), o.pan, o.bus);
  double n = (att + dur)*so->sr;
  c->bruit = o.rauque ? 2 : 1; c->aFiltre = 1; c->ftype = o.type ? o.type : FT_BANDE; c->ff = o.f; c->fq = o.q ? o.q : 1.5;
  c->kFf = o.vers ? pow(o.vers/o.f, 1/n) : 1;
  c->lecture = (long)(alea01(s)*1.8*so->sr);
}

/* une voix brève, échantillon par échantillon : rend le signal mono, déjà enveloppé */
static inline double jouerCoup(Son *so, Coup *c){
  double sr = so->sr, tl = c->pos/sr, x;
  if(tl < c->att) c->env = c->vol*tl/c->att;
  else if(!c->decroit){ c->decroit = 1; c->env = c->vol; }
  else if(c->env > .0006){ c->env *= c->kEnv; if(c->env < .0006) c->env = .0006; }
  if(c->ks){   /* corde : enveloppe tenue, puis éteinte sur le dernier sixième */
    long i0 = (long)c->ksPos; double fr = c->ksPos - i0;
    x = i0 + 1 < c->ksN ? c->ks[i0]*(1 - fr) + c->ks[i0 + 1]*fr : 0;
    c->ksPos += c->ksPas; if(tl < c->att + c->dur) c->ksPas *= c->kF;
    double reste = c->att + c->dur - tl; c->env = c->vol*(tl < c->att ? tl/c->att : reste < c->dur*.15 ? mx2(0, reste/(c->dur*.15)) : 1);
  }else if(c->bruit){
    const float *b = c->bruit == 2 ? so->bruit8 : so->bruit;
    x = b[c->lecture]; if(++c->lecture >= so->nBruit) c->lecture = 0;
  }else{
    double f = c->f;
    if(c->narp){ int k = (int)(tl/.022), q = imn(24, (int)((c->att + c->dur)/.022)); if(k > q) k = q; f = c->f*c->arp[k % 3]; }
    double fi = f;
    if(c->fmR){ double m = sin(2*PI*c->phm)*c->depFm; c->phm += f*c->fmR/sr; c->phm -= floor(c->phm); fi += m; if(tl < c->att + c->dur) c->depFm *= c->kFm; }
    double trem = 1;
    if(c->lfo){ double l = sin(2*PI*c->phl); c->phl += c->lfoF/sr; c->phl -= floor(c->phl);
      if(c->lfo == 2) fi += l*c->lfoD; else trem = (1 - c->lfoD*.5) + c->lfoD*.5*l; }
    double d1 = fi/sr; if(d1 < 0) d1 = -d1;
    x = onde(c->onde, c->ph1, d1); c->ph1 += fi/sr; c->ph1 -= floor(c->ph1);
    if(c->niv2){ double d2 = c->f2/sr; x += c->niv2*onde(c->onde, c->ph2, d2); c->ph2 += d2; c->ph2 -= floor(c->ph2); if(tl < c->att + c->dur) c->f2 *= c->kF; }
    if(tl < c->att + c->dur) c->f *= c->kF;
    x *= trem;
  }
  if(c->aFiltre){
    if((c->fcompte++ & 31) == 0){ Biquad b; biquadRegler(&b, c->ftype, c->ff, c->fq, sr); c->b0 = b.b0; c->b1 = b.b1; c->b2 = b.b2; c->a1 = b.a1; c->a2 = b.a2; }
    double y = c->b0*x + c->z1; c->z1 = c->b1*x - c->a1*y + c->z2; c->z2 = c->b2*x - c->a2*y; x = y;
    if(tl < c->att + c->dur) c->ff *= c->kFf;
  }
  return x*c->env;
}

static void bulle(Syn *s, double f, double vol, double pan, double t, int sens){
  double d = .13*f + .0072*pow(f, 1.5), dur = borne(6/d, .03, .15);
  coup(s, (Opt){ .f = f, .vol = vol*.75, .pan = pan, .t = t, .dur = dur, .att = .006, .glisse = sens < 0 ? .8 : 1.4, .bus = BUS_REV, .brut = 1 });   /* bulle ronde, sans attaque sèche */
}

/* note d'une forme, dans le timbre de son motif */
static void noteMotif(Syn *s, int motif, double f, double vol, double pan, double t, const Forme *forme, double etire){
  const Recette *R = recette(motif); double k = etire ? etire : 1;
  f *= recOct(R);
  if(R->shepard){
    int sens = forme && forme->sens < 0 ? -1 : 1;
    for(int o = -1; o <= 2; o++){ double pos = (o + .5)/3 - .5, w = exp(-pos*pos*7);
      coup(s, (Opt){ .f = f*pow(2, o), .glisse = pow(2, sens*.5), .vol = vol*R->vol*w*4.4, .pan = pan, .t = t, .dur = R->dec*k, .att = .05, .bus = R->bus }); }
    return;
  }
  if(R->arp){
    int d = (forme ? degreY(s, forme->cy) : 0) + decalage(forme); double r = f/hz(s, d);
    for(int a = 0; a < 3; a++) coup(s, (Opt){ .f = hz(s, d + a*2)*r, .onde = ON_TRIANGLE, .vol = vol*R->vol*5.5*(1 - a*.2), .pan = borne(pan + (a - 1)*.3, -1, 1),
      .t = mx2(tA(s), t) + a*s->son->dPas*.5, .dur = R->dec*k, .att = .004, .bus = R->bus, .rap2 = 2, .niv2 = .15,
      .aFiltre = 1, .ff = mn2(8000, f*6), .fvers = mx2(200, f*1.5), .fq = .8 });
    return;
  }
  Opt o = { .f = f, .onde = R->onde, .vol = vol*R->vol*5.5, .pan = pan, .t = t, .dur = R->dec*k, .att = R->att ? R->att : .004, .bus = R->bus,
            .rap2 = R->rap2, .niv2 = R->niv2, .det = R->det && forme ? 2 + (forme->id % 5) : 0 };
  if(R->fmR){ o.aFm = 1; o.fmR = R->fmR; o.fmI = R->fmI; }
  if(R->vib){ o.lfo = 2; o.lfoF = R->vibF; o.lfoD = R->vibD; }
  if(R->trem){ o.lfo = 1; o.lfoF = R->vibF; o.lfoD = R->vibD; }
  if(R->onde != ON_SINUS){ o.aFiltre = 1; o.ff = mn2(9000, f*6); o.fvers = mx2(200, f*1.5); o.fq = R->q; }
  coup(s, o);
}

/* ================= la batterie ================= */
typedef struct { double t, pan, haut, niv, fort, acc, grave, grave2; int i; } Piece;
#include "orch.inc"
static void caisseBreak(Syn *s, const Piece *c, double t, double w, double p, int court){
  souffle(s, (Opt){ .f = (3000 + 900*c->haut)*p, .q = .7, .vol = .16*c->niv*w, .dur = court ? .04 : .1, .att = .001, .t = t, .pan = c->pan, .bus = BUS_SEC, .pur = 1 });
  souffle(s, (Opt){ .rauque = 1, .f = 4200, .type = FT_HAUT, .q = .5, .vol = .03*c->niv*w, .dur = .05, .att = .001, .t = t, .pan = c->pan, .bus = BUS_SEC, .pur = 1 });
  coup(s, (Opt){ .f = 235*c->acc*p, .glisse = .8, .rap2 = 1.6, .niv2 = .3, .vol = .12*c->niv*w, .dur = .07, .att = .001, .t = t, .pan = c->pan, .bus = BUS_SEC, .pur = 1 });
}
/* pièce propre au style ; rend 0 si le style n'en a pas */
static int piece(Syn *s, int g, const Piece *c){
  double dPas = s->son->dPas;
  switch(s->son->style){
    case ST_MINIMAL: switch(g){
      case G_BOUM: coup(s, (Opt){ .f = 62*c->acc, .glisse = .7, .vol = .36*c->niv, .dur = .11, .att = .002, .t = c->t, .pan = c->pan*.3, .pur = 1 });
                   souffle(s, (Opt){ .f = 2500, .q = 2, .vol = .04*c->niv, .dur = .008, .att = .001, .t = c->t, .pan = c->pan*.3, .pur = 1 }); return 1;
      case G_CLAIRE: souffle(s, (Opt){ .f = 2200 + 500*c->haut, .q = 2.5, .vol = .09*c->niv, .dur = .035, .att = .001, .t = c->t, .pan = c->pan, .bus = BUS_ECHO, .pur = 1 });
                     coup(s, (Opt){ .f = 210*c->acc, .vol = .05*c->niv, .dur = .03, .att = .002, .t = c->t, .pan = c->pan, .pur = 1 }); return 1;
      case G_TIC: coup(s, (Opt){ .f = 6200 + 1400*c->haut, .vol = .035*c->niv*c->fort, .dur = .012, .att = .001, .t = c->t, .pan = (c->i & 1) ? .6 : -.6, .pur = 1 }); return 1;
      case G_SHAKER: souffle(s, (Opt){ .f = 5200, .q = 3, .vol = .05*c->niv, .dur = .012, .att = .001, .t = c->t, .pan = c->pan, .pur = 1 }); return 1;
      case G_BASSE: coup(s, (Opt){ .f = c->grave*.5, .vol = .3*c->niv, .dur = mn2(.16, dPas*1.2), .att = .004, .t = c->t, .pan = c->pan*.3, .pur = 1 }); return 1;
      case G_RONDE: coup(s, (Opt){ .f = c->grave2, .vol = .15*c->niv, .dur = .07, .att = .002, .t = c->t, .pan = c->pan*.5, .bus = BUS_ECHO, .pur = 1 }); return 1;
    } return 0;
    case ST_DNB: switch(g){
      case G_BOUM: coup(s, (Opt){ .f = 150*c->acc, .glisse = .3, .rap2 = 2, .niv2 = .2, .vol = .4*c->niv, .dur = .26, .att = .002, .t = c->t, .pan = c->pan*.3, .pur = 1 }); return 1;
      case G_CLAIRE: souffle(s, (Opt){ .f = 1800 + 600*c->haut, .q = .6, .vol = .2*c->niv, .dur = .16, .att = .002, .t = c->t, .pan = c->pan, .bus = BUS_REV, .pur = 1 });
                     coup(s, (Opt){ .f = 190*c->acc, .glisse = .75, .vol = .14*c->niv, .dur = .1, .att = .002, .t = c->t, .pan = c->pan, .pur = 1 }); return 1;
      case G_TIC: souffle(s, (Opt){ .f = 8200, .q = 1.2, .vol = .07*c->niv*c->fort, .dur = c->i == 0 ? .05 : .02, .att = .001, .t = c->t, .pan = (c->i & 1) ? .5 : -.5, .pur = 1 }); return 1;
      case G_SHAKER: souffle(s, (Opt){ .f = 6500, .q = 1, .vol = .045*c->niv, .dur = .03, .att = .002, .t = c->t, .pan = c->pan, .pur = 1 }); return 1;
      case G_BASSE: coup(s, (Opt){ .f = c->grave*.5, .det = 1.2, .niv2 = .9, .vol = .15*c->niv, .dur = mn2(1.1, dPas*6), .att = .02, .t = c->t, .pan = c->pan*.3, .lfo = 1, .lfoF = .5/dPas, .lfoD = .7, .pur = 1 }); return 1;
      case G_RONDE: coup(s, (Opt){ .f = c->grave2, .onde = ON_TRIANGLE, .glisse = .5, .vol = .15*c->niv, .dur = mn2(.4, dPas*1.6), .att = .004, .t = c->t, .pan = c->pan*.5, .bus = BUS_REV, .pur = 1 }); return 1;
    } return 0;
    case ST_JUNGLE: switch(g){
      case G_BOUM: coup(s, (Opt){ .f = 98*c->acc, .glisse = .55, .rap2 = 2, .niv2 = .3, .vol = .36*c->niv, .dur = .13, .att = .002, .t = c->t, .pan = c->pan*.3, .pur = 1 }); return 1;
      case G_CLAIRE: caisseBreak(s, c, c->t, 1, 1, c->niv < .5); return 1;
      case G_TRANCHE: { double p = .8 + .8*c->haut;
        if(c->haut < .34){ for(int k = 0; k < 2; k++) coup(s, (Opt){ .f = 98*c->acc*p*1.3, .glisse = .55, .rap2 = 2, .niv2 = .3, .vol = .3*c->niv, .dur = .1, .att = .002, .t = c->t + k*dPas*.5, .pan = c->pan*.4, .pur = 1 }); }
        else if(c->haut < .67){ for(int k = 0; k < 3; k++) caisseBreak(s, c, c->t + k*dPas*.5, pow(.8, k), p*(1 + k*.12), 1); }
        else for(int k = 0; k < 4; k++) souffle(s, (Opt){ .f = 5600*p, .q = 1.6, .vol = .06*c->niv*(1 - k*.15), .dur = .035, .att = .001, .t = c->t + k*dPas*.5, .pan = c->pan, .bus = BUS_ECHO, .pur = 1 });
        return 1; }
      case G_TIC: souffle(s, (Opt){ .f = 5600, .q = 1.6, .vol = .05*c->niv*c->fort, .dur = .13, .att = .002, .t = c->t, .pan = .35, .pur = 1 });
                  souffle(s, (Opt){ .rauque = 1, .f = 7000, .type = FT_HAUT, .q = .5, .vol = .014*c->niv, .dur = .06, .att = .001, .t = c->t, .pan = .35, .pur = 1 }); return 1;
      case G_SHAKER: souffle(s, (Opt){ .f = 4600, .q = 1.2, .vol = .035*c->niv, .dur = .03, .att = .004, .t = c->t, .pan = c->pan, .pur = 1 }); return 1;
      case G_TOM: coup(s, (Opt){ .f = (120 + 90*c->haut)*c->acc, .glisse = .6, .vol = .24*c->niv, .dur = .2, .att = .003, .t = c->t, .pan = c->pan, .pur = 1 }); return 1;
      case G_BASSE: coup(s, (Opt){ .f = mx2(36, c->grave*.25), .glisse = .8, .rap2 = 2, .niv2 = .22, .vol = .25*c->niv, .dur = mn2(1.5, dPas*9), .att = .012, .t = c->t, .pan = 0, .pur = 1 }); return 1;
      case G_RONDE: for(int k = 0; k < 2; k++) coup(s, (Opt){ .f = c->grave2*(k ? 3 : 2), .onde = ON_TRIANGLE, .vol = .07*c->niv, .dur = .07, .att = .003, .t = c->t, .pan = c->pan*.5, .bus = BUS_ECHO, .pur = 1 }); return 1;
      case G_CLOCHE: { double f = 620*c->acc*(1 + .3*c->haut);
        coup(s, (Opt){ .f = f, .glisse = 1.9, .vol = .06*c->niv, .dur = .22, .att = .02, .t = c->t, .pan = c->pan, .bus = BUS_ECHO, .pur = 1 });
        coup(s, (Opt){ .f = f*1.9, .glisse = .53, .vol = .06*c->niv, .dur = .3, .att = .02, .t = c->t + .24, .pan = -c->pan, .bus = BUS_ECHO, .pur = 1 }); return 1; }
    } return 0;
    case ST_CHIP: switch(g){
      case G_BOUM: coup(s, (Opt){ .f = 220*c->acc, .onde = ON_TRIANGLE, .glisse = .2, .vol = .42*c->niv, .dur = .07, .att = .001, .t = c->t, .pan = c->pan*.3, .pur = 1 }); return 1;
      case G_CLAIRE: souffle(s, (Opt){ .rauque = 1, .f = 1800, .type = FT_HAUT, .q = .5, .vol = .1*c->niv, .dur = .07, .att = .001, .t = c->t, .pan = c->pan, .pur = 1 }); return 1;
      case G_TIC: souffle(s, (Opt){ .rauque = 1, .f = 6000, .type = FT_HAUT, .q = .5, .vol = .05*c->niv*c->fort, .dur = .02, .att = .001, .t = c->t, .pan = (c->i & 1) ? .5 : -.5, .pur = 1 }); return 1;
      case G_SHAKER: souffle(s, (Opt){ .rauque = 1, .f = 7500, .type = FT_HAUT, .q = .5, .vol = .04*c->niv, .dur = .012, .att = .001, .t = c->t, .pan = c->pan, .pur = 1 }); return 1;
      case G_BASSE: coup(s, (Opt){ .f = c->grave*.5, .onde = ON_TRIANGLE, .vol = .36*c->niv, .dur = mn2(.2, dPas*1.6), .att = .002, .t = c->t, .pan = c->pan*.3, .pur = 1 }); return 1;
      case G_RONDE: coup(s, (Opt){ .f = c->grave2, .onde = ON_CARRE, .arp = 1, .vol = .06*c->niv, .dur = mn2(.3, dPas*2.4), .att = .002, .t = c->t, .pan = c->pan*.5, .pur = 1 }); return 1;
    } return 0;
  }
  return 0;
}

static void frappe(Syn *s, int genre, double x, double y, double niv, double t, int i, int dc){
  Son *so = s->son;
  double pan = panX(s, x), haut = borne(1 - y/s->H, 0, 1), fort = i == 0 ? 1.25 : 1; int n = E_(s)->n;
  int d = degreY(s, y) + dc; double acc = 1 + .05*dc, dPas = so->dPas;
  t = mx2(tA(s) + .002, t);
  if(so->enPartition && STYLES[so->style].humain){ double h = STYLES[so->style].humain; t += alea01(s)*dPas*h*.35; niv *= 1 - h*.45*alea01(s); }   /* le jeu humain */
  Piece c = { t, pan, haut, niv, fort, acc, hz(s, lround(haut*n*1.2) + dc), hz(s, lround(haut*n*1.5) + dc), i };
  if(so->orch){ int cat = percDe(genre); if(cat != PC_AUCUN){ jouerPercOrch(s, cat, &c); return; } }
  if(piece(s, genre, &c)) return;
  switch(genre){
    case G_BOUM: coup(s, (Opt){ .f = (118 + 20*haut)*acc, .glisse = .43, .rap2 = 2, .niv2 = .25, .vol = .34*niv, .dur = .2, .att = .005, .t = t, .pan = pan*.3 }); break;
    case G_CLAIRE: souffle(s, (Opt){ .f = (850 + 700*haut)*acc, .q = .7, .vol = .11*niv, .dur = .1, .att = .006, .t = t, .pan = pan, .bus = BUS_REV });
                   coup(s, (Opt){ .f = 175*acc, .glisse = .8, .vol = .07*niv, .dur = .08, .att = .004, .t = t, .pan = pan }); break;
    case G_TIC: souffle(s, (Opt){ .f = (4200 + 1600*haut)*acc, .q = 1.1, .vol = .1*niv*fort, .dur = i == 0 ? .09 : .03, .att = .004, .t = t, .pan = (i & 1) ? .7 : -.7, .bus = BUS_ECHO }); break;
    case G_BOIS: coup(s, (Opt){ .f = hz(s, d + n), .aFm = 1, .fmR = 2, .fmI = .6, .vol = .13*niv*fort, .dur = .14, .att = .004, .t = t, .pan = pan });
                 coup(s, (Opt){ .f = hz(s, d + n)*2, .vol = .03*niv, .dur = .25, .att = .004, .t = t + .012, .pan = pan, .bus = BUS_ECHO }); break;
    case G_CLAVE: for(int k = 0; k < 3; k++) coup(s, (Opt){ .f = hz(s, d + n*2 + ((i & 1) ? 2 - k : k)), .vol = .15*niv*fort*pow(.55, k), .dur = .045, .att = .003,
                    .t = t + k*dPas*.3, .pan = borne(pan + (k - 1)*.3, -1, 1) }); break;
    case G_TOM: coup(s, (Opt){ .f = (150 + 180*haut)*acc, .glisse = .55, .rap2 = 1.5, .niv2 = .12, .vol = .26*niv*fort, .dur = .5, .att = .006, .t = t, .pan = pan, .bus = BUS_REV }); break;
    case G_CLOCHE: coup(s, (Opt){ .f = hz(s, d + n*2), .rap2 = 2.76, .niv2 = .2, .vol = .075*niv*fort, .dur = .9, .att = .004, .t = t, .pan = pan, .bus = BUS_ECHO }); break;
    case G_RIDE: for(int k = 0; k < 4; k++) coup(s, (Opt){ .f = hz(s, d + n + k*2), .onde = ON_TRIANGLE, .vol = .07*niv*fort*(1 - k*.12), .dur = .38, .att = .004,
                   .t = t + k*.03, .pan = borne(pan - .35 + k*.23, -1, 1), .bus = BUS_REV, .aFiltre = 1, .ff = 3200, .fvers = 900, .fq = .7 }); break;
    case G_CONGA: coup(s, (Opt){ .f = (300 + 380*haut)*acc, .glisse = 1.9, .vol = .2*niv*fort, .dur = .085, .att = .004, .t = t, .pan = pan, .bus = BUS_REV }); break;
    case G_BASSE: coup(s, (Opt){ .f = hz(s, lround(haut*n*1.2) + dc)*.5, .glisse = .92, .rap2 = 2, .niv2 = .45, .vol = .26*niv, .dur = mn2(.75, dPas*3.6), .att = .014, .t = t, .pan = pan*.3 }); break;
    case G_RONDE: coup(s, (Opt){ .f = hz(s, lround(haut*n*1.5) + dc)*(i % 3 == 2 ? 2 : 1), .onde = ON_TRIANGLE, .vol = .2*niv*fort, .dur = mn2(.5, dPas*2.6), .att = .012, .t = t, .pan = pan*.5, .bus = BUS_ECHO,
                    .lfo = 1, .lfoF = 1.5/dPas, .lfoD = .85, .aFiltre = 1, .ff = 900 + 900*haut, .fvers = 340, .fq = .8 }); break;
    case G_GLISSE: { double a = hz(s, d + n), b = hz(s, d + n + 3); coup(s, (Opt){ .f = (i & 1) ? a : b, .glisse = (i & 1) ? b/a : a/b, .vol = .11*niv*fort, .dur = .2, .att = .02, .t = t, .pan = pan, .bus = BUS_REV }); break; }
    case G_MORSE: { int a = alea01(s) < .5 ? 0 : 4;
      coup(s, (Opt){ .f = hz(s, d + n*2 + a), .vol = .15*niv*fort, .dur = .035, .att = .003, .t = t, .pan = a ? .5 : -.5 });
      coup(s, (Opt){ .f = hz(s, d + n*2 + 4 - a), .vol = .11*niv, .dur = .035, .att = .003, .t = t + dPas*.5, .pan = a ? -.5 : .5 }); break; }
    case G_PLUIE: for(int k = 0; k < 3; k++) bulle(s, hz(s, d + n*2 + (int)(alea01(s)*n))*2, .12*niv, alea01(s)*1.6 - .8, t + alea01(s)*dPas*.9, 1); break;
    case G_ACCORD: for(int k = 0; k < 3; k++) coup(s, (Opt){ .f = hz(s, d + n + k*2), .vol = .07*niv*fort, .dur = .24, .att = .012, .t = t, .pan = borne(pan + (k - 1)*.2, -1, 1), .bus = BUS_REV }); break;
    case G_BOL: coup(s, (Opt){ .f = hz(s, d + n), .det = 1.6, .niv2 = .9, .vol = .09*niv, .dur = 1.7, .att = .02, .t = t, .pan = pan, .bus = BUS_REV });
                coup(s, (Opt){ .f = hz(s, d + n)*2.71, .vol = .02*niv, .dur = .9, .att = .01, .t = t, .pan = pan, .bus = BUS_REV }); break;
    case G_SCINTILLE: for(int k = 0; k < 2; k++) coup(s, (Opt){ .f = hz(s, d + n*3 + k*3), .vol = .085*niv*fort, .dur = .12, .att = .003, .t = t + k*dPas*.5, .pan = k ? .8 : -.8, .bus = BUS_ECHO }); break;
    case G_TOC: coup(s, (Opt){ .f = (330 + 200*haut)*acc, .glisse = .6, .vol = .2*niv*fort, .dur = .04, .att = .002, .t = t, .pan = pan }); break;
    case G_MAINS: for(int k = 0; k < 3; k++) souffle(s, (Opt){ .f = (1300 + 500*haut)*acc, .q = 1.2, .vol = .2*niv*fort, .dur = k == 2 ? .08 : .012, .att = .003, .t = t + k*.011, .pan = pan, .bus = BUS_REV }); break;
    case G_SHAKER: souffle(s, (Opt){ .f = (420 + 380*haut)*acc, .vers = (800 + 500*haut)*acc, .q = .5, .vol = .085*niv*fort, .dur = .22, .att = mn2(.2, dPas*1.2), .t = t, .pan = pan, .bus = BUS_REV }); break;
    case G_EPINES: for(int k = 0; k < 4; k++) coup(s, (Opt){ .f = hz(s, d + n + k*2), .vol = .075*niv*fort*(1 - k*.15), .dur = .035, .att = .002, .t = t + k*.024, .pan = borne(pan + (k - 1.5)*.15, -1, 1) }); break;
    case G_MARIMBA: coup(s, (Opt){ .f = hz(s, d + n), .onde = ON_TRIANGLE, .rap2 = 4, .niv2 = .12, .vol = .15*niv*fort, .dur = .2, .att = .003, .t = t, .pan = pan }); break;
    /* Méandre : escalier, quatre marches qui montent ou descendent */
    case G_ESCALIER: for(int k = 0; k < 4; k++){ int m = (i & 1) ? 3 - k : k; coup(s, (Opt){ .f = hz(s, d + n + m), .onde = ON_TRIANGLE, .vol = .12*niv*fort*(1 - k*.1), .dur = .06, .att = .003, .t = t + k*dPas*.25, .pan = borne(pan + (k - 1.5)*.25, -1, 1), .bus = BUS_SEC }); } break;
    /* Vertige : arpège qui tourne autour de l'auditeur */
    case G_TOURNE: for(int k = 0; k < 6; k++) coup(s, (Opt){ .f = hz(s, d + n + k), .glisse = 1.06, .vol = .08*niv*fort*(1 - k*.08), .dur = .15, .att = .004, .t = t + k*.035, .pan = sin(k*1.05)*.8, .bus = BUS_ECHO }); break;
    /* Rosace : cristal et sa quinte de l'autre côté */
    case G_CRISTAL: coup(s, (Opt){ .f = hz(s, d + n*2), .rap2 = 3.01, .niv2 = .25, .vol = .07*niv*fort, .dur = .6, .att = .002, .t = t, .pan = pan, .bus = BUS_ECHO });
                    coup(s, (Opt){ .f = hz(s, d + n*2)*1.5, .vol = .03*niv, .dur = .4, .att = .002, .t = t + .05, .pan = -pan, .bus = BUS_ECHO }); break;
    /* Corail : fourmillement de petits clics */
    case G_GROUILLE: for(int k = 0; k < 5; k++) coup(s, (Opt){ .f = hz(s, d + n + (int)(alea01(s)*n*2))*2, .glisse = 1.3, .vol = .06*niv*fort, .dur = .03, .att = .002, .t = t + alea01(s)*dPas*.6, .pan = borne(pan + (alea01(s) - .5)*.8, -1, 1), .bus = BUS_REV }); break;
    /* Soleils : éclat, un accord frappé d'un coup et un voile de cymbale */
    case G_ECLAT: for(int k = 0; k < 5; k++) coup(s, (Opt){ .f = hz(s, d + n + k*2), .onde = ON_TRIANGLE, .vol = .05*niv*fort, .dur = .25, .att = .002, .t = t, .pan = borne(pan + (k - 2)*.2, -1, 1), .bus = BUS_REV, .aFiltre = 1, .ff = 5000, .fvers = 1200, .fq = .7 });
                  souffle(s, (Opt){ .f = 7000, .q = 1, .vol = .04*niv, .dur = .12, .att = .002, .t = t, .pan = pan, .bus = BUS_REV }); break;
    /* Fleurs : carillon à vent */
    /* les voix des motifs d'op art */
    case G_REMOUS: for(int k = 0; k < 7; k++) coup(s, (Opt){ .f = hz(s, d + n + k), .glisse = 1.04, .vol = .075*niv*fort*(1 - k*.09), .dur = .11, .att = .003, .t = t + (1 - pow(.78, k))*dPas*2.4, .pan = cos(k*1.3)*.85, .bus = BUS_ECHO }); break;
    case G_ARCHE: { static const int A[6] = {0, 2, 4, 7, 4, 2}; for(int k = 0; k < 6; k++) coup(s, (Opt){ .f = hz(s, d + n + A[k]), .onde = ON_TRIANGLE, .vol = .09*niv*fort*(k == 3 ? 1.2 : 1), .dur = .13, .att = .004, .t = t + k*dPas*.2, .pan = borne(pan + (k - 2.5)*.15, -1, 1), .bus = BUS_REV }); break; }
    case G_BASCULE: for(int k = 0; k < 2; k++) coup(s, (Opt){ .f = hz(s, d + n + (k ? 4 : 0)), .onde = ON_CARRE, .aFiltre = 1, .ff = 2600, .fvers = 700, .fq = 1, .vol = .06*niv*fort, .dur = .09, .att = .003, .t = t + k*dPas*.5, .pan = k ? .65 : -.65, .bus = BUS_SEC }); break;
    case G_FANFARE: { static const int F[3] = {0, 2, 4}; for(int k = 0; k < 3; k++) coup(s, (Opt){ .f = hz(s, d + n + F[k]), .onde = ON_SCIE, .aFiltre = 1, .ff = 3200, .fvers = 900, .fq = .9, .vol = .07*niv*fort*(k == 2 ? 1.25 : 1), .dur = k == 2 ? .22 : .07, .att = .004, .t = t + k*dPas/3, .pan = pan, .bus = BUS_ECHO }); break; }
    case G_DEPHASE: souffle(s, (Opt){ .f = (3000 + 1500*haut)*acc, .q = 6, .vol = .12*niv*fort, .dur = .025, .att = .002, .t = t, .pan = -.5, .bus = BUS_SEC });
                    souffle(s, (Opt){ .f = (3000 + 1500*haut)*acc*1.5, .q = 6, .vol = .09*niv*fort, .dur = .025, .att = .002, .t = t + dPas*.33, .pan = .5, .bus = BUS_ECHO }); break;
    case G_GOUFFRE: coup(s, (Opt){ .f = hz(s, d + n*2), .glisse = .25, .rap2 = .5, .niv2 = .4, .vol = .12*niv*fort, .dur = .55, .att = .006, .t = t, .pan = pan, .bus = BUS_REV }); break;
    case G_CARREE: coup(s, (Opt){ .f = hz(s, d + n), .onde = ON_CARRE, .rap2 = .5, .niv2 = .5, .aFiltre = 1, .ff = 2400, .fvers = 500, .fq = 1.2, .vol = .08*niv*fort, .dur = .1, .att = .003, .t = t, .pan = pan, .bus = BUS_SEC }); break;
    case G_CRECELLE: for(int k = 0; k < 6; k++) souffle(s, (Opt){ .f = (2200 + 900*haut + k*120)*acc, .q = 2.2, .vol = .07*niv*fort*(1 - k*.1), .dur = .014, .att = .002, .t = t + k*.019, .pan = (k & 1) ? .6 : -.6, .bus = BUS_REV }); break;
    case G_PIZZ: for(int k = 0; k < 2; k++) coup(s, (Opt){ .f = hz(s, d + (k ? n + 4 : 0)), .onde = ON_TRIANGLE, .rap2 = 2, .niv2 = .15, .glisse = .985, .vol = .13*niv*fort, .dur = .08, .att = .002, .t = t + k*dPas*.5, .pan = k ? .35 : -.35, .bus = BUS_SEC }); break;
    case G_CLOCHETTE: for(int k = 0; k < 3; k++) coup(s, (Opt){ .f = hz(s, d + n*2 + 4 - k*2), .rap2 = 2.4, .niv2 = .3, .vol = .07*niv*fort*(1 - k*.2), .dur = .3, .att = .002, .t = t + k*.06 + alea01(s)*.015, .pan = borne(pan + (k - 1)*.35, -1, 1), .bus = BUS_ECHO }); break;
    case G_GONG: coup(s, (Opt){ .f = hz(s, d)*.5, .det = 2.4, .niv2 = .85, .glisse = .95, .vol = .13*niv, .dur = 1.2, .att = .03, .t = t, .pan = pan, .bus = BUS_REV });
                 coup(s, (Opt){ .f = hz(s, d), .vol = .03*niv, .dur = .5, .att = .02, .t = t, .pan = pan, .bus = BUS_REV }); break;
  }
}

/* ================= espace, tempo, voyage ================= */
static inline const Etape *E_(Syn *s){ return &ETAPES[s->son->etape]; }
static void accorder(Syn *s){ Son *so = s->son; so->racine = TON_PAL[s->P.palette] + E_(s)->racine + so->derive + so->tours*2; }
static void sonTempo(Syn *s){
  Son *so = s->son; double b = borne(so->tempo*s->P.vitesse*E_(s)->tempo, 30, 280); so->dPas = 60/b/(STYLES[so->style].tern ? 3 : 4);   /* les styles ternaires battent en triolets */
  if(so->hardcore) so->dPas = 60/borne(so->tempo, 120, 200)/4;   /* le Hardcore garde son tempo tel quel, en doubles croches */
  viser(&so->retard, mn2(1.4, so->dPas*3), .05);
}
static void espace(Syn *s){
  Son *so = s->son; const Etape *b0 = E_(s); int f = s->P.fond;
  double k = f == FOND_NOIR ? 1.45 : FOND_ANIME(f) ? 1 : .75; const Style *ks = &STYLES[so->style];
  double d = mn2(5, b0->d*k), sombre = mn2(.93, b0->sombre + (f == FOND_NOIR ? .1 : 0)), rev = mn2(.7, b0->rev*k*ks->rev);
  double echo = mn2(.6, (b0->echo + (FOND_ANIME(f) ? .14 : 0))*ks->echo), boucle = mn2(.62, b0->boucle + (FOND_ANIME(f) ? .1 : 0));
  /* réverbération : durée de décroissance d'après la longueur de la réponse d'origine, amortissement d'après sa couleur */
  double t60 = d*1.1;
  so->revFb = t60; so->revAmort = sombre*.85;
  viser(&so->revRetour, rev, .1); viser(&so->retRetour, echo, .1); viser(&so->retBoucle, boucle, .1);
}
static void passer(Syn *s){
  Son *so = s->son;
  so->avance = 0; so->etape++; if(so->etape >= 7){ so->etape = 0; so->tours++; }
  so->derive = 0; so->mesures = 0; accorder(s); sonTempo(s); espace(s);
  if(!so->actif) return;
  double t = tA(s); int n = E_(s)->n;
  souffle(s, (Opt){ .f = 300, .vers = 2400, .q = .7, .type = FT_BAS, .vol = .05, .dur = 1.1, .att = .5, .bus = BUS_REV });
  for(int o = -1; o <= 2; o++) coup(s, (Opt){ .f = hz(s, n)*pow(2, o), .glisse = 1.41, .vol = .05, .dur = 1.3, .att = .3, .bus = BUS_REV });
  for(int k = 0; k < 5; k++) coup(s, (Opt){ .f = hz(s, n*2 + k*2), .onde = ON_TRIANGLE, .vol = .07, .dur = .9, .t = t + 1.15 + k*.09, .pan = k*.35 - .7, .bus = BUS_ECHO });
}
static void son_chemin(Syn *s, double d){ if(!s->son) return; s->son->avance += d; if(s->son->avance >= 9*(s->W + s->H)) passer(s); }

/* ================= voix continue du geste ================= */
static void fermerVoix(Syn *s){ Son *so = s->son; if(!so->voix.ouverte || so->voix.fermeture) return; so->voix.fermeture = 1; viser(&so->voix.vca, 0, .05); so->voix.tFin = tA(s) + .4; }
static void ouvrirVoix(Syn *s, Traceur *tr, int genre){
  Son *so = s->son; memset(&so->voix, 0, sizeof so->voix);
  so->voix.ouverte = 1; so->voix.genre = genre; so->voix.forme = tr->forme; so->voix.deg = -99; so->voix.angle = 1e9;
  so->voix.motif = genre == PIN_MOLLE && tr->forme ? tr->forme->motif : M_LIQUIDE;
  so->voix.lecture = (long)(alea01(s)*1.5*so->sr);
  so->voix.f1.v = so->voix.f1.c = 220; so->voix.f2.v = so->voix.f2.c = 330; so->voix.ff.v = so->voix.ff.c = genre == PIN_MOLLE ? 500 : 800;
  so->voix.q = genre == PIN_MOLLE ? recette(so->voix.motif)->q : .8;
  { const Trait *T = &TRAITS[so->orch]; so->voix.onde = T->onde >= 0 ? T->onde : recette(so->voix.motif)->onde; so->voix.clair = T->clair; so->voix.souffle = T->souffle; so->voix.formants = T->formants;
    static const double PK[3][2] = { {700, 11}, {1150, 8}, {2700, 6} }; for(int k = 0; k < 3; k++) biquadPic(&so->voix.pk[k], PK[k][0], 4, PK[k][1], so->sr);
    so->voix.sfLecture = (long)(alea01(s)*so->nBruit) % so->nBruit; }
}
static void son_suivre(Syn *s, Traceur *tr, int genre, double poids){
  Son *so = s->son; if(!so || !so->actif) return;
  const Recette *R0 = genre == PIN_MOLLE && tr->forme ? recette(tr->forme->motif) : NULL;
  if(R0 && R0->rythme != G_AUCUN){
    if(!so->roul.actif || so->roul.forme != tr->forme){
      so->roul.actif = 1; so->roul.forme = tr->forme; so->roul.i = 1; fermerVoix(s);
      frappe(s, R0->rythme, tr->x, tr->y, 1, 0, 0, decalage(tr->forme));
    }
    so->roul.vu = s->dernier; so->roul.vit = hypot(tr->vx, tr->vy); so->roul.x = tr->x; so->roul.y = tr->y;
    return;
  }
  so->roul.actif = 0;
  if(so->voix.ouverte && !so->voix.fermeture && (so->voix.genre != genre || (genre == PIN_MOLLE && so->voix.forme != tr->forme))) fermerVoix(s);
  if(!so->voix.ouverte || so->voix.fermeture){ if(so->voix.ouverte || (genre == PIN_MOLLE && !tr->forme)) return; ouvrirVoix(s, tr, genre); }
  so->voix.vu = s->dernier;
  double t = tA(s), vit = hypot(tr->vx, tr->vy), e = mn2(1, vit/22);
  viser(&so->voix.pan, panX(s, tr->x), .05);
  if(vit > 1.5){
    double a = atan2(tr->vy, tr->vx);
    if(so->voix.angle < 1e8){
      double d = a - so->voix.angle; while(d > PI) d -= PI*2; while(d < -PI) d += PI*2;
      if(fabs(d) > 1.25 && vit > 5 && t - so->voix.tAccent > .12){ so->voix.tAccent = t;
        coup(s, (Opt){ .f = hz(s, degreY(s, tr->y) + E_(s)->n*2), .vol = mn2(.07, .02 + vit*.002), .dur = .08, .att = .004, .pan = panX(s, tr->x), .bus = BUS_ECHO }); }
      else so->voix.tours += d;
    }
    so->voix.angle = a;
  }
  if(genre == PIN_MOLLE){
    const Recette *R = recette(so->voix.motif);
    int deg = degreY(s, tr->y) + (int)borne(lround(so->voix.tours/2.2), -6, 6) + decalage(tr->forme);
    double f = hz(s, deg)*recOct(R), ux = vit > .5 ? tr->vx/vit : 0, uy = vit > .5 ? tr->vy/vit : 0;
    double coupure = (320 + 900*e + 4300*e*e)*E_(s)->clair*so->voix.clair;
    if(deg != so->voix.deg){ so->voix.deg = deg; so->voix.ff.v = coupure*2 + 600; }   /* chaque degré franchi s'articule */
    viser(&so->voix.ff, coupure, .05);
    viser(&so->voix.f1, f, .025);
    viser(&so->voix.f2, R->det ? f + 2 + (tr->forme ? tr->forme->id % 5 : 0) : f*(R->rap2 ? R->rap2 : 1.5), .025);
    viser(&so->voix.det1, -uy*38, .08);
    viser(&so->voix.g2, (R->niv2 ? R->niv2 : .25)*(R->det ? 1 : .3 + .7*mx2(0, ux)), .08);
    if(R->fmR){ viser(&so->voix.fm, f*R->fmR, .03); viser(&so->voix.gfm, f*R->fmI*(.25 + .75*e), .05); }
    if(R->vib) viser(&so->voix.gvib, f*(pow(2, R->vibD/1200) - 1)*(.4 + .6*e), .05);
    viser(&so->voix.vca, R->vol*1.3*(.32 + .68*e)*borne(poids ? poids : 1, .5, 1.8), R->att ? .16 : .04);
  }else{
    viser(&so->voix.ff, 300 + (1 - tr->y/s->H)*900 + vit*25, .05);
    viser(&so->voix.vca, .016 + .032*e, .08);   /* le liquide murmure */
  }
}
static void son_verse(Syn *s, Traceur *tr, int n){
  Son *so = s->son; if(!so || !so->actif) return;
  son_suivre(s, tr, PIN_LIQUIDE, 1);
  double t = tA(s); if(t - so->tBulle < .035) return; so->tBulle = t;
  bulle(s, hz(s, degreY(s, tr->y) + E_(s)->n + (int)(alea01(s)*3) + TEINTE_DEG[tr->ci % 7])*2, mn2(.12, .04 + n*.012), panX(s, tr->x), 0, tr->vy < -2 ? 1 : tr->vy > 2 ? -1 : 1);
}
static void son_disparition(Syn *s, Forme *f){
  Son *so = s->son; if(!so || !so->actif || tA(s) - so->tDisp < .09) return; so->tDisp = tA(s);
  noteMotif(s, f->motif, hz(s, degreY(s, f->cy) + decalage(f))*.5, .07, panX(s, f->cx), 0, f, .6);
}

/* ================= partition : la toile jouée en boucle ================= */
static int parCpt(const void *a, const void *b){ const Forme *p = *(Forme * const *)a, *q = *(Forme * const *)b; return q->cpt - p->cpt; }
static void jungle(Syn *s, int st, int pos, double t, Forme **r, int nr){
  Forme *bas = NULL, *gauche = NULL;
  for(int k = 0; k < s->nf; k++){ Forme *f = s->formes[k]; if(f->liquide || !f->cpt) continue; if(!bas || f->cy > bas->cy) bas = f; if(!gauche || f->cx < gauche->cx) gauche = f; }
  if(!bas) return;
  int mesure = (st >> 4) & 1;
  { char c = figure(ST_JUNGLE, G_BASSE, mesure)[pos];
    if(c != '-'){ Forme *f = c == 'x' ? bas : gauche; frappe(s, G_BASSE, s->W/2, f->cy, c == 'x' ? 1 : .8, t, 0, decalage(f)); } }
  if(!nr) return;
  double my = 0, poids = 0;
  for(int k = 0; k < nr; k++){ my += r[k]->cy*r[k]->cpt; poids += r[k]->cpt; }
  my /= poids;
  double niv = mn2(1, .55 + .08*nr + sqrt(poids)*.008);
  static const int INST[3] = { G_BOUM, G_CLAIRE, G_TIC };
  for(int k = 0; k < 3; k++){ char c = figure(ST_JUNGLE, INST[k], mesure)[pos]; if(c == '-') continue;
    frappe(s, INST[k], s->W/2, my, niv*(c == 'x' ? 1.1 : c == 'o' ? .85 : .4), t, c == 'x' ? 0 : 1, 0); }
  for(int k = 0; k < nr && k < 7; k++){
    Forme *f = r[k]; int g = recette(f->motif)->rythme; double nivf = mn2(1, .4 + sqrt(f->cpt)*.04); int dcf = decalage(f);
    if(st == (int)borne(floor(f->cx/s->W*32), 0, 31)) frappe(s, G_TRANCHE, f->cx, f->cy, nivf, t, 0, dcf);
    if(g == G_BOUM || g == G_CLAIRE || g == G_TIC || g == G_BASSE) continue;
    const char *fg = figure(ST_JUNGLE, g, f->cx >= s->W*.5);
    if(fg){ char c = fg[pos]; if(c != '-') frappe(s, g, f->cx, f->cy, nivf*(c == 'x' ? 1.1 : c == 'o' ? .85 : .4), t, c == 'x' ? 0 : 1, dcf); }
    else if(euclide((int)borne(f->rk ? f->rk : 3, 1, 12), 16, pos)) frappe(s, g, f->cx, f->cy, nivf*.8, t, 1, dcf);
  }
}
static void jouerPas(Syn *s, int st, double t){
  Son *so = s->son;
  double x0 = st/32.0*s->W, x1 = (st + 1)/32.0*s->W; int pos = st % 16, style = so->style;
  Forme *a = NULL, *b = NULL, *c = NULL; Forme *r[256]; int nr = 0;
  for(int k = 0; k < s->nf; k++){
    Forme *f = s->formes[k];
    if(f->liquide || !f->cpt) continue;
    if(recette(f->motif)->rythme != G_AUCUN){ if(nr < 256) r[nr++] = f; continue; }
    if(f->cx < x0 || f->cx >= x1) continue;
    if(!a || f->cpt > a->cpt){ c = b; b = a; a = f; } else if(!b || f->cpt > b->cpt){ c = b; b = f; } else if(!c || f->cpt > c->cpt) c = f;
  }
  Forme *mel[3] = { a, b, c };
  for(int k = 0; k < 3; k++) if(mel[k]){ Forme *f = mel[k];
    noteMotif(s, f->motif, hz(s, degreY(s, f->cy) + decalage(f)), mn2(.16, .04 + sqrt(f->cpt)*.008), panX(s, f->cx), t, f, 1 + mn2(1.5, f->cpt/200.0)); }
  qsort(r, nr, sizeof(Forme*), parCpt);
  if(style == ST_JUNGLE){ jungle(s, st, pos, t, r, nr); return; }
  int parGenre[NGENRES] = {0}, pistes = 0;
  for(int k = 0; k < nr; k++){
    Forme *f = r[k]; int g = recette(f->motif)->rythme;
    if(pistes >= 7 || parGenre[g] >= 2) continue;
    parGenre[g]++; pistes++;
    double niv = mn2(1, .4 + sqrt(f->cpt)*.04); int dcf = decalage(f);
    if(style != ST_LIBRE && (figure(style, g, 0) || g == G_BOUM)){
      int insts[2] = { g, -1 }; if(g == G_BOUM){ insts[0] = G_BOUM; insts[1] = G_CLAIRE; }
      for(int q = 0; q < 2 && insts[q] >= 0; q++){
        const char *fig = figure(style, insts[q], f->cx >= s->W*.5); if(!fig) continue;
        char ch = car(so, fig, st); if(ch == '-') continue;
        frappe(s, insts[q], f->cx, f->cy, niv*(ch == 'x' ? 1.1 : ch == 'o' ? .85 : .4), t, ch == 'x' ? 0 : 1, dcf);
      }
      continue;
    }
    int kk = (int)borne((f->rk ? f->rk : 3) + (style != ST_LIBRE ? 0 : lround(E_(s)->puls/2.0)), 1, 12);
    int q = style != ST_LIBRE ? pos : (pos + 16 - (int)borne((int)(f->cx/s->W*16), 0, 15)) % 16;
    if(euclide(kk, 16, q)) frappe(s, g, f->cx, f->cy, niv, t, q*kk/16, dcf);
    if(g == G_BOUM && (q == 4 || q == 12)) frappe(s, G_CLAIRE, f->cx, f->cy, niv, t, 0, dcf);
  }
}
static void rouler(Syn *s, int st, double t){
  Son *so = s->son; if(!so->roul.actif) return;
  if(s->dernier - so->roul.vu > 200){ so->roul.actif = 0; return; }
  if(so->roul.vit < 3.5 && (st & 1)) return;
  Forme *f = so->roul.forme; int g = recette(f->motif)->rythme; double niv = mn2(1, .6 + so->roul.vit*.03); int autre = g == G_BOUM ? G_CLAIRE : g, dc = decalage(f);
  frappe(s, g == G_BOUM && (so->roul.i & 1) ? G_CLAIRE : g, so->roul.x, so->roul.y, niv, t, so->roul.i % 4 == 0 ? 0 : 1, dc); so->roul.i++;
  if(so->roul.vit > 17) frappe(s, autre, so->roul.x, so->roul.y, niv*.7, t + so->dPas/2, 1, dc);
}
/* le groove des scènes dynamiques : grosse caisse ronde, claquement feutré, charleston léger, basse filtrée.
   Il suit le motif du style mais garde des sons doux : sinus, triangles passés au filtre, bruits adoucis. */
static void grooveDoux(Syn *s, int st, double t){
  Son *so = s->son; int pos = st % 16, mesure = (st >> 4) & 1, style = so->style; double dPas = so->dPas; int n = E_(s)->n;
  const char *k = figure(style, G_BOUM, mesure), *c = figure(style, G_CLAIRE, mesure), *b = figure(style, G_BASSE, mesure);
  char kp = k ? car(so, k, st) : 0, cp = c ? car(so, c, st) : 0, bp = b ? car(so, b, st) : 0;
  if(k && kp != '-') coup(s, (Opt){ .f = kp == 'x' ? 54 : 58, .glisse = .5, .vol = kp == 'x' ? .24 : .16, .dur = .26, .att = .006, .t = t, .pur = 1 });
  if(c && cp != '-'){ double v = cp == 'x' ? 1 : cp == 'o' ? .75 : .35;
    souffle(s, (Opt){ .f = 1500, .q = .9, .vol = .05*v, .dur = .13, .att = .006, .t = t, .bus = BUS_REV, .pur = 1 });
    coup(s, (Opt){ .f = 185, .glisse = .7, .vol = .04*v, .dur = .08, .att = .004, .t = t, .pur = 1 }); }
  int dense = style == ST_DNB || style == ST_JUNGLE;
  if(dense || !(pos & 1)) souffle(s, (Opt){ .f = 6200, .q = 1.3, .vol = (pos & 2) ? .024 : .016, .dur = .028, .att = .004, .t = t, .pan = (pos & 2) ? .4 : -.4, .pur = 1 });
  int basse = b ? bp != '-' : (pos % 4 == 2);
  if(basse){ int d = (int)lround(n*.4) + (pos >= 8 ? 2 : 0) + (bp == 'o' ? n : 0);
    coup(s, (Opt){ .f = hz(s, d)*.5, .onde = ON_TRIANGLE, .vol = .15, .dur = mn2(.5, dPas*2.4), .att = .012, .t = t, .pur = 1,
                   .aFiltre = 1, .ff = 720, .fvers = 240, .fq = .7 }); }
}
/* chaque forme qui rebondit sonne à ses chocs, dans le timbre de son motif : les balles font la rythmique */
static void rebondsSon(Syn *s){
  Son *so = s->son; double t = so ? tA(s) : 0;
  for(int k = 0; k < s->nf; k++){
    Forme *f = s->formes[k]; if(!f->rebond) continue;
    double avant = f->chocAvant; f->chocAvant = f->chocF;
    if(f->chocF < .35 || avant > .2){ f->chocF = 0; continue; }   /* seul le début d'un choc sonne, pas l'appui qui dure */
    if(so && so->actif && t - f->tChoc > .12){
      f->tChoc = t; double v = mn2(1, .3 + f->chocF*.12); int dc = decalage(f), g = recette(f->motif)->rythme;
      if(g != G_AUCUN) frappe(s, g, f->cx, f->cy, v*.8, 0, 1, dc);
      else noteMotif(s, f->motif, hz(s, degreY(s, f->cy) + dc), .05 + .07*v, panX(s, f->cx), 0, f, .3);
    }
    f->chocF = 0;
  }
}
static void planifier(Syn *s){
  Son *so = s->son;
  if(!so->actif || s->P.vitesse <= .01){ so->tProchain = 0; return; }
  double t = tA(s);
  if(so->tProchain < t) so->tProchain = t + .06;
  while(so->tProchain < t + .15){
    double tp = so->tProchain + ((so->pasSeq & 1) ? so->dPas*STYLES[so->style].swing : 0);
    double vitPas = 1;
    if(so->hardcore){ if(so->hc->jungle){ grooveJungle(s, so->pasTotal - so->hcDebut, so->tProchain); vitPas = so->hc->jg->vitPas; }   /* make me dance : la jungle */
                      else grooveHardcore(s, so->pasTotal - so->hcDebut, so->tProchain); }   /* le Hardcore : sa batterie et ses basses, sans les notes des formes */
    else {
      so->enPartition = 1; jouerPas(s, so->pasSeq, tp); so->enPartition = 0;
      rouler(s, so->pasSeq, tp);
      if(so->dynamique) grooveDoux(s, so->pasSeq, tp);
    }
    so->pasSeq = (so->pasSeq + 1) % 32; so->pasTotal++;
    if(so->pasSeq == 0 && ++so->mesures % 4 == 0){ so->derive = RACINES[(so->mesures/4) % 4]; accorder(s); }
    so->tProchain += so->dPas/vitPas;   /* la jungle accélère parfois : chaque pas a sa durée */
  }
}
/* une fois par image : libère la voix muette, règle les nappes sur la scène */
static void tic(Syn *s){
  Son *so = s->son;
  viser(&so->hcG, so->hardcore ? 1 : 0, .05);
  if(so->voix.ouverte && !so->voix.fermeture && s->dernier - so->voix.vu > 170) fermerVoix(s);
  if(!so->actif || (++so->compteTic & 1)) return;
  int trace = (so->voix.ouverte && !so->voix.fermeture) || so->roul.actif;
  for(int k = 0; k < 3; k++) viser(&so->gPart[k], trace ? (so->dynamique ? .6 : .28) : 1, trace ? .04 : .4);
  viser(&so->nappeG, mn2(.045, so->eauLisse*.00006), .12);
  viser(&so->nappeF, 450 + mn2(2600, so->eauLisse*1.2), .12);
  if(so->eauLisse > 40 && alea01(s) < mn2(.45, so->eauLisse*.0004))
    bulle(s, hz(s, E_(s)->n*2 + (int)(alea01(s)*E_(s)->n*2)), .02 + alea01(s)*.03, so->nappePan.v + (alea01(s) - .5)*.8, 0, 1);
}
static void physiqueSon(Syn *s){
  Son *so = s->son;
  so->eauLisse += (so->evEnergie - so->eauLisse)*.15;
  if(so->actif){
    double t = tA(s);
    if(so->evEauN) viser(&so->nappePan, panX(s, so->evEauX/so->evEauN)*.8, .2);
    if(so->evGel && t - so->tGel > .05){ so->tGel = t;
      souffle(s, (Opt){ .f = 900 + alea01(s)*500, .q = 1.1, .vol = mn2(.014, .005 + so->evGel*.001), .dur = .05, .att = .012, .pan = panX(s, so->evGelX/so->evGel) }); }
    if(so->evChocL && t - so->tChoc > .045){ so->tChoc = t;
      double p = panX(s, so->evChocLX/so->evChocL); int m = imn(3, 1 + so->evChocL/8);
      for(int k = 0; k < m; k++) bulle(s, borne(1000 - so->evChocLV*70, 220, 1000)*(1 + alea01(s)*.4), mn2(.09, .025 + so->evChocLV*.007), p + (alea01(s) - .5)*.3, t + k*.026, 1); }
    if(so->evChocS > 1 && t - so->tChocS > .2){ so->tChocS = t;
      double p = panX(s, so->evChocSX/so->evChocS), fo = mn2(1, so->evChocSV/7);
      coup(s, (Opt){ .f = 110, .glisse = .5, .vol = .06 + .12*fo, .dur = .22, .att = .012, .pan = p, .bus = BUS_REV, .pur = 1, .brut = 1 });   /* un choc feutré */
      souffle(s, (Opt){ .f = 180, .q = .5, .type = FT_BAS, .vol = .02 + .04*fo, .dur = .09, .att = .01, .pan = p, .pur = 1 }); }
  }
  so->evGel = so->evChocL = so->evChocS = so->evEauN = 0; so->evGelX = so->evChocLX = so->evChocLV = so->evChocSX = so->evChocSV = so->evEnergie = so->evEauX = 0;
}

/* ================= synthèse d'un bloc ================= */
static inline double tA(Syn *s){ return s->son->nEch/s->son->sr; }

static void reverbPreparer(Son *so){
  static const int PG[NPEIGNE] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 }, PT[NPASSE] = { 556, 441, 341, 225 };
  double k = so->sr/44100;
  for(int c = 0; c < 4; c++){
    for(int i = 0; i < NPEIGNE; i++){ Peigne *p = &so->pg[c][i]; p->n = (int)((PG[i] + c*23)*k); p->buf = calloc(p->n, sizeof(float)); }
    for(int i = 0; i < NPASSE; i++){ PasseTout *p = &so->pt[c][i]; p->n = (int)((PT[i] + c*23)*k); p->buf = calloc(p->n, sizeof(float)); }
  }
}
/* écrêtage doux : transparent sous 0,8, puis une courbe qui arrondit au lieu de couper net */
static inline double doux(double x){ double a = fabs(x); if(a <= .8) return x; double y = .8 + .2*tanh((a - .8)/.2); return x < 0 ? -y : y; }
static void synthetiser(Syn *s, float *out, int n){
  Son *so = s->son; double sr = so->sr, k1 = so->k1;
  /* les gains de rebouclage de la réverbération ne dépendent que de la durée : on les fige pour le bloc */
  double gPg[2][NPEIGNE]; for(int c = 0; c < 2; c++) for(int i = 0; i < NPEIGNE; i++) gPg[c][i] = pow(10, -3*so->pg[c][i].n/(sr*so->revFb));
  for(int i = 0; i < n; i++){
    long ech = so->nEch + i;
    double bl[2][3] = {{0}}, br[2][3] = {{0}};
    /* voix brèves */
    for(int k = 0; k < 128; k++){
      Coup *c = &so->coups[k]; if(!c->actif) continue;
      if(ech < c->debut) continue;
      if(ech >= c->fin){ c->actif = 0; so->nbCoups--; continue; }
      double x = jouerCoup(so, c); c->pos++;
      bl[c->part][c->bus] += x*c->gl; br[c->part][c->bus] += x*c->gr;
    }
    /* voix continue du geste */
    if(so->voix.ouverte){
      typeof(so->voix) *v = &so->voix; double vca = suivreL(&v->vca, k1), pan = suivreL(&v->pan, k1), x;
      if(v->genre == PIN_MOLLE){
        const Recette *R = recette(v->motif);
        double f1 = suivreL(&v->f1, k1)*pow(2, suivreL(&v->det1, k1)/1200), f2 = suivreL(&v->f2, k1), g2 = suivreL(&v->g2, k1);
        if(R->fmR){ f1 += sin(2*PI*v->phm)*suivreL(&v->gfm, k1); v->phm += suivreL(&v->fm, k1)/sr; v->phm -= floor(v->phm); }
        if(R->vib){ double l = sin(2*PI*v->phl)*suivreL(&v->gvib, k1); v->phl += R->vibF/sr; v->phl -= floor(v->phl); f1 += l; f2 += l; }
        x = .6*onde(v->onde, v->ph1, fabs(f1)/sr) + g2*onde(v->onde, v->ph2, f2/sr);
        v->ph1 += f1/sr; v->ph1 -= floor(v->ph1); v->ph2 += f2/sr; v->ph2 -= floor(v->ph2);
        double ff = suivreL(&v->ff, k1);
        if((v->fcompte++ & 31) == 0){ biquadRegler(&v->filtre, FT_BAS, ff, v->q, sr); if(v->souffle) biquadRegler(&v->sf, FT_BANDE, mn2(9000, f1*2), 3, sr); }
        if(v->souffle){ double b = so->bruit[v->sfLecture]; if(++v->sfLecture >= so->nBruit) v->sfLecture = 0; x += biquad(&v->sf, 0, b)*v->souffle*.5; }   /* le souffle, accordé sur la note */
        x = biquad(&v->filtre, 0, x);
        if(v->formants) for(int k = 0; k < 3; k++) x = biquad(&v->pk[k], 0, x);
      }else{
        x = so->bruit[v->lecture]; if(++v->lecture >= so->nBruit) v->lecture = 0;
        double ff = suivreL(&v->ff, k1); if((v->fcompte++ & 31) == 0) biquadRegler(&v->filtre, FT_BAS, ff*.8, .5, sr);   /* passe-bas : un ruissellement rond */
        x = biquad(&v->filtre, 0, x);
      }
      x *= vca; double p = (borne(pan, -1, 1) + 1)/2;
      int b = v->genre == PIN_MOLLE ? recette(v->motif)->bus : BUS_REV;
      bl[0][b] += x*cos(p*PI/2); br[0][b] += x*sin(p*PI/2);
      if(v->fermeture && tA(s) + i/sr >= v->tFin) v->ouverte = 0;
    }
    /* nappe du liquide */
    { double g = suivreL(&so->nappeG, k1);
      if(g > 1e-5){ double x = so->bruit[so->nappeLecture]; if(++so->nappeLecture >= so->nBruit) so->nappeLecture = 0;
        if((so->nappeCompte++ & 31) == 0) biquadRegler(&so->nappeFiltre, FT_BAS, suivreL(&so->nappeF, 32*k1), .7, sr);
        x = biquad(&so->nappeFiltre, 0, x)*g; double p = (borne(suivreL(&so->nappePan, k1), -1, 1) + 1)/2;
        bl[0][BUS_REV] += x*cos(p*PI/2); br[0][BUS_REV] += x*sin(p*PI/2); } else suivreL(&so->nappePan, k1); }
    /* partition qui recule pendant le tracé */
    double sL[3], sR[3];
    for(int k = 0; k < 3; k++){ double gp = suivreL(&so->gPart[k], k1); sL[k] = bl[0][k] + bl[1][k]*gp; sR[k] = br[0][k] + br[1][k]*gp; }
    /* mélange sec, envois vers la réverbération et l'écho */
    double mL = sL[0] + sL[1] + sL[2], mR = sR[0] + sR[1] + sR[2];
    double rvL = sL[1]*.55 + sL[2]*.3, rvR = sR[1]*.55 + sR[2]*.3;
    double rr = suivreL(&so->revRetour, k1);
    double revL = 0, revR = 0;
    for(int c = 0; c < 2; c++){
      double x = c ? rvR : rvL, y = 0;
      for(int q = 0; q < NPEIGNE; q++){ Peigne *p = &so->pg[c][q]; double o = p->buf[p->i];
        p->memo = (float)(o*(1 - so->revAmort) + p->memo*so->revAmort); p->buf[p->i] = (float)(x*.015 + p->memo*gPg[c][q]); if(++p->i >= p->n) p->i = 0; y += o; }
      for(int q = 0; q < NPASSE; q++){ PasseTout *p = &so->pt[c][q]; double b = p->buf[p->i]; p->buf[p->i] = (float)(y + b*.5); if(++p->i >= p->n) p->i = 0; y = b - y; }
      if(c) revR = y; else revL = y;
    }
    revL *= 3.2*rr; revR *= 3.2*rr;
    /* écho bouclé : ligne à retard, passe-bas, rebouclage */
    double dt = suivreL(&so->retard, k1)*sr, bo = suivreL(&so->retBoucle, k1), rt = suivreL(&so->retRetour, k1), ecL, ecR;
    { double lu = so->iLigne - dt; while(lu < 0) lu += so->nLigne; int i0 = (int)lu, i1 = (i0 + 1) % so->nLigne; double fr = lu - i0;
      double dL = so->ligne[0][i0]*(1 - fr) + so->ligne[0][i1]*fr, dR = so->ligne[1][i0]*(1 - fr) + so->ligne[1][i1]*fr;
      ecL = biquad(&so->retFiltre, 0, dL); ecR = biquad(&so->retFiltre, 1, dR);
      so->ligne[0][so->iLigne] = (float)(sL[2]*.5 + ecL*bo); so->ligne[1][so->iLigne] = (float)(sR[2]*.5 + ecR*bo);
      if(++so->iLigne >= so->nLigne) so->iLigne = 0; }
    double L = mL + revL + ecL*rt, R = mR + revR + ecR*rt;
    /* compresseur : seuil -12 dB, genou 8 dB, rapport 12, attaque 3 ms, relâche 180 ms, gain de rattrapage */
    { double niv = mx2(fabs(L), fabs(R)), dB = niv > 1e-6 ? 20*log10(niv) : -120, sortie;
      if(dB <= -12) sortie = dB; else if(dB < -4){ double u = dB + 12; sortie = -12 + u - (1 - 1.0/12)*u*u/16; } else sortie = -12 + 8 - (1 - 1.0/12)*4 + (dB + 4)/12;
      double cible = pow(10, (sortie - dB)/20); if(cible > 1) cible = 1;
      double tau = cible < so->compGain ? .003 : .18; so->compGain += (cible - so->compGain)*(1 - exp(-k1/tau));
      L *= so->compGain*1.66; R *= so->compGain*1.66; }
    L = biquad(&so->doux, 0, L); R = biquad(&so->doux, 1, R);
    { double gH = suivreL(&so->hcG, k1);   /* le Hardcore, à part : son propre écrêtage, puis le passe-haut de la montée */
      if(gH > 1e-4){ HC *h = so->hc; double B, a4[4]; so->iBloc = i; if(h->jungle) jgEchantillon(s, tA(s) + i/sr, &B, a4, 0); else hcEchantillon(s, tA(s) + i/sr, &B, a4, 0);
        if((h->hpCompte & 31) == 1) for(int c = 0; c < 2; c++){ Biquad b = h->hpM[c]; waRegler(&h->hpM[c], WA_HAUT, h->hpF, .9, 0, sr); memcpy(h->hpM[c].z1, b.z1, sizeof b.z1); memcpy(h->hpM[c].z2, b.z2, sizeof b.z2); }
        if(h->jungle){ L += (B + a4[0] + h->fxo[0])*JG_NIVEAU*gH; R += (B + a4[1] + h->fxo[1])*JG_NIVEAU*gH; }   /* la jungle : ni écrêteur ni filtre de montée en plus, elle a les siens */
        else { L += biquad(&h->hpM[0], 0, faconner(h->kClip, B + a4[0]) + h->fxo[0])*.55*gH; R += biquad(&h->hpM[1], 0, faconner(h->kClip, B + a4[1]) + h->fxo[1])*.55*gH; } } }   /* les espaces, après l'écrêteur */
    double m = suivreL(&so->maitre, k1); L *= m; R *= m;
    out[2*i] = (float)doux(L); out[2*i + 1] = (float)doux(R);
  }
  so->nEch += n;
}

/* ================= synthèse en 5.1 =================
   Les mêmes voix, placées autour de l'auditeur (FL, FR, C, SL, SR) ; quatre réverbérations décorrélées, plus fortes
   à l'arrière, pour que la pièce entière résonne ; l'écho traverse la pièce en diagonale ; la voix du geste et la nappe
   du liquide tournent lentement autour ; le caisson (LFE) reçoit le grave de tout, sous 90 Hz. Sortie : 6 canaux entrelacés. */
static void synthetiserS(Syn *s, float *out, int n){
  Son *so = s->son; double sr = so->sr, k1 = so->k1;
  double gPg[4][NPEIGNE]; for(int c = 0; c < 4; c++) for(int i = 0; i < NPEIGNE; i++) gPg[c][i] = pow(10, -3*so->pg[c][i].n/(sr*so->revFb));
  for(int i = 0; i < n; i++){
    long ech = so->nEch + i; double tc = (double)ech/sr;
    double b5[2][3][5]; memset(b5, 0, sizeof b5);
    for(int k = 0; k < 128; k++){
      Coup *c = &so->coups[k]; if(!c->actif) continue;
      if(ech < c->debut) continue;
      if(ech >= c->fin){ c->actif = 0; so->nbCoups--; continue; }
      double x = jouerCoup(so, c); c->pos++;
      double *d = b5[c->part][c->bus]; for(int q = 0; q < 5; q++) d[q] += x*c->g5[q];
    }
    if(so->voix.ouverte){
      typeof(so->voix) *v = &so->voix; double vca = suivreL(&v->vca, k1), pan = suivreL(&v->pan, k1), x;
      if(v->genre == PIN_MOLLE){
        const Recette *R = recette(v->motif);
        double f1 = suivreL(&v->f1, k1)*pow(2, suivreL(&v->det1, k1)/1200), f2 = suivreL(&v->f2, k1), g2 = suivreL(&v->g2, k1);
        if(R->fmR){ f1 += sin(2*PI*v->phm)*suivreL(&v->gfm, k1); v->phm += suivreL(&v->fm, k1)/sr; v->phm -= floor(v->phm); }
        if(R->vib){ double l = sin(2*PI*v->phl)*suivreL(&v->gvib, k1); v->phl += R->vibF/sr; v->phl -= floor(v->phl); f1 += l; f2 += l; }
        x = .6*onde(v->onde, v->ph1, fabs(f1)/sr) + g2*onde(v->onde, v->ph2, f2/sr);
        v->ph1 += f1/sr; v->ph1 -= floor(v->ph1); v->ph2 += f2/sr; v->ph2 -= floor(v->ph2);
        double ff = suivreL(&v->ff, k1);
        if((v->fcompte++ & 31) == 0){ biquadRegler(&v->filtre, FT_BAS, ff, v->q, sr); if(v->souffle) biquadRegler(&v->sf, FT_BANDE, mn2(9000, f1*2), 3, sr); }
        if(v->souffle){ double b = so->bruit[v->sfLecture]; if(++v->sfLecture >= so->nBruit) v->sfLecture = 0; x += biquad(&v->sf, 0, b)*v->souffle*.5; }
        x = biquad(&v->filtre, 0, x);
        if(v->formants) for(int k = 0; k < 3; k++) x = biquad(&v->pk[k], 0, x);
      }else{
        x = so->bruit[v->lecture]; if(++v->lecture >= so->nBruit) v->lecture = 0;
        double ff = suivreL(&v->ff, k1); if((v->fcompte++ & 31) == 0) biquadRegler(&v->filtre, FT_BAS, ff*.8, .5, sr);
        x = biquad(&v->filtre, 0, x);
      }
      x *= vca;
      int b = v->genre == PIN_MOLLE ? recette(v->motif)->bus : BUS_REV;
      if((i & 63) == 0) placer(borne(pan, -1, 1)*70 + 50*sin(2*PI*tc/23), .2, so->gVoix);   /* la voix du geste dérive lentement autour */
      for(int q = 0; q < 5; q++) b5[0][b][q] += x*so->gVoix[q];
      if(v->fermeture && tA(s) + i/sr >= v->tFin) v->ouverte = 0;
    }
    { double g = suivreL(&so->nappeG, k1);
      if(g > 1e-5){ double x = so->bruit[so->nappeLecture]; if(++so->nappeLecture >= so->nBruit) so->nappeLecture = 0;
        if((so->nappeCompte++ & 31) == 0) biquadRegler(&so->nappeFiltre, FT_BAS, suivreL(&so->nappeF, 32*k1), .7, sr);
        x = biquad(&so->nappeFiltre, 0, x)*g; double p = borne(suivreL(&so->nappePan, k1), -1, 1);
        if((i & 63) == 0) placer(p*80 + 150*sin(2*PI*tc/31), .35, so->gNappe);   /* le ruissellement fait le tour de la pièce */
        for(int q = 0; q < 5; q++) b5[0][BUS_REV][q] += x*so->gNappe[q]; } else suivreL(&so->nappePan, k1); }
    double S[3][5];
    for(int k = 0; k < 3; k++){ double gp = suivreL(&so->gPart[k], k1); for(int q = 0; q < 5; q++) S[k][q] = b5[0][k][q] + b5[1][k][q]*gp; }
    double M[5]; for(int q = 0; q < 5; q++) M[q] = S[0][q] + S[1][q] + S[2][q];
    /* envois : à gauche ce qui est à gauche, à droite ce qui est à droite (le centre des deux côtés) */
    double rvL = (S[1][0] + S[1][3] + S[1][2]*.7)*.55 + (S[2][0] + S[2][3] + S[2][2]*.7)*.3;
    double rvR = (S[1][1] + S[1][4] + S[1][2]*.7)*.55 + (S[2][1] + S[2][4] + S[2][2]*.7)*.3;
    double rr = suivreL(&so->revRetour, k1), rev[4];
    for(int c = 0; c < 4; c++){
      double x = (c & 1) ? rvR : rvL, y = 0;
      for(int q = 0; q < NPEIGNE; q++){ Peigne *p = &so->pg[c][q]; double o = p->buf[p->i];
        p->memo = (float)(o*(1 - so->revAmort) + p->memo*so->revAmort); p->buf[p->i] = (float)(x*.015 + p->memo*gPg[c][q]); if(++p->i >= p->n) p->i = 0; y += o; }
      for(int q = 0; q < NPASSE; q++){ PasseTout *p = &so->pt[c][q]; double b = p->buf[p->i]; p->buf[p->i] = (float)(y + b*.5); if(++p->i >= p->n) p->i = 0; y = b - y; }
      rev[c] = y*3.2*rr;
    }
    double dt = suivreL(&so->retard, k1)*sr, bo = suivreL(&so->retBoucle, k1), rt = suivreL(&so->retRetour, k1), ecL, ecR;
    { double eL = S[2][0] + S[2][3] + S[2][2]*.7, eR = S[2][1] + S[2][4] + S[2][2]*.7;
      double lu = so->iLigne - dt; while(lu < 0) lu += so->nLigne; int i0 = (int)lu, i1 = (i0 + 1) % so->nLigne; double fr = lu - i0;
      double dL = so->ligne[0][i0]*(1 - fr) + so->ligne[0][i1]*fr, dR = so->ligne[1][i0]*(1 - fr) + so->ligne[1][i1]*fr;
      ecL = biquad(&so->retFiltre, 0, dL); ecR = biquad(&so->retFiltre, 1, dR);
      so->ligne[0][so->iLigne] = (float)(eL*.5 + ecR*bo); so->ligne[1][so->iLigne] = (float)(eR*.5 + ecL*bo);   /* ping-pong : chaque retour change de côté */
      if(++so->iLigne >= so->nLigne) so->iLigne = 0; }
    /* FL, FR, C, SL, SR : le sec, la réverbération (plus forte derrière), l'écho en diagonale */
    double o[6];
    o[0] = M[0] + rev[0]*.75 + ecL*rt*.45;
    o[1] = M[1] + rev[1]*.75 + ecR*rt*.45;
    o[2] = M[2];
    o[4] = M[3] + rev[2]*1.1 + ecR*rt*.8;
    o[5] = M[4] + rev[3]*1.1 + ecL*rt*.8;
    /* compresseur, lié sur toutes les enceintes */
    { double niv = 0; for(int q = 0; q < 6; q++) if(q != 3 && fabs(o[q]) > niv) niv = fabs(o[q]);
      double dB = niv > 1e-6 ? 20*log10(niv) : -120, sortie;
      if(dB <= -12) sortie = dB; else if(dB < -4){ double u = dB + 12; sortie = -12 + u - (1 - 1.0/12)*u*u/16; } else sortie = -12 + 8 - (1 - 1.0/12)*4 + (dB + 4)/12;
      double cible = pow(10, (sortie - dB)/20); if(cible > 1) cible = 1;
      double tau = cible < so->compGain ? .003 : .18; so->compGain += (cible - so->compGain)*(1 - exp(-k1/tau));
      for(int q = 0; q < 6; q++) o[q] *= so->compGain*1.66; }
    o[0] = biquad(&so->douxS[0], 0, o[0]); o[1] = biquad(&so->douxS[0], 1, o[1]); o[2] = biquad(&so->douxS[1], 0, o[2]);
    o[4] = biquad(&so->douxS[2], 0, o[4]); o[5] = biquad(&so->douxS[2], 1, o[5]);
    double grave = (o[0] + o[1] + o[2])*.5;   /* le caisson : le grave de l'avant */
    { double gH = suivreL(&so->hcG, k1);
      if(gH > 1e-4 && so->hc->jungle){ HC *h = so->hc; double B, a4[4], g = JG_NIVEAU*gH; so->iBloc = i; jgEchantillon(s, tc, &B, a4, 1);
        o[0] += (B*.8 + a4[0] + h->fxo[0])*g; o[1] += (B*.8 + a4[1] + h->fxo[1])*g; o[2] += B*.45*g; o[4] += (B*.35 + h->fxo[2])*g; o[5] += (B*.35 + h->fxo[3])*g; grave += B*g*1.2; }
      else if(gH > 1e-4){ HC *h = so->hc; double B, a4[4]; so->iBloc = i; hcEchantillon(s, tc, &B, a4, 1);
        if((h->hpCompte & 31) == 1) for(int c = 0; c < 6; c++){ Biquad b = h->hpM[c]; waRegler(&h->hpM[c], WA_HAUT, h->hpF, .9, 0, sr); memcpy(h->hpM[c].z1, b.z1, sizeof b.z1); memcpy(h->hpM[c].z2, b.z2, sizeof b.z2); }
        const Courbe *K = h->kClip; double g = .55*gH;
        o[0] += biquad(&h->hpM[0], 0, faconner(K, B*.8 + a4[0]) + h->fxo[0])*g;
        o[1] += biquad(&h->hpM[1], 0, faconner(K, B*.8 + a4[1]) + h->fxo[1])*g;
        o[2] += biquad(&h->hpM[2], 0, faconner(K, B*.6))*g;
        o[4] += biquad(&h->hpM[3], 0, faconner(K, B*.5 + a4[2]) + h->fxo[2])*g;
        o[5] += biquad(&h->hpM[4], 0, faconner(K, B*.5 + a4[3]) + h->fxo[3])*g;
        grave += biquad(&h->hpM[5], 0, faconner(K, B))*g*1.2; } }
    o[3] = biquad(&so->lfeF[1], 0, biquad(&so->lfeF[0], 0, grave))*.9;
    double m = suivreL(&so->maitre, k1);
    for(int q = 0; q < 6; q++) out[6*i + q] = (float)doux(o[q]*m);
  }
  so->nEch += n;
}

/* ================= branchements ================= */
static void sonCreer(Syn *s, double sr, double volume, int nc){
  Son *so = calloc(1, sizeof(Son)); s->son = so; so->nc = nc > 2 ? 6 : 2;
  static int pretSon = 0; if(!pretSon){ pretSon = 1; preparerRecettes(); preparerStyles(); }
  so->sr = sr; so->k1 = 1/sr; so->actif = 1;
  so->capa = (int)sr; so->anneau = calloc(so->capa*so->nc, sizeof(float));
  so->nBruit = (int)(sr*2); so->bruit = malloc(sizeof(float)*so->nBruit); so->bruit8 = malloc(sizeof(float)*so->nBruit);
  { double m = 0; for(int i = 0; i < so->nBruit; i++){ m += ((alea01(s)*2 - 1) - m)*.4; so->bruit[i] = (float)(m*1.9); } }
  { float v = 0; for(int i = 0; i < so->nBruit; i++){ if(i % 9 == 0) v = alea01(s) < .5 ? -.7f : .7f; so->bruit8[i] = v; } }
  reverbPreparer(so);
  so->nLigne = (int)(sr*1.6); so->ligne[0] = calloc(so->nLigne, sizeof(float)); so->ligne[1] = calloc(so->nLigne, sizeof(float));
  biquadRegler((Biquad *)&so->retFiltre, FT_BAS, 2600, 1, sr);
  biquadRegler(&so->doux, FT_BAS, 7500, .5, sr); for(int c = 0; c < 3; c++) biquadRegler(&so->douxS[c], FT_BAS, 7500, .5, sr);
  for(int c = 0; c < 2; c++) waNeuf(&so->lfeF[c], WA_BAS, 90, -3, 0, sr);
  hcCreer(s); so->hc->kClip = courbeHC(CB_CUBIQUE, 0, 0, 0); jgCreer(s); so->hc->jungle = 1;   /* make me dance : la jungle remplace le mathcore */
  placer(0, .2, so->gVoix); placer(0, .35, so->gNappe);
  so->compGain = 1;
  so->maitre.v = 0; viser(&so->maitre, volume*1.5, .8);   /* le son entre doucement */
  for(int k = 0; k < 3; k++){ so->gPart[k].v = so->gPart[k].c = 1; }
  so->nappeF.v = so->nappeF.c = 700;
  so->tempo = 96; so->style = ST_LIBRE; so->revFb = 1; so->niveau = 1; so->orch = OR_SYNTHE;
  accorder(s); sonTempo(s); so->retard.v = so->retard.c; espace(s);
  so->revRetour.v = so->revRetour.c; so->retRetour.v = so->retRetour.c; so->retBoucle.v = so->retBoucle.c;
}
static void sonDetruire(Son *so){
  if(!so) return;
  for(int c = 0; c < 4; c++){ for(int i = 0; i < NPEIGNE; i++) free(so->pg[c][i].buf); for(int i = 0; i < NPASSE; i++) free(so->pt[c][i].buf); if(c < 2) free(so->ligne[c]); }
  if(so->hc) jgDetruire(so->hc->jg); hcDetruire(so->hc);
  free(so->anneau); free(so->bruit); free(so->bruit8); free(so);
}
/* à chaque image : programmer, puis calculer ce qu'il faut pour garder environ 0,12 s d'avance sur la lecture */
static void sonAvancer(Syn *s){
  Son *so = s->son; if(!so) return;
  tic(s);
  long lu = atomic_load(&so->lu), ecrit = atomic_load(&so->ecrit);
  long cible = (long)(so->sr*.12);
  if(ecrit - lu > so->capa) return;
  if(ecrit < lu){ atomic_store(&so->ecrit, lu); ecrit = lu; }   /* le lecteur a pris de l'avance : on repart de lui */
  float bloc[6*256]; int nc = so->nc;
  while(ecrit - lu < cible){
    planifier(s);
    int n = 256; if(nc == 6) synthetiserS(s, bloc, n); else synthetiser(s, bloc, n);
    for(int i = 0; i < n; i++){ long p = (ecrit + i) % so->capa; memcpy(so->anneau + nc*p, bloc + nc*i, sizeof(float)*nc); }
    ecrit += n; atomic_store(&so->ecrit, ecrit);
  }
}
/* lu par le fil audio */
void syn_audio(Syn *s, float *out, int n){
  static const float M2[2*6] = { 1, 0, .7f, 0, .7f, 0,   0, 1, .7f, 0, 0, .7f };   /* repli du 5.1 en stéréo, si besoin */
  syn_audio_canaux(s, out, n, 2, M2);
}
/* la matrice d'une disposition d'enceintes : pour chaque canal de sortie, son étiquette (SYN_L, SYN_R…), on calcule
   sa part de chacun des six canaux internes. Ce qui n'a pas d'enceinte est replié sur ses voisines. */
void syn_matrice(int nc, const int *etiquettes, float *m){
  int a[SYN_NETIQ] = {0}; for(int c = 0; c < nc; c++) if(etiquettes[c] >= 0 && etiquettes[c] < SYN_NETIQ) a[etiquettes[c]]++;
  int surG = a[SYN_LS] + a[SYN_LB] > 0, surD = a[SYN_RS] + a[SYN_RB] > 0, deuxG = a[SYN_LS] && a[SYN_LB], deuxD = a[SYN_RS] && a[SYN_RB];
  for(int c = 0; c < nc; c++){
    float *r = m + 6*c; memset(r, 0, sizeof(float)*6);
    switch(etiquettes[c]){
      case SYN_L: r[0] = 1; if(!a[SYN_C]) r[2] = .707f; if(!surG) r[4] = .707f; if(!a[SYN_LFE]) r[3] = 0; break;
      case SYN_R: r[1] = 1; if(!a[SYN_C]) r[2] = .707f; if(!surD) r[5] = .707f; break;
      case SYN_C: r[2] = 1; break;
      case SYN_LFE: r[3] = 1; break;
      case SYN_LS: r[4] = deuxG ? .8f : 1; break;
      case SYN_RS: r[5] = deuxD ? .8f : 1; break;
      case SYN_LB: r[4] = deuxG ? .8f : 1; break;
      case SYN_RB: r[5] = deuxD ? .8f : 1; break;
      case SYN_CB: r[4] = .707f; r[5] = .707f; break;
      default: break;
    }
  }
}
/* lu par le fil audio : n images de nc canaux entrelacés, chacune faite des six canaux internes par la matrice (nc × 6) */
void syn_audio_canaux(Syn *s, float *out, int n, int nc, const float *matrice){
  Son *so = s ? s->son : NULL;
  if(!so){ memset(out, 0, sizeof(float)*nc*n); return; }
  long lu = atomic_load(&so->lu), ecrit = atomic_load(&so->ecrit); int ni = so->nc;
  for(int i = 0; i < n; i++){
    float *o = out + nc*i;
    if(lu < ecrit){ const float *a = so->anneau + ni*(lu % so->capa); lu++;
      if(ni == 2){ for(int c = 0; c < nc; c++) o[c] = c == 0 ? a[0] : c == 1 ? a[1] : 0; if(nc == 1) o[0] = .5f*(a[0] + a[1]);
                   if(nc > 2 && matrice){ for(int c = 0; c < nc; c++){ const float *m = matrice + 6*c; o[c] = a[0]*(m[0] + m[2]*.5f) + a[1]*(m[1] + m[2]*.5f); } } }
      else for(int c = 0; c < nc; c++){ const float *m = matrice + 6*c; float v = 0; for(int k = 0; k < 6; k++) v += a[k]*m[k]; o[c] = v; } }
    else for(int c = 0; c < nc; c++) o[c] = 0;
  }
  atomic_store(&so->lu, lu);
}
