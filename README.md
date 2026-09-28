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

The server core is **not yet a network server**. WebSocket transport, GetDataDefinition/GetDataValues responses, DataSets, RCB/reporting and the headless Web Workbench are the next gates.

See:

- [Architecture](docs/ARCHITECTURE.md)
- [Roadmap](docs/ROADMAP.md)
- [Netbeheer engine audit](docs/NETBEHEER_ENGINE_AUDIT.md)
- [Third-party provenance](THIRD_PARTY_NOTICES.md)

## Next milestones

### P2 — standalone native DMS server

- WebSocket transport;
- independent IEC role vs active/passive WebSocket role;
- GetDataDefinition and complete GetDataValues;
- native server CLI;
- deterministic server integration tests.

### P3 — headless Web Lab

- local HTTP control plane;
- model/signal API;
- trace event stream;
- Server Workbench and Protocol Inspector;
- browser lifecycle independent from protocol lifecycle.

### P4 — DataSets and real event-driven reporting

- DataSet directory/values;
- BRCB/URCB state;
- `RptEna`, GI, TrgOps and ConfRev;
- reason-for-inclusion;
- event-driven report delivery with no hidden polling fallback.

### P5/P6 — native client then ARStack61850

First prove our own independent client/server loopback. Then implement the ARStack61850 DMS client adapter and test ARStack against this repository's server.

## Build

```bash
cmake -S . -B build -DAR61850_DMS_BUILD_TESTS=ON -DAR61850_DMS_BUILD_EXAMPLES=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CI builds and tests the native core on both Linux and Windows.

## License

GPL-3.0. Third-party reference material keeps its original notices. See `THIRD_PARTY_NOTICES.md`.
