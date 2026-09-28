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
