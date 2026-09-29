# Third-party notices and reference provenance

## Netbeheer Nederland — iec61850-websocket

Reference repository: https://github.com/Netbeheer-Nederland/iec61850-websocket

Pinned research baseline:

`f78b0d55780aecab551fb5fd1e75e916bc120215`

License: Apache License 2.0.

This project uses the Netbeheer implementation as an interoperability reference and behavioral oracle for the preliminary IEC 61850-8-3 research profile.

The native runtime in this repository does **not** import, link, spawn or require the Netbeheer Python implementation.

### Current reference-derived evidence

The protocol tests contain small BER message vectors captured from/validated against the pinned FT20 laboratory behavior for association and read-only directory services. They are retained as interoperability evidence and are not a replacement for the normative standard.

No Netbeheer runtime source file is copied into the native engine.

### Future schema/material imports

If the public preliminary ASN.1 schema, generated derivatives, or other source material are imported into this repository, the relevant Apache-2.0 copyright/license notices must be retained and the exact upstream commit plus content hash must be recorded in a provenance manifest.

The presence of a public draft/reference implementation does not imply IEC conformance or final-standard status.


## Machine Zone — IXWebSocket

Reference/dependency repository: https://github.com/machinezone/IXWebSocket

Pinned commit:

`514a0b968503d758a2954ff9016289f41f489616`

License: BSD 3-Clause.

IXWebSocket is used only as the native WebSocket transport implementation. IEC 61850 association, BER, service dispatch, model semantics, reporting semantics, tracing and simulator state remain implemented in this repository.

The dependency is pinned at configure/build time and is not downloaded or loaded dynamically by the resulting server executable at runtime.
