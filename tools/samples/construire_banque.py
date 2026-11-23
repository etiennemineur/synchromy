"""Banque d'échantillons de Synchromy (Uptempo) : des sons CC0 (domaine public), découpés, nettoyés, normalisés,
rangés en un seul bloc PCM 16 bits mono, avec un index JSON. Sources : stargate-sample-pack (CC0) et
meadowlark-factory-library (CC0), tous deux sur GitHub."""
import soundfile as sf, numpy as np, json, base64, scipy.signal as S
SG = 'sources/stargate-sample-pack/stargate-sample-pack/'
ML = 'sources/meadowlark-factory-library/Drums - One Shot/'
def lire(f, sr=44100):
    d, r = sf.read(f, always_2d=True); m = d.mean(1)
    if r != sr: m = S.resample_poly(m, sr, r)
    return m
def hp(x, f, sr=44100, o=2): b, a = S.butter(o, f/(sr/2), 'high'); return S.lfilter(b, a, x)
def lp(x, f, sr=44100, o=2): b, a = S.butter(o, f/(sr/2), 'low'); return S.lfilter(b, a, x)
def debut(x, seuil=.02):   # le premier échantillon audible : l'attaque tombe pile au départ
    i = np.argmax(np.abs(x) > seuil*np.abs(x).max()); return x[max(0, i - 8):]
def couper(x, dur, fondu, sr=44100):
    n = int(dur*sr); x = x[:n].copy(); f = int(fondu*sr); x[-f:] *= np.linspace(1, 0, f)**2; return x
def norm(x, pic=.97): return x/np.abs(x).max()*pic
B = {}
# les attaques (« tok ») des kicks : la partie haute seulement, le corps reste synthétisé et accordé
B['tok_piep']  = norm(couper(hp(debut(lire(SG + 'freesound/drums/kick/348785__arctis__hardstyle-rawstyle-kick-8.wav')), 180), .045, .02))
B['tok_zaag']  = norm(couper(hp(debut(lire(SG + 'freesound/drums/kick/192512__nabz871__knife-party-like-kick.wav')), 220), .04, .018))
B['tok_tok']   = norm(couper(hp(debut(lire(ML + 'Kicks/Snappy/Kickmess Snappy 1.wav')), 120), .055, .025))
B['tok_rond']  = norm(couper(hp(debut(lire(ML + 'Kicks/Deep/Kickmess Deep 3.wav')), 90), .06, .03))
B['tok_dist']  = norm(couper(hp(debut(lire(ML + 'Kicks/Distorted/Kickmess Distorted 1.wav')), 160), .05, .02))
# le clap : deux claps superposés, calés sur leur crête
c1 = debut(lire(SG + 'fugue-state-audio/drums/claps/x0xproc1-clap.wav')); c2 = debut(lire(SG + 'freesound/drums/clap/388042__sami-kullstrom__clap.wav'))
n = min(len(c1), len(c2), int(.42*44100)); B['clap'] = norm(couper(hp(c1[:n]*.8 + c2[:n]*.7, 250), .4, .12))
B['hat_ouvert'] = norm(couper(hp(debut(lire(SG + 'freesound/drums/cymbal/open/165028__rodrigo-the-mad__mini-909ish-open-hat.wav')), 3000), .1, .04))
B['hat_ferme'] = norm(couper(hp(debut(lire(SG + 'fugue-state-audio/drums/hihats/x0xproc2-hatclsd.wav')), 4000), .08, .03))
B['crash'] = norm(couper(hp(debut(lire(SG + 'freesound/drums/cymbal/crash/104210__minorr__crash-paiste-pst5-17-left-ff.wav')), 300), 2.6, 1.6))
# les voix, à 22 050 Hz (la voix n'en demande pas plus)
for nom, f in [('voix_go', "microlag/One-Shots/Vocals/One_Two_Three_Let's_Go.wav"), ('voix_insane', "microlag/One-Shots/Vocals/That's_Insane.wav")]:
    v = lire(SG + f, 22050); v = v[:np.nonzero(np.abs(v) > .005)[0][-1] + 2205]; B[nom] = (norm(hp(v, 120, 22050)), 22050)
# le bloc
idx, bloc, pos = {}, [], 0
for k, v in B.items():
    x, sr = (v if isinstance(v, tuple) else (v, 44100))
    pcm = np.round(np.clip(x, -1, 1)*32767).astype('<i2'); idx[k] = [pos, len(pcm), sr]; bloc.append(pcm.tobytes()); pos += len(pcm)
    sf.write(f'banque/{k}.wav', x, sr)
octets = b''.join(bloc)
open('banque/sons.bin', 'wb').write(octets); json.dump(idx, open('banque/index.json', 'w'))
open('banque/sons.b64', 'w').write(base64.b64encode(octets).decode())
print(len(octets)/1000, 'ko ;', len(base64.b64encode(octets))/1000, 'ko en base64'); print(json.dumps(idx))

# Ensuite : python3 convertir_flac.py page.html page.html — la page publiée embarque les banques en FLAC (voir convertir_flac.py).
