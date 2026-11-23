/* Synchromy — orchestres : familles d'instruments, port de la version web.
   Le style décide quand on joue ; l'orchestre décide de quoi sont faits les sons. Chaque famille assemble ses instruments
   avec les voix brèves du synthétiseur : un partiel, un formant, une corde, un bruit filtré par voix. */

enum { PC_AUCUN = -1, PC_GRAVE, PC_CLAIRE, PC_AIGU, PC_GRAIN, PC_TOM, PC_CLIC, PC_PEAU };
/* correction de niveau propre à chaque famille : [notes, batterie] */
static const double NIVEAUX[NORCH][2] = { {1, 1}, {1.26, .75}, {1.78, 1.41}, {3.2, 1.6}, {1.41, 1.06}, {1, .5}, {1.5, .63}, {3.8, 1.12},
                                          {2, 1.68}, {.9, .63}, {1, 1}, {1.5, 1.03}, {1.33, 1.1}, {1.33, .63} };
/* timbre du trait : onde, clarté, souffle, formants */
typedef struct { int onde; double clair, souffle; int formants; } Trait;
static const Trait TRAITS[NORCH] = { {-1, 1, 0, 0}, {ON_TRIANGLE, 1, 0, 0}, {ON_SCIE, .7, 0, 0}, {ON_SCIE, 1, 0, 1}, {ON_SINUS, 1, .5, 0}, {ON_TRIANGLE, 1, 0, 0},
  {ON_TRIANGLE, 1, 0, 0}, {ON_SINUS, 1, .8, 0}, {ON_CARRE, 1, .35, 0}, {ON_SINUS, 1, 0, 0}, {ON_SCIE, .6, 0, 0}, {ON_TRIANGLE, .5, 0, 0}, {ON_TRIANGLE, 1, 0, 0}, {ON_TRIANGLE, 1, 0, 0} };

static inline int percDe(int g){
  switch(g){ case G_BOUM: return PC_GRAVE; case G_CLAIRE: case G_MAINS: case G_TRANCHE: return PC_CLAIRE; case G_TIC: return PC_AIGU; case G_SHAKER: return PC_GRAIN;
             case G_TOM: return PC_TOM; case G_TOC: return PC_CLIC; case G_CONGA: return PC_PEAU; }
  return PC_AUCUN;
}

#define CB(...) coup(s, (Opt){ .brut = 1, .pur = 1, __VA_ARGS__ })
#define SB(...) souffle(s, (Opt){ .brut = 1, .pur = 1, __VA_ARGS__ })
typedef struct { double r, a, d, det; } Mode;
static const Mode MEMBRANE[5] = { {1, 1, 1, 0}, {1.59, .45, .55, 0}, {2.14, .3, .4, 0}, {2.3, .2, .3, 0}, {2.65, .12, .25, 0} };
static const Mode BOIS_[3] = { {1, 1, 1, 0}, {2.76, .35, .35, 0}, {5.4, .14, .18, 0} };
static const Mode VERRE[4] = { {1, 1, 1, 0}, {2.32, .45, .6, 0}, {4.25, .25, .38, 0}, {6.63, .12, .25, 0} };
/* modes de vibration : chaque partiel s'éteint à son rythme */
static void modes(Syn *s, double f, const Mode *M, int n, double t, double a, double d, double niv, double pan, int bus, double g){
  for(int k = 0; k < n; k++){ double fr = f*M[k].r + M[k].det; if(fr > 14000) continue;
    CB(.f = fr, .vol = niv*M[k].a, .dur = d*M[k].d, .att = a, .t = t, .pan = pan, .bus = bus, .glisse = g); }
}
/* corde pincée de Karplus-Strong */
static void corde(Syn *s, double f, double t, double d, double clair, double niv, double pan, int bus, double g){
  CB(.ks = 1, .clair = clair, .f = f, .vol = niv, .dur = d, .att = .001, .t = t, .pan = pan, .bus = bus, .glisse = g,
     .aFiltre = 1, .ftype = FT_BAS, .ff = mn2(14000, f*(3 + 12*clair)), .fq = .5);
}
/* voix chantée : une scie passe par trois formants ; la voyelle dépend du registre */
static const double VOY[5][3][3] = { { {325, 1, 9}, {700, .35, 10}, {2530, .1, 14} }, { {450, 1, 8}, {800, .55, 10}, {2830, .12, 14} }, { {750, 1, 7}, {1150, .6, 9}, {2900, .22, 12} },
                                     { {420, 1, 8}, {1900, .4, 12}, {2600, .28, 14} }, { {290, 1, 9}, {2300, .3, 14}, {3100, .2, 16} } };
static inline int voyelle(double f){ return f < 150 ? 0 : f < 260 ? 1 : f < 470 ? 2 : f < 820 ? 3 : 4; }
static void chant(Syn *s, double f, double t, double a, double d, double niv, int v, double pan, int bus, double souffle_){
  for(int k = 0; k < 3; k++){ const double *F = VOY[v][k];
    CB(.onde = ON_SCIE, .f = f, .vol = niv*F[1]*F[2]*.22, .dur = d, .att = a, .t = t, .pan = pan, .bus = bus, .lfo = 2, .lfoF = 5.1, .lfoD = 10,
       .aFiltre = 1, .ftype = FT_BANDE, .ff = F[0], .fq = F[2]); }
  if(souffle_) SB(.f = VOY[v][1][0], .q = 2.5, .vol = niv*souffle_, .dur = d*.8, .att = a, .t = t, .pan = pan, .bus = bus);
}
static inline double plancher(double f){ while(f < 32) f *= 2; return f; }
static inline double dureeNote(const Opt *o, double k, double a, double b){ return borne((o->dur ? o->dur : .2)*k, a, b); }

/* ---------- les notes ---------- */
static void tonOrch(Syn *s, Opt o){
  Son *so = s->son; double f = plancher(o.f ? o.f : 220), t = mx2(tA(s) + .002, o.t), du = o.dur ? o.dur : .2, v = mn2(.5, o.vol ? o.vol : .1), p = o.pan; int b = o.bus;
  switch(so->orch){
    case OR_OBJETS:
      if(f < 180) modes(s, f, MEMBRANE, 5, t, .002, dureeNote(&o, 1.4, .25, 1.2), v*.9, p, b, o.glisse);
      else if(f < 900) modes(s, f, BOIS_, 3, t, .002, dureeNote(&o, 1.2, .15, .9), v*.9, p, b, o.glisse);
      else modes(s, f, VERRE, 4, t, .002, dureeNote(&o, 2.2, .4, 2.4), v*.9, p, b, o.glisse);
      SB(.f = mn2(9000, f*3), .q = 1.2, .vol = v*.5, .dur = .014, .att = .001, .t = t, .pan = p, .bus = b); break;
    case OR_CORDES:
      if(du >= .9 || o.att >= .05){ double a = mx2(.09, o.att), d = dureeNote(&o, 1.25, .5, 3);
        for(int k = 0; k < 2; k++) CB(.onde = ON_SCIE, .f = f*(k ? 1.0025 : .9975), .vol = v*.42, .dur = d, .att = a, .t = t, .pan = p, .bus = b, .glisse = o.glisse,
                                      .lfo = 2, .lfoF = 5.3, .lfoD = 8, .aFiltre = 1, .ftype = FT_BAS, .ff = mn2(9000, f*5), .fq = .7);
        SB(.f = mn2(9000, f*2.2), .q = 3, .vol = v*.22, .dur = d*.7, .att = a*.6, .t = t, .pan = p, .bus = b);
      }else corde(s, f, t, dureeNote(&o, 3, .45, 2.6), f < 200 ? .35 : .62, v*1.9, p, b, o.glisse);
      break;
    case OR_CHOEUR: { while(f > 900) f /= 2; int court = du < .25; double a = court ? .018 : mx2(.07, o.att), d = court ? .16 : dureeNote(&o, 1.3, .3, 3.2);
      chant(s, f, t, a, d, v*.8, voyelle(f), p, b, .25);
      if(!court && f < 500) CB(.onde = ON_SCIE, .f = f*1.5 + 2.2, .vol = v*.8*.45*.3, .dur = d, .att = a, .t = t, .pan = -p*.5, .bus = b, .aFiltre = 1, .ftype = FT_BANDE, .ff = VOY[voyelle(f)][0][0], .fq = 8);
      if(court) SB(.f = 2200, .q = 1.4, .vol = v*.35, .dur = .02, .att = .001, .t = t, .pan = p, .bus = b);
      break; }
    case OR_SOUFFLES: { double a = mx2(.04, o.att), d = dureeNote(&o, 1.15, .18, 2.8);
      if(f >= 220){ CB(.f = f, .vol = v*.75, .rap2 = 2, .niv2 = .13, .dur = d, .att = a, .t = t, .pan = p, .bus = b, .glisse = o.glisse, .lfo = 2, .lfoF = 5.2, .lfoD = 9);
        SB(.f = mn2(9000, f), .q = 7, .vol = v*.55, .dur = d*.9, .att = a*.5, .t = t, .pan = p, .bus = b);
        SB(.f = 7000, .type = FT_HAUT, .q = .7, .vol = v*.06, .dur = mn2(.12, d), .att = .01, .t = t, .pan = p, .bus = b); }
      else { CB(.onde = ON_CARRE, .f = f, .vol = v*.38, .dur = d, .att = a, .t = t, .pan = p, .bus = b, .glisse = o.glisse, .lfo = 2, .lfoF = 4.8, .lfoD = 7,
                .aFiltre = 1, .ftype = FT_BAS, .ff = f*3.2, .fq = 1.1);
        SB(.f = mn2(9000, f*4), .q = 4, .vol = v*.18, .dur = d*.6, .att = a, .t = t, .pan = p, .bus = b); }
      break; }
    case OR_GAMELAN: { double d = dureeNote(&o, 3, .7, 2.8); Mode M[4] = { {1, 1, 1, 0}, {1, .8, 1, 3.5 + f*.004}, {2.71, .22, .4, 0}, {5.3, .08, .2, 0} };
      modes(s, f, M, 4, t, .002, d, v*.55, p, b, o.glisse); SB(.f = mn2(9000, f*4), .q = 2, .vol = v*.35, .dur = .012, .att = .001, .t = t, .pan = p, .bus = b); break; }
    case OR_CHAMBRE:
      if(f < 140) corde(s, f, t, dureeNote(&o, 2.5, .45, 1.4), .3, v*1.9, p, b, 0);
      else if(f > 880 && du < .5) corde(s, f, t, dureeNote(&o, 4, .9, 2), .85, v*1.6, p, b, 0);
      else { double d = borne(2.6 - f/650, .7, 2.6)*(du > .3 ? 1 : .55), B = .00035;
        for(int n = 1; n <= 6; n++){ double fn = f*n*sqrt(1 + B*n*n); if(fn > 12000) break; CB(.f = fn, .vol = v*.6/pow(n, 1.25)*(n == 2 ? 1.2 : 1), .dur = d/sqrt(n), .att = .002, .t = t, .pan = p, .bus = b); }
        SB(.f = mn2(9000, f*6), .type = FT_BAS, .q = .8, .vol = v*.3, .dur = .012, .att = .001, .t = t, .pan = p, .bus = b); }
      break;
    case OR_ECOSYSTEME:
      if(du >= .6){ double a = mx2(.25, o.att), d = dureeNote(&o, 1.5, .8, 3.5);
        SB(.f = mn2(8000, f*2), .q = 18, .vol = v*2.4, .dur = d, .att = a, .t = t, .pan = p, .bus = b, .vers = mn2(8000, f*2*(o.glisse ? o.glisse : 1.06))); }
      else if(f >= 300){ CB(.f = f, .vol = v*.9, .dur = borne(9/sqrt(f), .03, .14), .att = .003, .t = t, .pan = p, .bus = b, .glisse = 1.6);
        if(alea01(s) < .4) CB(.f = f*1.5, .vol = v*.35, .dur = .05, .att = .003, .t = t + .05 + alea01(s)*.06, .pan = p, .bus = b, .glisse = 1.5); }
      else { static const Mode T[2] = { {1, 1, 1, 0}, {2.3, .3, .4, 0} }; modes(s, f, T, 2, t, .002, dureeNote(&o, 1, .15, .5), v*.8, p, b, 0);
        SB(.f = f*4, .q = 1, .vol = v*.3, .dur = .02, .att = .001, .t = t, .pan = p, .bus = b); }
      break;
    case OR_RADIO: while(f < 400) f *= 2;
      if(du < .25){ int n = 2 + (alea01(s) < .5); for(int k = 0; k < n; k++){ int lg = alea01(s) < .35; CB(.f = f, .vol = v*.7, .dur = lg ? .06 : .025, .att = .002, .t = t + k*.075, .pan = p, .bus = b); } }
      else { double d = dureeNote(&o, 1.2, .3, 2.6);
        CB(.f = f, .vol = v*.6, .dur = d, .att = .03, .t = t, .pan = p, .bus = b, .glisse = o.glisse ? o.glisse : (alea01(s) < .5 ? 1.03 : .97), .lfo = 1, .lfoF = 6 + alea01(s)*6, .lfoD = .7);
        SB(.rauque = 1, .f = f, .q = 3, .vol = v*.3, .dur = d, .att = .02, .t = t, .pan = p, .bus = b); }
      break;
    case OR_SONAR: { while(f < 300) f *= 2; double d = dureeNote(&o, 3, .7, 3.4); b = BUS_ECHO;
      CB(.f = f, .vol = v*.75, .rap2 = 2.005, .niv2 = .16, .dur = d, .att = .002, .t = t, .pan = p, .bus = b, .glisse = o.glisse ? o.glisse : .985);
      if(du > .6) CB(.f = f, .vol = v*.35, .dur = d*.7, .att = .002, .t = t + mn2(1, so->dPas*6), .pan = p, .bus = b, .glisse = .98);
      break; }
    case OR_METAL: CB(.f = f, .aFm = 1, .fmR = f > 600 ? 3.53 : 1.41, .fmI = f > 600 ? .9 : 1.6, .rap2 = 2.76, .niv2 = .12, .vol = v*.85, .dur = dureeNote(&o, 1.8, .3, 3),
                      .att = o.att ? o.att : .003, .t = t, .pan = p, .bus = b, .glisse = o.glisse ? o.glisse : (du > .8 ? .97 : 0), .lfo = du > .8 ? 2 : 0, .lfoF = 3.2, .lfoD = 14); break;
    case OR_BANDE: { double d = dureeNote(&o, 1.25, .2, 3.2), a = mx2(.006, o.att), w = .9 + alea01(s)*.6;
      CB(.onde = ON_TRIANGLE, .f = f*.997, .vol = v*.55, .dur = d, .att = a, .t = t, .pan = p, .bus = b, .glisse = o.glisse, .lfo = 2, .lfoF = w, .lfoD = 12, .aFiltre = 1, .ftype = FT_BAS, .ff = mn2(4200, f*5), .fq = .6);
      CB(.onde = ON_TRIANGLE, .f = f*1.003, .vol = v*.4, .dur = d, .att = a, .t = t + .012, .pan = p, .bus = b, .glisse = o.glisse, .lfo = 2, .lfoF = w*1.3, .lfoD = 10, .aFiltre = 1, .ftype = FT_BAS, .ff = mn2(4200, f*5), .fq = .6);
      SB(.f = 6500, .type = FT_HAUT, .q = .7, .vol = v*.05, .dur = mn2(.6, d), .att = a, .t = t, .pan = p, .bus = b); break; }
    case OR_AUTOMATES: { while(f < 420) f *= 2; static const Mode M[3] = { {1, 1, 1, 0}, {3.9, .14, .3, 0}, {6.8, .05, .15, 0} };
      modes(s, f, M, 3, t, .001, dureeNote(&o, 3, .6, 2.2), v*.6, p, b, o.glisse); SB(.f = 6500, .type = FT_HAUT, .q = 1, .vol = v*.4, .dur = .004, .att = .001, .t = t, .pan = p, .bus = b); break; }
    case OR_PEAUX: { static const Mode M[4] = { {1, 1, 1, 0}, {1, .5, 1, 1.5}, {2, .4, .6, 0}, {3, .15, .4, 0} };
      modes(s, f, M, 4, t, .003, dureeNote(&o, 2, .4, 1.6), v*.55, p, b, o.glisse); CB(.f = f*.5, .vol = v*.3, .dur = .05, .att = .002, .t = t, .pan = p, .bus = b); break; }
  }
}

/* ---------- la batterie ---------- */
static void percOrch(Syn *s, int cat, const Piece *c){
  Son *so = s->son; double t = c->t, n = c->niv, fo = c->fort, h = c->haut, A = c->acc, P = c->pan, aig = (c->i & 1) ? .55 : -.55, dPas = so->dPas;
  switch(so->orch){
    case OR_OBJETS: switch(cat){
      case PC_GRAVE: modes(s, 52*A*(1 + .25*h), MEMBRANE, 3, t, .002, .45, .38*n, P*.3, BUS_SEC, .82); SB(.f = 220, .type = FT_BAS, .q = .6, .vol = .14*n, .dur = .06, .att = .002, .t = t, .pan = P*.3); break;
      case PC_CLAIRE: modes(s, 430*A, BOIS_, 3, t, .001, .1, .2*n, P, BUS_SEC, 0); SB(.f = 2600, .q = .8, .vol = .12*n, .dur = .05, .att = .001, .t = t, .pan = P); break;
      case PC_AIGU: modes(s, 3100 + 1300*h, VERRE, 2, t, .001, .16, .05*n*fo, aig, BUS_SEC, 0); break;
      case PC_GRAIN: for(int k = 0; k < 3; k++) modes(s, 1700 + alea01(s)*1400, BOIS_, 2, t + k*.009 + alea01(s)*.006, .001, .03, .035*n, P, BUS_SEC, 0); break;
      case PC_TOM: modes(s, (105 + 130*h)*A, MEMBRANE, 5, t, .002, .5, .26*n*fo, P, BUS_SEC, .94); break;
      case PC_CLIC: modes(s, 1500*A*(1 + .3*h), BOIS_, 2, t, .001, .035, .16*n*fo, P, BUS_SEC, 0); break;
      case PC_PEAU: modes(s, (170 + 210*h)*A, MEMBRANE, 4, t, .002, .22, .2*n*fo, P, BUS_SEC, 1.12); break; } break;
    case OR_CORDES: switch(cat){
      case PC_GRAVE: corde(s, 49*A*(1 + .2*h), t, .45, .12, .55*n, P*.3, BUS_SEC, 0); SB(.f = 160, .type = FT_BAS, .q = .7, .vol = .2*n, .dur = .07, .att = .002, .t = t, .pan = P*.3); break;
      case PC_CLAIRE: SB(.f = 1900, .q = .6, .vol = .2*n, .dur = .08, .att = .001, .t = t, .pan = P); corde(s, 165*A, t, .16, .9, .2*n, P, BUS_SEC, 0); break;
      case PC_AIGU: corde(s, (1700 + 900*h)*A, t, .25, .95, .07*n*fo, aig, BUS_SEC, 0); break;
      case PC_GRAIN: SB(.f = 3600, .q = 3, .vol = .07*n, .dur = .05, .att = .004, .t = t, .pan = P, .vers = 2100); break;
      case PC_TOM: corde(s, (82 + 80*h)*A, t, .7, .3, .5*n*fo, P, BUS_SEC, 0); break;
      case PC_CLIC: corde(s, 880*A*(1 + .4*h), t, .09, 1, .22*n*fo, P, BUS_SEC, 0); SB(.f = 5000, .type = FT_HAUT, .q = 1, .vol = .08*n, .dur = .006, .att = .001, .t = t, .pan = P); break;
      case PC_PEAU: corde(s, (130 + 130*h)*A, t, .42, .4, .42*n*fo, P, BUS_SEC, 0); break; } break;
    case OR_CHOEUR: switch(cat){
      case PC_GRAVE: CB(.f = 72*A, .vol = .4*n, .dur = .16, .att = .003, .t = t, .pan = P*.3, .glisse = .7); SB(.f = 280, .type = FT_BAS, .q = .7, .vol = .2*n, .dur = .05, .att = .002, .t = t, .pan = P*.3); break;
      case PC_CLAIRE: for(int k = 0; k < 3; k++) SB(.f = 1250, .q = 1.1, .vol = .26*n, .dur = k == 2 ? .08 : .01, .att = .001, .t = t + k*.009, .pan = P); break;
      case PC_AIGU: SB(.f = 6500, .type = FT_HAUT, .q = .9, .vol = .06*n*fo, .dur = .035, .att = .002, .t = t, .pan = aig); break;
      case PC_GRAIN: SB(.f = 2800, .q = 1, .vol = .08*n, .dur = .07, .att = .02, .t = t, .pan = P); break;
      case PC_TOM: chant(s, (105 + 70*h)*A, t, .01, .2, .32*n*fo, 0, P, BUS_SEC, 0); break;
      case PC_CLIC: SB(.f = 2700, .q = 3, .vol = .32*n*fo, .dur = .016, .att = .001, .t = t, .pan = P); CB(.f = 2100, .vol = .06*n, .dur = .01, .att = .001, .t = t, .pan = P); break;
      case PC_PEAU: chant(s, (150 + 110*h)*A, t, .008, .13, .3*n*fo, 1, P, BUS_SEC, 0); break; } break;
    case OR_SOUFFLES: switch(cat){
      case PC_GRAVE: CB(.f = 88*A, .vol = .32*n, .dur = .2, .att = .02, .t = t, .pan = P*.3); SB(.f = 90*A, .q = 5, .vol = .5*n, .dur = .2, .att = .015, .t = t, .pan = P*.3); break;
      case PC_CLAIRE: SB(.f = 1450, .q = .7, .vol = .2*n, .dur = .12, .att = .004, .t = t, .pan = P); break;
      case PC_AIGU: SB(.f = 5200, .type = FT_HAUT, .q = 1, .vol = .07*n*fo, .dur = .012, .att = .001, .t = t, .pan = aig); CB(.f = 3000, .vol = .03*n, .dur = .008, .att = .001, .t = t, .pan = aig); break;
      case PC_GRAIN: SB(.f = 4200, .q = 1.5, .vol = .09*n, .dur = .05, .att = .02, .t = t, .pan = P); break;
      case PC_TOM: { double f = (150 + 130*h)*A; CB(.f = f, .vol = .22*n*fo, .dur = .22, .att = .025, .t = t, .pan = P); SB(.f = f, .q = 6, .vol = .4*n, .dur = .2, .att = .02, .t = t, .pan = P); break; }
      case PC_CLIC: CB(.f = 620*A, .vol = .15*n*fo, .dur = .02, .att = .001, .t = t, .pan = P); SB(.f = 3000, .q = 1, .vol = .1*n, .dur = .01, .att = .001, .t = t, .pan = P); break;
      case PC_PEAU: CB(.f = (260 + 260*h)*A, .vol = .17*n*fo, .dur = .14, .att = .01, .t = t, .pan = P, .lfo = 1, .lfoF = 24, .lfoD = .9); break; } break;
    case OR_GAMELAN: switch(cat){
      case PC_GRAVE: { Mode M[4] = { {1, 1, 1, 0}, {1, .8, 1, 1.3}, {2.02, .3, .55, 0}, {2.98, .15, .35, 0} }; modes(s, 58*A*(1 + .2*h), M, 4, t, .015, 2.6, .3*n, P*.3, BUS_SEC, 0); break; }
      case PC_CLAIRE: for(int k = 0; k < 3; k++) SB(.f = 3800, .type = FT_HAUT, .q = .8, .vol = .12*n*(1 - k*.25), .dur = .025, .att = .001, .t = t + k*.014, .pan = P);
                      SB(.f = 5200, .q = 9, .vol = .1*n, .dur = .25, .att = .001, .t = t, .pan = P); break;
      case PC_AIGU: { static const Mode M[2] = { {1, 1, 1, 0}, {2.3, .5, .6, 0} }; SB(.f = 7200, .q = 2, .vol = .07*n*fo, .dur = .05, .att = .001, .t = t, .pan = aig); modes(s, 4100, M, 2, t, .001, .12, .025*n, aig, BUS_SEC, 0); break; }
      case PC_GRAIN: SB(.f = 4200, .type = FT_HAUT, .q = .9, .vol = .08*n, .dur = .03, .att = .001, .t = t, .pan = P); break;
      case PC_TOM: CB(.f = (130 + 110*h)*A, .vol = .3*n*fo, .dur = .22, .att = .003, .t = t, .pan = P, .glisse = 1.3); SB(.f = 900, .q = .8, .vol = .08*n, .dur = .03, .att = .001, .t = t, .pan = P); break;
      case PC_CLIC: { static const Mode M[2] = { {1, 1, 1, 0}, {2.71, .3, .5, 0} }; modes(s, 1250*A*(1 + .3*h), M, 2, t, .001, .12, .1*n*fo, P, BUS_SEC, 0); break; }
      case PC_PEAU: CB(.f = (290 + 150*h)*A, .vol = .22*n*fo, .dur = .08, .att = .002, .t = t, .pan = P, .glisse = .88); break; } break;
    case OR_CHAMBRE: switch(cat){
      case PC_GRAVE: { static const Mode M[4] = { {1, 1, 1, 0}, {1.5, .5, .8, 0}, {1.98, .35, .6, 0}, {2.44, .2, .45, 0} }; modes(s, (68 + 30*h)*A, M, 4, t, .004, 1.1, .3*n, P*.3, BUS_SEC, 0);
        SB(.f = 200, .type = FT_BAS, .q = .7, .vol = .15*n, .dur = .05, .att = .002, .t = t, .pan = P*.3); break; }
      case PC_CLAIRE: SB(.f = 2600, .q = .5, .vol = .16*n, .dur = .14, .att = .012, .t = t, .pan = P); break;
      case PC_AIGU: { static const Mode M[3] = { {1, 1, 1, 0}, {2.75, .4, .7, 0}, {5.1, .2, .5, 0} }; modes(s, 4200, M, 3, t, .001, c->i == 0 ? 1.2 : .4, .035*n*fo, aig, BUS_SEC, 0); break; }
      case PC_GRAIN: SB(.f = 5200, .q = .9, .vol = .08*n, .dur = .07, .att = .01, .t = t, .pan = P, .vers = 3000); break;
      case PC_TOM: { static const Mode M[3] = { {1, 1, 1, 0}, {1.5, .45, .8, 0}, {1.98, .3, .6, 0} }; modes(s, (100 + 70*h)*A, M, 3, t, .004, .8, .26*n*fo, P, BUS_SEC, 0); break; }
      case PC_CLIC: { static const Mode M[2] = { {1, 1, 1, 0}, {2.6, .3, .5, 0} }; modes(s, 1100*A*(1 + .3*h), M, 2, t, .001, .06, .17*n*fo, P, BUS_SEC, 0); break; }
      case PC_PEAU: modes(s, (190 + 120*h)*A, MEMBRANE, 3, t, .002, .18, .2*n*fo, P, BUS_SEC, .95); break; } break;
    case OR_ECOSYSTEME: switch(cat){
      case PC_GRAVE: SB(.f = 110, .type = FT_BAS, .q = .6, .vol = .55*n, .dur = .7, .att = .03, .t = t, .pan = P*.4); CB(.f = 44, .vol = .16*n, .dur = .5, .att = .02, .t = t, .pan = P*.4, .glisse = .8); break;
      case PC_CLAIRE: SB(.f = 3100, .q = .8, .vol = .3*n, .dur = .02, .att = .001, .t = t, .pan = P); SB(.f = 2200, .q = 1.2, .vol = .16*n, .dur = .015, .att = .001, .t = t + .016, .pan = P); break;
      case PC_AIGU: CB(.f = 4300 + 500*h, .vol = .045*n*fo, .dur = .07, .att = .003, .t = t, .pan = aig*1.3, .lfo = 1, .lfoF = 42, .lfoD = 1); break;
      case PC_GRAIN: for(int k = 0; k < 4; k++) CB(.f = 1400 + alea01(s)*2400, .vol = .04*n, .dur = .03, .att = .002, .t = t + alea01(s)*dPas*.8, .pan = P, .glisse = 1.5); break;
      case PC_TOM: { static const Mode M[3] = { {1, 1, 1, 0}, {2.3, .35, .4, 0}, {3.9, .1, .2, 0} }; modes(s, (130 + 90*h)*A, M, 3, t, .002, .3, .3*n*fo, P, BUS_SEC, 0); break; }
      case PC_CLIC: { static const Mode M[2] = { {1, 1, 1, 0}, {2.1, .4, .5, 0} }; modes(s, 2400*A*(1 + .3*h), M, 2, t, .001, .025, .14*n*fo, P, BUS_SEC, 0); break; }
      case PC_PEAU: CB(.onde = ON_SCIE, .f = (100 + 50*h)*A, .vol = .5*n*fo, .dur = .13, .att = .01, .t = t, .pan = P, .lfo = 1, .lfoF = 28, .lfoD = 1, .aFiltre = 1, .ftype = FT_BANDE, .ff = 650, .fq = 4); break; } break;
    case OR_RADIO: switch(cat){
      case PC_GRAVE: SB(.rauque = 1, .f = 280, .type = FT_BAS, .q = .7, .vol = .5*n, .dur = .08, .att = .002, .t = t, .pan = P*.3); CB(.f = 62*A, .vol = .3*n, .dur = .14, .att = .002, .t = t, .pan = P*.3, .glisse = .6); break;
      case PC_CLAIRE: SB(.rauque = 1, .f = 2100, .type = FT_HAUT, .q = .6, .vol = .16*n, .dur = .05, .att = .001, .t = t, .pan = P); SB(.rauque = 1, .f = 3200, .type = FT_HAUT, .q = .6, .vol = .1*n, .dur = .012, .att = .001, .t = t + .02, .pan = P); break;
      case PC_AIGU: CB(.f = 7000, .vol = .04*n*fo, .dur = .015, .att = .001, .t = t, .pan = aig, .glisse = .3); break;
      case PC_GRAIN: SB(.rauque = 1, .f = 3200, .q = .5, .vol = .09*n, .dur = .06, .att = .01, .t = t, .pan = P); break;
      case PC_TOM: CB(.f = (1200 + 800*h)*A, .vol = .1*n*fo, .dur = .12, .att = .002, .t = t, .pan = P, .glisse = .15); break;
      case PC_CLIC: SB(.rauque = 1, .f = 4200, .type = FT_HAUT, .q = 1, .vol = .2*n, .dur = .004, .att = .001, .t = t, .pan = P); CB(.onde = ON_CARRE, .f = 900*A, .vol = .05*n, .dur = .006, .att = .001, .t = t, .pan = P); break;
      case PC_PEAU: CB(.f = (300 + 200*h)*A, .vol = .2*n*fo, .dur = .045, .att = .001, .t = t, .pan = P, .glisse = .5); break; } break;
    case OR_SONAR: switch(cat){
      case PC_GRAVE: CB(.f = 50*A, .vol = .4*n, .dur = .8, .att = .004, .t = t, .pan = P*.3, .glisse = .6); SB(.f = 170, .type = FT_BAS, .q = .6, .vol = .3*n, .dur = .45, .att = .01, .t = t, .pan = P*.3); break;
      case PC_CLAIRE: { static const Mode M[3] = { {1, 1, 1, 0}, {1.47, .5, .7, 0}, {2.09, .3, .5, 0} }; modes(s, 240*A, M, 3, t, .002, .35, .16*n, P, BUS_SEC, 0);
        SB(.f = 900, .q = .7, .vol = .12*n, .dur = .04, .att = .001, .t = t, .pan = P); break; }
      case PC_AIGU: CB(.f = 2800 + 600*h, .vol = .07*n*fo, .dur = .014, .att = .001, .t = t, .pan = aig, .bus = BUS_ECHO); break;
      case PC_GRAIN: for(int k = 0; k < 3; k++){ double f = 500 + alea01(s)*900; CB(.f = f, .vol = .06*n, .dur = borne(6/(.13*f), .03, .1), .att = .004, .t = t + alea01(s)*dPas*.7, .pan = P, .glisse = 1.4); } break;
      case PC_TOM: CB(.f = (68 + 40*h)*A, .vol = .38*n*fo, .dur = .3, .att = .003, .t = t, .pan = P, .glisse = .7); break;
      case PC_CLIC: CB(.f = 1500*A, .vol = .16*n*fo, .dur = .007, .att = .001, .t = t, .pan = P, .bus = BUS_ECHO); break;
      case PC_PEAU: CB(.f = (115 + 60*h)*A, .vol = .3*n*fo, .dur = .12, .att = .003, .t = t, .pan = P, .glisse = .85); SB(.f = 300, .type = FT_BAS, .q = .6, .vol = .12*n, .dur = .04, .att = .002, .t = t, .pan = P); break; } break;
    case OR_METAL: switch(cat){
      case PC_GRAVE: CB(.f = 88*A, .aFm = 1, .fmR = 1.41, .fmI = 1.3, .glisse = .8, .vol = .36*n, .dur = .32, .att = .002, .t = t, .pan = P*.3); break;
      case PC_CLAIRE: SB(.f = 2500, .type = FT_HAUT, .q = .7, .vol = .12*n, .dur = .12, .att = .001, .t = t, .pan = P); CB(.f = 520*A, .aFm = 1, .fmR = 2.3, .fmI = 2.2, .vol = .08*n, .dur = .18, .att = .001, .t = t, .pan = P, .bus = BUS_REV); break;
      case PC_AIGU: CB(.f = 5000 + 900*h, .aFm = 1, .fmR = 1.41, .fmI = .5, .vol = .035*n*fo, .dur = .025, .att = .001, .t = t, .pan = aig); break;
      case PC_GRAIN: SB(.f = 8000, .q = 4, .vol = .1*n, .dur = .06, .att = .005, .t = t, .pan = P); break;
      case PC_TOM: { static const Mode M[3] = { {1, 1, 1, 0}, {2, .4, .7, 0}, {2.92, .2, .5, 0} }; modes(s, (196 + 150*h)*A, M, 3, t, .002, .55, .2*n*fo, P, BUS_SEC, 0); break; }
      case PC_CLIC: CB(.f = 2600*A*(1 + .3*h), .rap2 = 2.76, .niv2 = .3, .vol = .1*n*fo, .dur = .08, .att = .001, .t = t, .pan = P, .bus = BUS_ECHO); break;
      case PC_PEAU: { static const Mode M[3] = { {1, 1, 1, 0}, {2, .45, .7, 0}, {3, .2, .5, 0} }; modes(s, (260 + 200*h)*A, M, 3, t, .002, .4, .16*n*fo, P, BUS_SEC, 0); break; } } break;
    case OR_BANDE: switch(cat){
      case PC_GRAVE: CB(.f = 105*A, .glisse = .45, .vol = .4*n, .dur = .2, .att = .002, .t = t, .pan = P*.3, .aFiltre = 1, .ftype = FT_BAS, .ff = 420, .fq = .7); break;
      case PC_CLAIRE: SB(.f = 1700, .q = .6, .vol = .2*n, .dur = .11, .att = .002, .t = t, .pan = P); CB(.f = 185*A, .vol = .08*n, .dur = .06, .att = .002, .t = t, .pan = P, .glisse = .8); break;
      case PC_AIGU: SB(.f = 4800, .q = 1, .vol = .07*n*fo, .dur = .03, .att = .002, .t = t, .pan = aig*.7); break;
      case PC_GRAIN: for(int k = 0; k < 3; k++) SB(.rauque = 1, .f = 3200, .type = FT_HAUT, .q = .7, .vol = .12*n, .dur = .003, .att = .001, .t = t + alea01(s)*dPas, .pan = P); break;
      case PC_TOM: CB(.f = (130 + 90*h)*A, .glisse = .6, .vol = .28*n*fo, .dur = .24, .att = .003, .t = t, .pan = P, .aFiltre = 1, .ftype = FT_BAS, .ff = 700, .fq = .7); break;
      case PC_CLIC: CB(.f = 880*A, .vol = .12*n*fo, .dur = .02, .att = .001, .t = t, .pan = P); SB(.f = 2500, .q = 1, .vol = .08*n, .dur = .012, .att = .001, .t = t, .pan = P); break;
      case PC_PEAU: CB(.f = (250 + 160*h)*A, .glisse = .8, .vol = .18*n*fo, .dur = .1, .att = .002, .t = t, .pan = P, .aFiltre = 1, .ftype = FT_BAS, .ff = 1500, .fq = .7); break; } break;
    case OR_AUTOMATES: switch(cat){
      case PC_GRAVE: { static const Mode M[2] = { {1, 1, 1, 0}, {2.4, .4, .5, 0} }; modes(s, 92*A, M, 2, t, .002, .13, .38*n, P*.3, BUS_SEC, 0); SB(.f = 400, .type = FT_BAS, .q = .8, .vol = .2*n, .dur = .03, .att = .001, .t = t, .pan = P*.3); break; }
      case PC_CLAIRE: for(int k = 0; k < 4; k++) SB(.f = 3000, .q = 4, .vol = .4*n*(1 - k*.2), .dur = .006, .att = .001, .t = t + k*.012, .pan = P); break;
      case PC_AIGU: CB(.f = (c->i & 1) ? 2900 : 3500, .vol = .08*n*fo, .dur = .01, .att = .001, .t = t, .pan = aig); break;
      case PC_GRAIN: for(int k = 0; k < 6; k++) SB(.f = 4200, .q = 5, .vol = .2*n, .dur = .004, .att = .001, .t = t + k*dPas/6, .pan = P); break;
      case PC_TOM: modes(s, (180 + 120*h)*A, BOIS_, 3, t, .002, .2, .24*n*fo, P, BUS_SEC, 0); SB(.f = 1200, .q = .8, .vol = .1*n, .dur = .02, .att = .001, .t = t, .pan = P); break;
      case PC_CLIC: CB(.f = 4000*A, .vol = .14*n*fo, .dur = .006, .att = .001, .t = t, .pan = P); break;
      case PC_PEAU: CB(.f = (220 + 120*h)*A, .vol = .18*n*fo, .dur = .3, .att = .002, .t = t, .pan = P, .glisse = .7, .lfo = 2, .lfoF = 17, .lfoD = 100); break; } break;
    case OR_PEAUX: switch(cat){
      case PC_GRAVE: CB(.f = 58*A*(1 + .2*h), .vol = .42*n, .dur = .6, .att = .004, .t = t, .pan = P*.3, .glisse = .75); CB(.f = 92*A, .vol = .12*n, .dur = .25, .att = .004, .t = t, .pan = P*.3);
                     SB(.f = 260, .type = FT_BAS, .q = .7, .vol = .3*n, .dur = .14, .att = .004, .t = t, .pan = P*.3); break;
      case PC_CLAIRE: SB(.f = 1150, .q = 1.2, .vol = .3*n, .dur = .05, .att = .001, .t = t, .pan = P); CB(.f = 340*A, .vol = .16*n, .dur = .06, .att = .001, .t = t, .pan = P); break;
      case PC_AIGU: { static const Mode M[4] = { {1, 1, 1, 0}, {2, .6, .75, 0}, {3, .35, .5, 0}, {4, .2, .35, 0} }; modes(s, (620 + 300*h)*A, M, 4, t, .001, .35, .06*n*fo, aig, BUS_SEC, 0); break; }
      case PC_GRAIN: for(int k = 0; k < 2; k++) SB(.f = 6000, .q = 1.2, .vol = .09*n*(k ? .6 : 1), .dur = .045, .att = .01, .t = t + k*dPas*.5, .pan = P); break;
      case PC_TOM: CB(.f = (88 + 60*h)*A, .vol = .36*n*fo, .dur = .3, .att = .002, .t = t, .pan = P, .glisse = .8); SB(.f = 700, .q = .8, .vol = .14*n, .dur = .03, .att = .001, .t = t, .pan = P); break;
      case PC_CLIC: { static const Mode M[2] = { {1, 1, 1, 0}, {2.4, .3, .5, 0} }; modes(s, 2500*A*(1 + .2*h), M, 2, t, .001, .05, .18*n*fo, P, BUS_SEC, 0); break; }
      case PC_PEAU: CB(.f = (100 + 50*h)*A, .vol = .32*n*fo, .dur = .42, .att = .003, .t = t, .pan = P, .glisse = 1.4); break; } break;
  }
}
#undef CB
#undef SB

/* l'orchestre joue, avec sa correction de niveau */
static void jouerTonOrch(Syn *s, Opt o){ Son *so = s->son; so->niveau = NIVEAUX[so->orch][0]; tonOrch(s, o); so->niveau = 1; }
static void jouerPercOrch(Syn *s, int cat, const Piece *c){ Son *so = s->son; so->niveau = NIVEAUX[so->orch][1]; percOrch(s, cat, c); so->niveau = 1; }
