# Third-party notices

Synchromy is released under the [MIT License](LICENSE). It includes, or was built with, the third-party material listed here. Each item keeps its own license.

## Fonts

**IBM Plex Mono** (Regular, Medium, Bold), embedded as WOFF2 in `web/synchromy.html`.
Copyright © 2017 IBM Corp., with Reserved Font Name "Plex". Licensed under the [SIL Open Font License 1.1](https://openfontlicense.org).
Source: <https://github.com/IBM/plex>

## Sound samples

All instruments are synthesized by the program. Only the two sets, *make me dance* and *make me happy*, use short drum and voice samples, embedded in `web/synchromy.html` (FLAC) and in `native/engine/jg_sons.h` (PCM). Every source sample was published under [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/) (public domain dedication). No attribution is required; the sources are credited here with thanks.

| Source | Used for | License |
| --- | --- | --- |
| [Stargate Sample Pack](https://github.com/stargatedaw/stargate-sample-pack): Karoryfer acoustic drums | kicks, snares, toms, hi-hats, ride, crash and metal percussion of *make me dance*, and the drum kit the breakbeats are played on | CC0 1.0 |
| Stargate Sample Pack: Fugue State Audio | analog drum machine kick, snare, hi-hats and clap | CC0 1.0 |
| Stargate Sample Pack: freesound contributions (arctis, nabz871, sami-kullstrom, rodrigo-the-mad, minorr, menegass) | two kicks, a clap, an open hi-hat and a crash of *make me happy*; bongos in the breakbeats | CC0 1.0 |
| Stargate Sample Pack: microlag | two short spoken phrases | CC0 1.0 |
| [Meadowlark Factory Library](https://github.com/BillyDM/meadowlark-factory-library): Kickmess | three kicks of *make me happy* | CC0 1.0 |

The samples were trimmed, filtered and normalized by the scripts in `tools/samples`.

**The breakbeats are original.** They were composed and rendered for Synchromy from the single drum hits above, together with a synthesized tambourine. Synchromy contains no excerpt of any commercial recording and no sampled break from an existing record.

## Faust

The mastering chain (`tools/faust/maitre.dsp`, MIT) is compiled with [Faust](https://faust.grame.fr) 2.70.3 and embedded as WebAssembly in `web/synchromy.html`. The compiled code contains functions from the Faust libraries:

- `filters.lib`, `reverbs.lib` (zita_rev1), parts of `signals.lib`: Copyright © 2003-2019 Julius O. Smith III, MIT-style STK-4.3 license.
- `maths.lib`, `basics.lib`, `delays.lib`, `routes.lib`, `analyzers.lib`, `signals.lib`: GRAME and contributors (Jonatan Liljedahl, Romain Michon, Dario Sanfilippo, Remy Muller, Bart Brouns), LGPL with an exception that allows code generated from the libraries to be distributed under any license.

Source: <https://github.com/grame-cncm/faustlibraries>

## Build tools

Not distributed with Synchromy, used to build it:

- [terser](https://github.com/terser/terser) (BSD 2-Clause), to minify the web program.
- [XcodeGen](https://github.com/yonaskolb/XcodeGen) (MIT), to generate the Xcode projects.
- [mingw-w64](https://www.mingw-w64.org), to build the Windows version. The Windows binaries link its runtime statically (public domain and permissive licenses, including the winpthreads library under MIT and BSD terms).

## Tribute

Synchromy is named after, and pays tribute to, *Synchromy* (1971) by Norman McLaren, produced by the National Film Board of Canada. This project is independent, is not affiliated with or endorsed by the National Film Board of Canada or the estate of Norman McLaren, and contains no image or sound from the film.
