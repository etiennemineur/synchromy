"""Banque d'échantillons du Hardcore (mathcore) : une vraie batterie acoustique (Karoryfer « Big Rusty Drums » et
« Unruly Drums », CC0, via stargate-sample-pack), des effets de guitare enregistrés (Fugue State Audio, CC0), et deux
réponses impulsionnelles de baffle 4×12 dessinées ici (phase minimale), une par côté de la guitare doublée."""
import soundfile as sf, numpy as np, json, base64, scipy.signal as S
SG = 'sources/stargate-sample-pack/stargate-sample-pack/'
def lire(f, sr):
    d, r = sf.read(SG + f, always_2d=True); m = d.mean(1)
    return S.resample_poly(m, sr, r) if r != sr else m
def debut(x, seuil=.03): i = np.argmax(np.abs(x) > seuil*np.abs(x).max()); return x[max(0, i - 6):]
def couper(x, dur, fondu, sr):
    n = min(len(x), int(dur*sr)); x = x[:n].copy(); f = min(n, int(fondu*sr)); x[-f:] *= np.linspace(1, 0, f)**2; return x
def hp(x, f, sr, o=2): b, a = S.butter(o, f/(sr/2), 'high'); return S.lfilter(b, a, x)
def norm(x, pic=.97): return x/np.abs(x).max()*pic
B = {}
R1, R2 = 32000, 44100
B['k_mc']   = (norm(couper(hp(debut(lire('karoryfer/kicks/kick_Szpaderski_24_dampened.wav', R1), .05), 30, R1), .32, .14, R1)), R1)
B['k_mc2']  = (norm(couper(hp(debut(lire('karoryfer/kicks/kick_Polmuz_mod_20_clean.wav', R1), .05), 30, R1), .32, .14, R1)), R1)
B['sn_a']   = (norm(couper(debut(lire('karoryfer/snares/snare_Pearl_alumunum_14x8.wav', R1)), .5, .25, R1)), R1)
B['sn_b']   = (norm(couper(debut(lire('karoryfer/snares/snare_12x5.wav', R1)), .5, .25, R1)), R1)
B['sn_g']   = (norm(couper(debut(lire('karoryfer/snares/snare_10x6.wav', R1)), .35, .2, R1)), R1)
B['tom_h']  = (norm(couper(debut(lire('karoryfer/toms/tom_Szpaderski_14.wav', R1)), .5, .3, R1)), R1)
B['tom_m']  = (norm(couper(debut(lire('karoryfer/toms/tom_Szpaderski_15.wav', R1)), .55, .3, R1)), R1)
B['tom_b']  = (norm(couper(debut(lire('karoryfer/toms/tom_Szpaderski_18.wav', R1)), .65, .35, R1)), R1)
# la china : une crash courte, creusée au médium, rendue plus rauque
ch = debut(lire('karoryfer/cymbals/crash_UD_16.wav', R2)); ch = hp(ch, 900, R2, 3)
b, a = S.iirpeak(5200/(R2/2), 2.5); ch = ch + .8*S.lfilter(b, a, ch); ch = np.tanh(ch/np.abs(ch).max()*2.2)
B['china'] = (norm(couper(ch, .85, .6, R2)), R2)
B['crash_hc'] = (norm(couper(hp(debut(lire('karoryfer/cymbals/crash_BRD_17.wav', R2)), 250, R2), 2.2, 1.4, R2)), R2)
B['ride']  = (norm(couper(hp(debut(lire('karoryfer/cymbals/ride_BRD_22.wav', R2)), 300, R2), .7, .45, R2)), R2)
B['hh_f']  = (norm(couper(hp(debut(lire('karoryfer/hihats/hihat_BRD_tight.wav', R2)), 2500, R2), .12, .06, R2)), R2)
B['hh_o']  = (norm(couper(hp(debut(lire('karoryfer/hihats/hihat_BRD_open.wav', R2)), 2000, R2), .45, .3, R2)), R2)
B['g_pique'] = (norm(couper(debut(lire('fugue-state-audio/guitar-fx/a-dive1.wav', R1)), 2.0, .6, R1)), R1)
B['g_harm']  = (norm(couper(debut(lire('fugue-state-audio/guitar-fx/e-harm.wav', R1)), 1.8, .7, R1)), R1)
B['g_racle'] = (norm(couper(debut(lire('fugue-state-audio/guitar-fx/stringscrape.wav', R1)), 2.2, .7, R1)), R1)
# la forge : les percussions de métal de Karoryfer, pour le collage industriel
B['enclume'] = (norm(couper(debut(lire('karoryfer/percussion/jewelers_anvil.wav', R2)), .6, .4, R2)), R2)
B['chaine']  = (norm(couper(debut(lire('karoryfer/percussion/chain_drop.wav', R2)), .7, .4, R2)), R2)
B['tiges']   = (norm(couper(debut(lire('karoryfer/percussion/big_metal_rods.wav', R2)), .5, .3, R2)), R2)
B['epee']    = (norm(couper(debut(lire('karoryfer/percussion/sword.wav', R2)), .8, .5, R2)), R2)
B['os']      = (norm(couper(debut(lire('karoryfer/percussion/bones_down.wav', R2)), .3, .2, R2)), R2)
# deux baffles 4×12 dessinés : coupure à 75 Hz, bosse de caisse close vers 110 Hz, présence vers 2,4 et 4 kHz, chute
# raide au-dessus de 5,5 kHz, et les irrégularités du cône (différentes à gauche et à droite) ; phase minimale.
def baffle(graine, sr=48000, N=16384, L=2048):
    f = np.fft.rfftfreq(N, 1/sr); f[0] = 1; rng = np.random.default_rng(graine)
    dB = np.zeros_like(f)
    dB += 20*np.log10(np.abs(1/(1 + (75/f)**4)) + 1e-9)/1            # passe-haut du 4e ordre
    dB += 3.5*np.exp(-np.log(f/112)**2/.08)                           # la caisse close
    dB += -2.5*np.exp(-np.log(f/450)**2/.25)                          # le creux des bas-médiums
    dB += 4.5*np.exp(-np.log(f/2400)**2/.04) + 2.5*np.exp(-np.log(f/4000)**2/.03)
    dB += -10*np.log10(1 + (f/5500)**12)                              # la chute : environ -60 dB/octave
    ond = np.zeros_like(f); k = (f > 700) & (f < 7000)
    for _ in range(14): c = np.exp(rng.uniform(np.log(800), np.log(6500))); ond += rng.uniform(-2.4, 2.4)*np.exp(-np.log(f/c)**2/rng.uniform(.002, .01))
    dB += ond*k
    mag = 10**(dB/20); c = np.fft.irfft(np.log(mag + 1e-9), N)            # cepstre réel
    w = np.zeros(N); w[0] = 1; w[1:N//2] = 2; w[N//2] = 1
    h = np.fft.irfft(np.exp(np.fft.rfft(c*w, N)), N)[:L]; h *= np.hanning(2*L)[L:]**.5
    return h/np.sqrt((h**2).sum())*.5
# les deux breaks du breakcore, composés et enregistrés par construire_breaks.py
for k in ('brk_a', 'brk_b', 'brk_c', 'brk_d', 'brk_e', 'brk_f', 'tamb', 'tamb_o', 'bongo_h', 'bongo_b', 'k_x0x', 'k_808', 'sn_x0x', 'hx_f', 'hx_o', 'rim_x0x'): x, r = sf.read(f'banque-hc/{k}.wav'); B[k] = (x, r)
B['cab_g'] = (baffle(11), 48000); B['cab_d'] = (baffle(29), 48000)
# la jungle n'emploie plus la forge, l'ouvert du charleston ni les effets de guitare : ils quittent la banque
for k in ('enclume', 'chaine', 'tiges', 'epee', 'os', 'hh_o', 'g_pique', 'g_harm', 'g_racle', 'china', 'brk_d'): B.pop(k, None)   # ni la forge, ni la china, ni le break nerveux
idx, bloc, pos = {}, [], 0
for k, (x, sr) in B.items():
    pcm = np.round(np.clip(x/(np.abs(x).max()*1.0001) if k.startswith('cab') else x, -1, 1)*32767).astype('<i2')   # le baffle : normalisé, son gain à part
    if k.startswith('cab'): idx[k] = [pos, len(pcm), sr, float(np.abs(x).max())]   # le gain d'origine, pour la restituer
    else: idx[k] = [pos, len(pcm), sr]
    bloc.append(pcm.tobytes()); pos += len(pcm); sf.write(f'banque-hc/{k}.wav', x/np.abs(x).max()*.9 if k.startswith('cab') else x, sr)
octets = b''.join(bloc); open('banque-hc/sons-hc.b64', 'w').write(base64.b64encode(octets).decode()); json.dump(idx, open('banque-hc/index.json', 'w'))
print(len(octets)//1000, 'ko ;', len(base64.b64encode(octets))//1000, 'ko en base64'); print(json.dumps(idx))

# Ensuite : python3 convertir_flac.py page.html page.html — la page publiée embarque les banques en FLAC (voir convertir_flac.py).
