# IEC 61850-8-3 Experimental Engine

A public, native C++20 implementation project for the emerging IEC 61850-8-3 client/server mapping.

> Status: **experimental / pre-standard research**. This repository is not an IEC conformance claim and does not represent a final IEC 61850-8-3 implementation.

## Product goal

Build a standalone native DMS laboratory endpoint that runs without Python or the Netbeheer runtime:

```text
Native IEC 61850-8-3 engine
        |
Headless DMS server
        |
Local control/event API
        |
Browser Workbench + Inspector
```

After this server is proven, ARStack61850 becomes an independent second implementation and connects to it over the actual DMS wire protocol.

Netbeheer Nederland's public `iec61850-websocket` project is used only as an **optional interoperability oracle and source of pinned public research evidence**.

## Architecture principles

1. **Canonical ACSI/model core** — protocol mapping never owns the IEC 61850 model.
2. **Transport isolation** — WebSocket is an adapter behind `IWireTransport`.
3. **Codec isolation** — BER/DER/JER plug into the same canonical PDU model.
4. **Headless first** — protocol/model state lives outside the browser.
5. **Event-driven reporting** — reports are pushed as events; cyclic reads are explicit client policy.
6. **Bounded resources** — workers, request tables, traces and parser limits are finite.
7. **Client/server symmetry** — shared codecs, model types, tracing and service semantics.
8. **Evidence first** — protocol traces, golden vectors and interoperability tests are first-class artifacts.

## Implemented on the current development branch

### Native runtime foundation

- dependency-light C++20 static core;
- canonical data, quality and timestamp types;
- typed hierarchical LD/LN/DO/DA model;
- deterministic FT20-style sample model;
- bounded worker queue using `std::jthread`;
- bounded invoke-ID/session tracking;
- bounded protocol trace buffer.

### Preliminary DMS BER foundation

- typed `DmsPdu` service model;
- strict BER TLV reader/writer;
- explicit message/TLV/nesting/child limits;
- rejection of indefinite BER lengths, malformed/truncated TLVs and non-minimal lengths;
- Associate request/response;
- Release and Abort lifecycle;
- GetServerDirectory;
- GetLogicalDeviceDirectory;
- GetLogicalNodeDirectory;
- GetDataDirectory;
- GetDataValues request parsing foundation;
- service-error response foundation.

### Standalone server core

The in-memory `ServerCore` already answers association and read-only discovery from its own native model. It does not import, launch or require Netbeheer.

Captured FT20 BER responses are used as golden interoperability vectors. For the covered services, the native server is tested for byte-compatible output against those pinned vectors.

## Current boundary

The P2 read-only standalone server foundation is now in place: native WebSocket transport, association/discovery, GetDataDefinition and GetDataValues run through the real DMS wire path without Python or Netbeheer.

The P3 Headless Web Lab foundation is now active on the development branch: the same native process serves a compact engineering Workbench with Server, Model, Signals and Protocol Inspector views. The browser remains an observer/control surface; protocol state stays in the native runtime.

The local control plane exposes health/readiness, transport lifecycle, model/tree snapshots, atomic simulator mutations, deterministic scenarios, incremental trace cursors, live RCB state and a delivered-report monitor.

P4 is now implemented on this branch: static DataSets, BRCB/URCB state, GI, TrgOps, BufTm coalescing, dchg/qchg/dupd, periodic integrity reports, sequence/ConfRev continuity, bounded BRCB buffering with EntryID/PurgeBuf/BufOvfl, reconnect replay, and event-driven report delivery over the DMS transport path with no hidden cyclic read fallback.

## Engineering contract

This repository is intentionally **not** developed as disposable PoC code.

All implementation work is governed by:

- [AGENTS.md](AGENTS.md) — mandatory repository-wide coding/architecture rules;
- [Engineering Standard](docs/ENGINEERING_STANDARD.md) — layering, ownership, hot-path, backpressure and review rules;
- [Scalability Plan](docs/SCALABILITY.md) — million-point architecture and benchmark acceptance gates;
- [Implementation Phases](docs/IMPLEMENTATION_PHASES.md) — phase-by-phase completion plan through stable 1.0.

The long-term scale target is at least **1,000,000 leaf signals in one loaded model**. This is a design target until the P9 benchmark gate is completed; the project will not claim validated million-point capability before measured evidence exists.

See also:

- [Architecture](docs/ARCHITECTURE.md)
- [Roadmap](docs/ROADMAP.md)
- [Netbeheer engine audit](docs/NETBEHEER_ENGINE_AUDIT.md)
- [Third-party provenance](THIRD_PARTY_NOTICES.md)

## Next milestones

### P2 — standalone native DMS server

- [x] WebSocket transport;
- [x] independent IEC role vs active/passive WebSocket role;
- [x] GetDataDefinition and GetDataValues;
- [x] native server CLI;
- [x] deterministic real-WebSocket integration tests.

### P3 — headless Web Lab

- [x] local HTTP control plane;
- [x] health/readiness and transport lifecycle API;
- [x] model/tree and signal mutation API;
- [x] deterministic atomic simulator scenarios;
- [x] embedded Server / Model / Signals Workbench;
- [x] embedded Protocol Inspector with incremental trace cursor;
- [x] browser lifecycle independent from protocol lifecycle;
- [~] CI server artifacts are published; release packaging and push-style trace streaming remain.

### P4 — DataSets and real event-driven reporting

- [x] static DataSet directory and values;
- [x] BRCB/URCB discovery and state;
- [x] `RptEna`, GI, TrgOps and ConfRev;
- [x] dchg, qchg and dupd reason-for-inclusion;
- [x] BufTm coalescing;
- [x] periodic integrity scheduling;
- [x] bounded BRCB journal with EntryID, PurgeBuf and BufOvfl;
- [x] BRCB replay after reconnect;
- [x] live browser RCB tree and Reports workspace;
- [x] event-driven report delivery with no hidden polling fallback.

### P5/P6 — native client then ARStack61850

First prove our own independent client/server loopback. Then implement the ARStack61850 DMS client adapter and test ARStack against this repository's server.

## Run the headless lab

After building:

```bash
./build/ar61850_dms_server --mode listen --host 127.0.0.1 --port 8765 --access-point cp1 --http-port 8080
```

Open `http://127.0.0.1:8080/` for the embedded Workbench. The DMS wire endpoint remains separate at `ws://127.0.0.1:8765/cp1`.

## Build

```bash
cmake -S . -B build -DAR61850_DMS_BUILD_TESTS=ON -DAR61850_DMS_BUILD_EXAMPLES=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CI builds and tests the native core on both Linux and Windows.

## License

GPL-3.0. Third-party reference material keeps its original notices. See `THIRD_PARTY_NOTICES.md`.
