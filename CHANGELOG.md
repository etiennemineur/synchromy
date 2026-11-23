# Changelog

Notable changes to Synchromy. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow [Semantic Versioning](https://semver.org/).

## [1.1.0] - 2026-11-23

First public release of the source code.

### Added

- English interface. French and English are switched in place, without reloading the page; the choice is remembered.
- *make me dance*: a mixed set of 90s UK jungle in five styles (ragga, atmospheric, darkside, amen, jump-up), on breakbeats composed for Synchromy.
- Nine op art patterns: Whirl, Arches, Prisms, Peaks, Rifts, Abyss, Frames, Diamonds and Frieze.
- macOS app, with an About panel and menus in French and English.
- New app icon.
- Sources of the web tool, the macOS app, the macOS screen saver, the Apple TV app and the Windows app and screen saver, published under the MIT License.

### Changed

- Sound banks are stored as lossless FLAC and decoded on demand: 3.3 MB down to 1.4 MB.
- The published program is minified: 661 kB down to 371 kB.
- Unused code from earlier sound engines was removed.

### Fixed

- Synthesized partials are kept below the Nyquist frequency at every sample rate.
- The standalone HTML export works from the minified build.
- The drawing surface and the color inputs are described correctly to assistive technologies.

## [1.0.0] - 2026-11

Initial release: web tool, macOS screen saver and Apple TV app.

[1.1.0]: https://github.com/etiennemineur/synchromy/releases/tag/v1.1.0
