# IEC 61850-8-3 Experimental Engine

A public, native C++20 implementation project for the emerging IEC 61850-8-3 client/server mapping.

> Status: **experimental / pre-standard research**. This repository is not an IEC conformance claim and does not represent a final IEC 61850-8-3 implementation.

## Why this repository exists

The project provides an engine we can own, test, optimize and evolve independently for:

- IEC 61850-8-3 DMS client and server research;
- WebSocket-based laboratory interoperability testing;
- deterministic model discovery and data access;
- event-driven reporting and GI testing;
- a headless simulator for CI and regression tests;
- future integration with ARStack61850 without coupling ACSI semantics to MMS or DMS.

Netbeheer Nederland's public `iec61850-websocket` project is used as an **interoperability reference and behavioral oracle**, not as the runtime engine of this repository.

## Architecture principles

1. **Canonical ACSI/model core** — protocol mapping never owns the IEC 61850 model.
2. **Transport isolation** — WebSocket is an adapter behind `IWireTransport`.
3. **Codec isolation** — BER/DER/JER plug into the same canonical PDU model.
4. **Event-driven runtime** — reports are pushed as events; cyclic reads remain an explicit client policy.
5. **Bounded workers and queues** — no unbounded background growth.
6. **Client/server symmetry** — shared codecs, model types, tracing and service semantics.
7. **Evidence first** — protocol traces, golden vectors and interoperability tests are first-class artifacts.

## P0 implemented

- dependency-light C++20 static core;
- canonical data, quality, timestamp and report types;
- service/message taxonomy;
- association/session state and bounded outstanding request tracking;
- bounded worker queue using `std::jthread`;
- bounded protocol trace buffer;
- CMake + CTest baseline;
- small loopback/core example.

## Planned next

### P1 — DMS wire foundation

- import/pin the public preliminary ASN.1 schema with attribution;
- canonical TpaaPdu representation;
- bounded BER codec compatible with the Netbeheer FT20 laboratory profile;
- association / release / abort;
- golden vectors captured from the reference implementation.

### P2 — read-only client/server interoperability

- WebSocket adapter;
- `GetServerDirectory`;
- `GetLogicalDeviceDirectory`;
- `GetLogicalNodeDirectory`;
- `GetDataDirectory` / `GetDataDefinition` / `GetDataValues`;
- automated AR engine ↔ Netbeheer reference tests.

### P3 — DataSets and event-driven reporting

- DataSet directory/values;
- BRCB/URCB state model;
- `RptEna`, GI, TrgOps, ConfRev and reason-for-inclusion;
- event-driven report delivery with no hidden polling fallback.

### P4 — simulator, tracing and tooling API

- headless IED simulator;
- signal mutation API;
- protocol inspector event stream;
- deterministic scenarios and CI matrix;
- future web workbench backed by this native engine.

## Build

```bash
cmake -S . -B build -DAR61850_DMS_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## License

GPL-3.0. Third-party reference material keeps its original notices. See `THIRD_PARTY_NOTICES.md`.
