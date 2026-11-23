# Security policy

## Supported versions

Only the latest release, and the version online at <https://etienne.design/tools/synchromy/>, receive fixes.

## Reporting a vulnerability

Please do not open a public issue for a security problem. Write to <etienne@etienne.design>, or use GitHub's [private vulnerability reporting](https://github.com/etiennemineur/synchromy/security/advisories/new), with:

- a description of the problem and its impact;
- the steps to reproduce it;
- the platform and version concerned.

You will receive an answer as soon as possible. Please allow time for a fix before any public disclosure.

## Scope

Synchromy runs entirely on the device: the web tool has no server side, no account and sends no data. The points most worth reporting are those that could affect a visitor or a user, such as script injection through an exported HTML page, or memory errors in the native engine.
