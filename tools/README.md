# Tools

Material used to prepare Synchromy, kept here so that every embedded asset can be traced and rebuilt. None of it is needed to run or build the tool.

## `faust/maitre.dsp`

The mastering chain of the web tool: send reverb (zita-rev1), upward and downward multiband compression, alias-free soft clipper (ADAA) and look-ahead limiter. It is compiled to WebAssembly with [Faust](https://faust.grame.fr) 2.70.3 and embedded in `web/synchromy.html`:

```sh
faust -lang wasm -ct 1 -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 2 maitre.dsp -o maitre.wasm
```

## `samples/`

The Python scripts that derived the sound banks of the two sets from public-domain (CC0) sources. They are a record of provenance: each line names the source file and the treatment applied (trim, filter, normalize).

| Script | Role |
| --- | --- |
| `construire_banque.py` | bank of *make me happy* |
| `construire_breaks.py` | renders the original breakbeats from single drum hits |
| `construire_banque_hc.py` | bank of *make me dance* |
| `convertir_flac.py` | packs the banks as lossless FLAC inside the page, and drops unused samples |

To run them, clone the two source libraries into `samples/sources/`:

```sh
git clone https://github.com/stargatedaw/stargate-sample-pack samples/sources/stargate-sample-pack
git clone https://github.com/BillyDM/meadowlark-factory-library samples/sources/meadowlark-factory-library
pip install numpy scipy soundfile
```

Some lines of these scripts prepare samples that the program no longer uses; `convertir_flac.py` leaves them out of the published banks.

Licenses of the sources: see [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).
