# Contributing to Synchromy

Thank you for your interest. Synchromy is the personal work of one designer, shared so that it can be studied, built and adapted. Reports and focused fixes are welcome; you may write in English or in French.

## Reporting a bug

Open an [issue](https://github.com/etiennemineur/synchromy/issues/new/choose) and include:

- the platform (web, Mac app, Mac screen saver, Apple TV, Windows) and its version;
- for the web tool: the browser and its version, the device, and the language of the interface;
- the steps that lead to the problem, what you expected and what happened;
- a screenshot, a short video, or an HTML export of the scene when it helps.

Please search existing issues first.

## Suggesting a change

Open an issue before writing code for anything larger than a fix. Synchromy has a deliberate visual and sonic identity, and a change that alters how it looks or sounds is a design decision: it is easier to agree on it first.

## Pull requests

1. Fork the repository and create a branch from `main`.
2. Keep the change small and limited to one subject.
3. Check your work:
   - **Web.** Open `web/synchromy.html` in at least one Chromium browser and Safari or Firefox; draw, start auto-draw, start each set, switch the language, and check that the console stays clean. Then run `python3 build.py dist` in `web/` and check the built page the same way.
   - **Native.** Build the targets your change touches (see the [README](README.md#build)).
4. Describe what the change does and how you tested it.

### Code conventions

- `web/synchromy.html` is the single source of the web tool. Do not commit `web/dist/`.
- The web program uses no framework and no run-time dependency. Please keep it that way.
- The C engine mirrors the web program function by function. A change in behavior should be made in both, or the pull request should say which side is still to do.
- Comments and identifiers are in French, like the existing code. Messages shown to users exist in French and in English: update both.
- Follow the style of the surrounding code. `.editorconfig` holds the basics.

### Sounds and assets

Only add material you created yourself or that is in the public domain (CC0). State the source in the pull request and in `THIRD_PARTY_NOTICES.md`. Samples taken from commercial recordings, including well-known breakbeats, will not be accepted.

### Commit messages

Use a short imperative summary in English (for example `Fix the tempo display at 200% pattern speed`), then a blank line and any useful detail.

## License

By contributing, you agree that your contribution is licensed under the [MIT License](LICENSE).

## Contact

Etienne Mineur, <etienne@etienne.design>
