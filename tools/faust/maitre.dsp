declare name "Synchromy - maître";
declare author "Synchromy (Etienne Mineur, etienne.design)";
declare license "MIT";
declare description "La chaîne de sortie : réverbération d'envoi (zita-rev1), compression multibande haut et bas (façon OTT), écrêteur doux sans repliement (ADAA), limiteur à anticipation.";

import("stdfaust.lib");

// ---------- réglages ----------
ott     = hslider("ott", 0, 0, 1, 0.01) : si.smoo;                    // profondeur de la compression multibande
dur     = hslider("dur", 0, 0, 1, 0.01) : si.smoo;                    // 0 : écrêteur contourné ; 1 : écrêteur en service
pousse  = hslider("pousse", 0, -6, 18, 0.1) : ba.db2linear : si.smoo; // gain avant l'écrêteur (dB)
plafond = hslider("plafond", -1, -12, 0, 0.1) : ba.db2linear : si.smoo; // plafond du limiteur (dBFS)
rev     = hslider("rev", 1, 0, 2, 0.01) : si.smoo;                    // niveau du retour de réverbération

// ---------- réverbération d'envoi : zita-rev1 (Fons Adriaensen), 25 ms de prédélai, 2,8 s au médium ----------
espace = re.zita_rev1_stereo(25, 180, 5000, 3.2, 2.6, 96000) : *(rev), *(rev);

// ---------- compression multibande, vers le haut et vers le bas (à la manière d'OTT) ----------
// trois bandes (Linkwitz-Riley 4e ordre à 120 Hz et 2,5 kHz ; le médium est le reste : la somme est exacte)
lo(x) = x : fi.lowpass(2, 120) : fi.lowpass(2, 120);
hi(x) = x : fi.highpass(2, 2500) : fi.highpass(2, 2500);
bande(a, b, att, rel, tbas, thaut, montee) = a*g, b*g with {
  e  = max(abs(a), abs(b)) : an.amp_follower_ar(att, rel) : max(1e-9);
  lv = ba.linear2db(e);
  bas  = 0 - max(lv - thaut, 0)*(1 - 1/4);                         // au-dessus : 4:1
  haut = min(max(tbas - lv, 0)*(1 - 1/3), montee)*(lv > -66);      // en dessous : remonté, jusqu'à « montee » dB, jamais le silence
  g = ba.db2linear((bas + haut)*ott);
};
// le grave ne remonte jamais (le silence entre deux kicks fait la frappe) ; le médium et l'aigu, un peu
ott3(l, r) = bande(lL, lR, 0.048, 0.28, -40, -20, 0), bande(mL, mR, 0.022, 0.28, -40, -22, 6), bande(hL, hR, 0.0135, 0.13, -42, -26, 8) :> _, _ with {
  lL = lo(l); hL = hi(l); mL = l - lL - hL;
  lR = lo(r); hR = hi(r); mR = r - lR - hR;
};

// ---------- écrêteur doux sans repliement : tangente hyperbolique, antidérivée du premier ordre (ADAA) ----------
F1(x) = abs(x) + log(1 + exp(-2*abs(x))) - log(2);                 // primitive de tanh, sans débordement
adaa(x) = select2(abs(x - x1) > 1e-5, ma.tanh((x + x1)*0.5), (F1(x) - F1(x1))/(x - x1)) with { x1 = x'; };
ecrete(x) = x + dur*(adaa(x*pousse) - x);

// ---------- limiteur à anticipation (1,3 ms), stéréo lié ----------
// le gain ne dépasse jamais plafond / crête sur toute la fenêtre où passe l'échantillon retardé ; descente en moyenne
// glissante (pas de clic), remontée en 80 ms
L = 64;
maxN(1) = _;
maxN(n) = maxN(n - 1), _ : max;
tient(n) = _ <: par(i, n, @(i)) : maxN(n);
moyenne(n) = _ <: par(i, n, @(i)) :> /(n);
limiteur(l, r) = l@(L)*g, r@(L)*g with {
  pk = max(abs(l), abs(r)) : tient(2*L);
  t  = min(1, plafond/max(pk, 1e-6));
  rc = 1 - exp(-1/(0.08*ma.SR));
  g  = t : moyenne(L) : (min ~ (\(y).(y + (1 - y)*rc)));
};

process(dl, dr, sl, sr) = (dl, dr), ((sl, sr) : espace) :> ott3 : ecrete, ecrete : limiteur;
