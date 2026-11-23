"""Deux breaks originaux pour le Hardcore breakcore, joués par une vraie batterie acoustique (Karoryfer, CC0, via
stargate-sample-pack). Deux mesures à 140 bpm, comme un break de disque funk, mais net comme une production d'aujourd'hui :
une petite salle, une compression de bus légère, de l'air dans les aigus, 32 kHz et 16 bits. Le moteur les découpe en 32 tranches de double croche, les joue plus
vite (le break monte d'un ton, comme en jungle) et les réarrange à la volée."""
import numpy as np, soundfile as sf, scipy.signal as S, json
SG = 'sources/stargate-sample-pack/stargate-sample-pack/'
SR, BPM = 48000, 120   # joués à 120 bpm : à ce tempo, la tranche sonne à sa hauteur ; quand la jungle accélère, le break monte
PAS = 60/BPM/4; N = int(round(32*PAS*SR))
def lire(f):
    d, r = sf.read(SG + f, always_2d=True); m = d.mean(1)
    m = S.resample_poly(m, SR, r) if r != SR else m
    i = np.argmax(np.abs(m) > .03*np.abs(m).max()); m = m[max(0, i - 4):]
    return m/np.abs(m).max()
def lp(x, f, o=2): b, a = S.butter(o, f/(SR/2)); return S.lfilter(b, a, x)
def hp(x, f, o=2): b, a = S.butter(o, f/(SR/2), 'high'); return S.lfilter(b, a, x)

# Les motifs : X caisse claire, g note fantôme ; x grosse caisse ; h charleston fermé, o ouvert ; > accent
BREAKS = {
  'brk_a': {   # « la balle » : le break funk aéré, des fantômes entre les coups
    'k': ['x.x.......xx....', 'x.x.......x..x..'],
    's': ['....X..g.g..X..g', '....X..g.g.gX.g.'],
    'h': ['h.h.h.h.h.h.h.o.', 'h.h.h.h.h.h.o.h.'],
    'kit': ('karoryfer/kicks/kick_Szpaderski_24_dampened.wav', 'karoryfer/snares/snare_Pearl_alumunum_14x8.wav') },
  'brk_b': {   # « le serré » : doubles croches au charleston, grosse caisse syncopée
    'k': ['x.....x.x.......', 'x.x...x...x..x..'],
    's': ['....X...g..gX.g.', '..g.X..g....X..g'],
    'h': ['hhhhhhohhhhhhhhh', 'hhhhhhhhhhhhhhoh'],
    'kit': ('karoryfer/kicks/kick_Polmuz_mod_20_clean.wav', 'karoryfer/snares/snare_12x5.wav') },
  'brk_c': {   # « le lourd » : la caisse claire au troisième temps (un demi-temps), la ride, une crash qui ouvre
    'k': ['x......x..x.....', 'x.x....x...x..x.'],
    's': ['........X......g', '..g.....X...g...'],
    'h': ['c.r.r.r.r.r.r.r.', 'r.r.r.r.r.r.r.rr'],
    'kit': ('karoryfer/kicks/kick_Szpaderski_24_open.wav', 'karoryfer/snares/snare_16x7.wav') },
  'brk_d': {   # « le nerveux » : des fantômes partout, le charleston serré, une grosse caisse qui ne tombe jamais où on l'attend
    'k': ['x..x..x...x..x..', 'x.x..x....x.x...'],
    's': ['....X..gg..gX.gg', 'g...X.g..g..X..g'],
    'h': ['tttttttttttttttt', 'ttttttttttttttot'],
    'kit': ('karoryfer/kicks/kick_Polmuz_mod_20_buzz.wav', 'karoryfer/snares/snare_ziggurat.wav') },
  # deux breaks dans l'esprit des grands disques de la jungle, composés ici (aucun n'est relevé sur un disque) :
  'brk_e': {   # « le tambourin » : un break sec, presque sans salle, porté par un tambourin qui secoue en doubles croches
    'k': ['x......x..x.....', 'x.x....x..x.....'],
    's': ['....X.......X...', '....X..g....X.g.'],
    'h': ['................', '..............o.'],
    'j': ['jJjjjJjJjJjjjJjJ', 'jJjjjJjJjJjjjJJj'], 'salle':.03,
    'kit': ('karoryfer/kicks/kick_Szpaderski_24_dampened.wav', 'karoryfer/snares/snare_12x5.wav') },
  'brk_f': {   # « le groove » : les bongos croisent la caisse claire, la ride respire ; b petit bongo, B grand, p claqué
    'k': ['x.........x.....', 'x......x..x.....'],
    's': ['....X.......X..g', '....X..g....X...'],
    'h': ['r.r.r.r.r.r.r.r.', 'r.r.r.r.r.r.r.o.'],
    'b': ['B.b.bB.bB.b.bBpb', 'B.b.bB.b..bBp.b.'],
    'kit': ('karoryfer/kicks/kick_Polmuz_mod_20_clean.wav', 'karoryfer/snares/snare_Pearl_alumunum_14x8.wav') } }

FANTOME = lp(lire('karoryfer/snares/snare_10x6.wav'), 3500)
HH_F, HH_O = hp(lire('karoryfer/hihats/hihat_BRD_closed.wav'), 600), hp(lire('karoryfer/hihats/hihat_BRD_open.wav'), 500)
HH_T, RIDE, CRASH = hp(lire('karoryfer/hihats/hihat_BRD_tight.wav'), 1500), hp(lire('karoryfer/cymbals/ride_UD_20.wav'), 300), hp(lire('karoryfer/cymbals/crash_UD_16.wav'), 250)
rng = np.random.default_rng(7)

# le tambourin, synthétisé : quelques chocs de cymbalettes en 8 ms, chacun une grappe de résonances métalliques
# inharmoniques (5 à 12 kHz) et un souffle étroit ; fermé (80 ms) ou ouvert, accentué (220 ms)
rT = np.random.default_rng(31)
def tambourin(ouvert):
    n = int((.26 if ouvert else .12)*SR); t = np.arange(n)/SR; x = np.zeros(n); tau = .09 if ouvert else .035
    for c in range(5 if ouvert else 3):
        d = int(rT.uniform(0, .008)*SR); tt = t[:n - d]
        for f in rT.uniform(5200, 11800, 9): x[d:] += np.sin(2*np.pi*f*tt + rT.uniform(0, 6.28))*np.exp(-tt/rT.uniform(.6, 1.2)/tau)*rT.uniform(.4, 1)
        b = rT.standard_normal(n - d)*np.exp(-tt/(tau*.6)); x[d:] += .6*b
    b, a = S.butter(2, [4800/(SR/2), 13000/(SR/2)], 'band'); x = S.lfilter(b, a, x); x[:int(.0008*SR)] *= np.linspace(0, 1, int(.0008*SR))
    return x/np.abs(x).max()
TAMB, TAMB_O = tambourin(False), tambourin(True)
def bongo(f): return hp(lire('freesound/drums/bongo/' + f), 120)
BONGO_H, BONGO_B, BONGO_P = bongo('99752__menegass__bongo2.wav'), bongo('99754__menegass__bongo4.wav'), bongo('99755__menegass__bngoh.wav')

def poser(piste, x, t, v):   # en boucle : la queue d'un coup de fin de mesure revient au début, comme sur le disque
    i = int(round(t*SR)); n = min(len(x), N)
    seg = x[:n]*v; fin = i + n
    if fin <= N: piste[i:fin] += seg
    else: k = N - i; piste[i:] += seg[:k]; piste[:fin - N] += seg[k:]

def salle(sec):   # une petite pièce : premières réflexions, puis une queue de bruit qui s'éteint en 0,35 s
    n = int(.4*SR); t = np.arange(n)/SR; ir = rng.standard_normal(n)*np.exp(-t/.07); ir = lp(hp(ir, 250), 5000)
    for d, g in [(.007, .5), (.013, .35), (.021, .25)]: ir[int(d*SR)] += g*8
    ir /= np.sqrt((ir**2).sum()); w = S.fftconvolve(np.concatenate([sec, sec[:n]]), ir)[:N + n]
    w[:n] += w[N:N + n]; return w[:N]   # la queue revient au début : la boucle est sans couture

out = {}
for nom, B in BREAKS.items():
    K, SN = lire(B['kit'][0]), lire(B['kit'][1]); K = hp(K, 35)
    sec = np.zeros(N)
    for m in range(2):
        for p in range(16):
            t = (m*16 + p)*PAS + rng.uniform(0, .003)     # un batteur : à peine en retard, jamais en avance (la tranche reste propre)
            if B['k'][m][p] == 'x': poser(sec, K, t, rng.uniform(.82, .95))
            c = B['s'][m][p]
            if c == 'X': poser(sec, SN, t, rng.uniform(.88, 1))
            elif c == 'g': poser(sec, FANTOME, t + .004, rng.uniform(.13, .2))
            h = B['h'][m][p]
            if h == 'h': poser(sec, HH_F, t, (.34 if p % 4 == 0 else .26 if p % 2 == 0 else .15)*rng.uniform(.85, 1.1))
            elif h == 'o': poser(sec, HH_O*np.exp(-np.arange(len(HH_O))/SR/.22), t, .3)
            elif h == 't': poser(sec, HH_T, t, (.3 if p % 4 == 0 else .2 if p % 2 == 0 else .12)*rng.uniform(.85, 1.1))
            elif h == 'r': poser(sec, RIDE, t, (.26 if p % 4 == 0 else .18)*rng.uniform(.9, 1.05))
            elif h == 'c': poser(sec, CRASH, t, .32)
            j = B.get('j', ['.'*16]*2)[m][p]
            if j == 'j': poser(sec, TAMB, t, (.2 if p % 2 == 0 else .14)*rng.uniform(.85, 1.1))
            elif j == 'J': poser(sec, TAMB_O, t, .3*rng.uniform(.92, 1.05))
            c = B.get('b', ['.'*16]*2)[m][p]
            if c == 'b': poser(sec, BONGO_H, t + .002, .26*rng.uniform(.85, 1.05))
            elif c == 'B': poser(sec, BONGO_B, t + .002, .3*rng.uniform(.85, 1.05))
            elif c == 'p': poser(sec, BONGO_P, t + .002, .24*rng.uniform(.9, 1.05))
    mix = sec + B.get('salle', .08)*salle(sec)
    # la compression de bus : un suiveur d'enveloppe lent, -6 dB au plus fort, puis la bande qui arrondit
    env = S.lfilter([1 - np.exp(-1/(.012*SR))], [1, -np.exp(-1/(.012*SR))], np.abs(mix)); env /= env.max()
    mix = mix/(1 + .55*env); mix = mix/np.abs(mix).max()   # une compression de bus, sans saturation
    # un peu d'air (+2 dB au-dessus de 7 kHz), sans souffle ; les aigus extrêmes adoucis
    # le grain d'une console : un grave plus rond (+1,5 dB sous 160 Hz), un soupçon d'air, des aigus adoucis au-dessus de 12 kHz
    mix = hp(mix, 32); mix = mix + .19*lp(mix, 160, 2) + .08*hp(mix, 8000, 2); mix = lp(mix, 12000, 2)
    x = S.resample_poly(mix, 2, 3); x = x/np.abs(x).max()*.95                     # 32 kHz, 16 bits
    sf.write(f'banque-hc/{nom}.wav', x, 32000, subtype='PCM_16'); out[nom] = { 'k': B['k'], 's': B['s'] }
    print(nom, len(x), 'échantillons', round(len(x)/32000, 3), 's')
json.dump({ 'bpm':BPM, 'motifs':out }, open('banque-hc/breaks.json', 'w'))

# les pièces séparées, pour la batterie programmée : le tambourin (fermé, ouvert) et les bongos
for nom, x in [('tamb', TAMB), ('tamb_o', TAMB_O), ('bongo_h', BONGO_H[:int(.35*SR)]), ('bongo_b', BONGO_B[:int(.45*SR)])]:
    x = x.copy(); f = int(.04*SR); x[-f:] *= np.linspace(1, 0, f)**2
    sf.write(f'banque-hc/{nom}.wav', S.resample_poly(x/np.abs(x).max()*.95, 2, 3), 32000, subtype='PCM_16')
