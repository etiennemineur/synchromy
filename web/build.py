#!/usr/bin/env python3
"""Build the published version of Synchromy from the single-file source.

    python3 build.py [output-dir] [--readable]

The source of truth is `synchromy.html`: one self-contained page (markup, styles, program, fonts and sound banks).
This script splits it into what a web server should deliver:

    index.html      French page (the interface switches language in place)
    en/index.html   English page (same program, English metadata)
    app.js          the program, minified with terser (use --readable to keep it as written)
    sons.bin        sound bank of "make me happy"  (FLAC files, loaded on demand)
    sons-hc.bin     sound bank of "make me dance"  (FLAC files, loaded on demand)
    fonts/          IBM Plex Mono, three weights

Requirements: Python 3.9+, and Node.js 18+ with `npm ci` run once in this folder (terser), unless --readable is given.
"""
import base64, hashlib, os, re, subprocess, sys

ICI = os.path.dirname(os.path.abspath(__file__))
args = [a for a in sys.argv[1:] if not a.startswith('--')]
SORTIE = os.path.abspath(args[0]) if args else os.path.join(ICI, 'dist')
LISIBLE = '--readable' in sys.argv

s = open(os.path.join(ICI, 'synchromy.html'), encoding='utf-8').read()
os.makedirs(os.path.join(SORTIE, 'en'), exist_ok=True); os.makedirs(os.path.join(SORTIE, 'fonts'), exist_ok=True)

# 1. les banques d'échantillons : hors de la page, en fichiers binaires
banques = {}
for ident, fichier in [('sons-synchromy', 'sons.bin'), ('sons-hc', 'sons-hc.bin')]:
    m = re.search(r'<script type="application/octet-stream" id="' + ident + r'">(.*?)</script>\n', s, re.S)
    assert m, 'sound bank not found: ' + ident
    banques[fichier] = base64.b64decode(m.group(1).strip()); s = s[:m.start()] + s[m.end():]

# 2. le programme : à part, minifié (noms locaux raccourcis, code mort retiré ; le comportement ne change pas)
m = re.search(r'<script>(.*?)</script>', s, re.S); assert m, 'program not found'
js = m.group(1); ecrit = len(js.encode())
if not LISIBLE:
    try:
        r = subprocess.run(['npx', '--no-install', 'terser', '--compress', 'passes=2', '--mangle', '--format', 'ascii_only=false,comments=false'],
                           input=js, capture_output=True, text=True, encoding='utf-8', cwd=ICI)
    except FileNotFoundError:
        sys.exit('Node.js not found. Install it and run `npm ci` in web/, or build with --readable.')
    if r.returncode != 0 or len(r.stdout) < 100000:
        sys.exit('terser failed. Run `npm ci` in web/ first, or build with --readable.\n' + r.stderr[:800])
    js = '/* Synchromy — https://github.com/etiennemineur/synchromy — MIT License */\n' + r.stdout.strip() + '\n'
empreinte = hashlib.sha256(js.encode()).hexdigest()[:12]   # dans l'adresse du programme : un nouveau fichier n'est jamais servi depuis un vieux cache
html = s[:m.start()] + f'<script src="app.js?v={empreinte}"></script>' + s[m.end():]

# 3. les polices : hors de la page, en fichiers
for graisse in ('400', '500', '700'):
    mf = re.search(r'(@font-face\{font-family:"IBM Plex Mono";font-style:normal;font-weight:' + graisse + r';font-display:swap;src:)url\(data:font/woff2;base64,([^)]*)\)', html)
    assert mf, 'font not found: ' + graisse
    open(os.path.join(SORTIE, 'fonts', f'plex-mono-{graisse}.woff2'), 'wb').write(base64.b64decode(mf.group(2)))
    html = html[:mf.start()] + mf.group(1) + f'url(fonts/plex-mono-{graisse}.woff2)' + html[mf.end():]

# 4. la page anglaise : le même programme, les métadonnées en anglais
en = html.replace('<html lang="fr">', '<html lang="en">', 1).replace('url(fonts/', 'url(../fonts/').replace('<script src="app.js', '<script src="../app.js')
TRADUCTIONS = [
    ('<title>Synchromy — dessiner, c’est jouer |', '<title>Synchromy — to draw is to play |'),
    ('content="Synchromy — dessiner, c’est jouer"', 'content="Synchromy — to draw is to play"'),
    ('Synchromy : chaque trait dépose une forme molle qui coule, sèche et sonne. Le motif choisit l’instrument, la hauteur donne la note, la couleur la tonalité. Un hommage au film de Norman McLaren (1971).',
     'Synchromy: every stroke lays down a soft shape that runs, dries and sounds. The pattern picks the instrument, the height gives the note, the color the key. A tribute to Norman McLaren’s 1971 film.'),
    ('Synchromy — un dessin fait dans l’outil : formes molles à motifs, en noir et blanc et en arc-en-ciel',
     'Synchromy — a drawing made in the tool: soft patterned shapes, in black and white and in rainbow colors'),
    ('<meta property="og:locale" content="fr_FR">', '<meta property="og:locale" content="en_US">'),
    ('href="https://etienne.design/tools/synchromy/"', 'href="https://etienne.design/tools/synchromy/en/"'),
    ('content="https://etienne.design/tools/synchromy/"', 'content="https://etienne.design/tools/synchromy/en/"'),
]
for fr, anglais in TRADUCTIONS:
    assert fr in en, 'text to translate not found: ' + fr[:50]
    en = en.replace(fr, anglais)

open(os.path.join(SORTIE, 'index.html'), 'w', encoding='utf-8').write(html)
open(os.path.join(SORTIE, 'en', 'index.html'), 'w', encoding='utf-8').write(en)
open(os.path.join(SORTIE, 'app.js'), 'w', encoding='utf-8').write(js)
for fichier, octets in banques.items(): open(os.path.join(SORTIE, fichier), 'wb').write(octets)
print(f'{SORTIE}: app.js {len(js.encode())//1000} kB' + ('' if LISIBLE else f' (written: {ecrit//1000} kB)') + f', v={empreinte}, ' + ', '.join(f'{k} {len(v)//1000} kB' for k, v in banques.items()))
