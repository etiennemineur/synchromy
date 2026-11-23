"""Les banques d'échantillons de Synchromy, de PCM 16 bits en base64 vers un conteneur FLAC (sans perte, environ 2,3 fois plus léger).
Le conteneur : les fichiers FLAC mis bout à bout ; l'index du programme donne pour chaque son [décalage, taille en octets, fréquence].
Les sons que le programme ne cite plus sont laissés de côté.

  python3 convertir_flac.py entree.html sortie.html
"""
import re, json, base64, io, sys, numpy as np, soundfile as sf
ENTREE, SORTIE = sys.argv[1], sys.argv[2]
INUTILES = {'hat_ferme', 'bongo_h', 'bongo_b', 'rim_x0x', 'cab_g', 'cab_d'}
s = open(ENTREE).read(); bilan = []
m = re.search(r'<script>(.*?)</script>', s, re.S); js = m.group(1)
for ident, nom_index in [('sons-synchromy', 'SONS_INDEX'), ('sons-hc', 'SONS_HC_INDEX')]:
    mb = re.search(r'(<script type="application/octet-stream" id="' + ident + r'">)(.*?)(</script>)', s, re.S)
    brut = base64.b64decode(mb.group(2).strip()); pcm = np.frombuffer(brut, dtype='<i2')
    mi = re.search(r'const ' + nom_index + r' = (\{.*?\});', s); idx = json.loads(mi.group(1))
    blocs, neuf, pos = [], {}, 0
    for nom, v in idx.items():
        if nom in INUTILES: continue
        assert len(v) == 3, (nom, v)   # plus aucun gain particulier
        x = pcm[v[0]:v[0] + v[1]]; f = io.BytesIO(); sf.write(f, x, v[2], format='FLAC', subtype='PCM_16', compression_level=1.0)
        o = f.getvalue(); y, sr = sf.read(io.BytesIO(o), dtype='int16'); assert sr == v[2] and np.array_equal(x, y), nom   # sans perte, vérifié
        # la référence (nom entre guillemets) existe encore dans le programme, hors de l'index
        assert len(re.findall(r"['\"]" + re.escape(nom) + r"['\"]", js)) >= 2 or nom.startswith('brk_'), nom
        neuf[nom] = [pos, len(o), v[2]]; blocs.append(o); pos += len(o)
    octets = b''.join(blocs); b64 = base64.b64encode(octets).decode()
    bilan.append((ident, len(mb.group(2).strip()), len(octets), len(b64), len(idx), len(neuf)))
    s = s[:mb.start(2)] + b64 + s[mb.end(2):]
    mi = re.search(r'const ' + nom_index + r' = (\{.*?\});', s); s = s[:mi.start(1)] + json.dumps(neuf) + s[mi.end(1):]
open(SORTIE, 'w').write(s)
for b in bilan: print('%s : %d Ko (PCM en base64) → %d Ko (FLAC binaire), %d Ko en base64 dans la page ; %d sons → %d' % (b[0], b[1]//1024, b[2]//1024, b[3]//1024, b[4], b[5]))
