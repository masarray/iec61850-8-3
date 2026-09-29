# Architecture

## Non-negotiable product boundary

`iec61850-8-3` must run as a complete laboratory endpoint **without Netbeheer, Python, or another IEC 61850-8-3 implementation installed**.

Netbeheer is retained only for optional interoperability tests and captured behavioral evidence.

The intended sequence is:

```text
1. iec61850-8-3 native server + headless web lab
2. prove standalone discovery/data/reporting
3. add native client in this repo for self-test
4. integrate DMS client adapter into ARStack61850
5. ARStack reads/subscribes to this repo's server
6. external Netbeheer tests remain optional compatibility evidence
```

## Target shape

```text
                Browser
                  |
          Local HTTP / event API
                  |
        +---------------------+
        | Headless Web Lab    |
        | control + telemetry |
        +---------------------+
                  |
            Native C++ API
                  |
+----------------------------------------+
| Canonical IEC 61850 domain / ACSI      |
| LD/LN/DO/DA · DataSet · RCB · Reports  |
+----------------------------------------+
                  |
             DMS services
                  |
       +-----------------------+
       | Session / state       |
       | request correlation   |
       +-----------------------+
          |               |
       Codec           Trace/events
   BER/DER/JER          bounded
          |
    IWireTransport
          |
      WebSocket
```

The browser never owns protocol state. Closing or refreshing a browser tab must not stop the DMS server.

## Runtime workers

The native runtime is split into bounded execution paths:

- **I/O worker** — WebSocket accept/connect, frame RX/TX;
- **decode worker** — wire bytes to canonical PDU;
- **service worker** — association, requests, RCB/report state;
- **model worker** — serialized mutations to the simulated IED model;
- **observer/event path** — bounded trace and UI telemetry;
- **HTTP control plane** — management only; it must not execute protocol work inline.

A slow browser, inspector, logger or trace consumer must not block the DMS service path.

## Headless-first rule

Everything required for testing must be available without a browser:

- start/stop server;
- inspect health;
- load/sample a model;
- mutate simulator values;
- inspect session state;
- capture traces;
- subscribe to report events.

The web UI is a client of that headless API, not the engine itself.

## Server-first milestone

The first complete interoperable endpoint is the server.

```text
Native model -> DMS service engine -> BER -> WebSocket
```

The server must be capable of association, discovery and data reads before ARStack integration begins.

## Client state

```text
Disconnected -> Connecting -> Associated -> Releasing -> Disconnected
                         \-> Failed
```

Each confirmed request owns an `invokeId`, timestamp, service identity and timeout budget. Outstanding requests are bounded.

## Reporting

Reporting is event driven. A report update enters the application as a `ReportEvent` carrying:

- report ID;
- DataSet reference;
- sequence number;
- configuration revision;
- reason-for-inclusion per member;
- values and, when present, quality/timestamp.

The web UI receives these through a push/event channel. It does not poll `GetDataValues` to imitate reporting.

An explicit verification read may be offered as a diagnostic action, but it is never a hidden fallback.

## ARStack integration boundary

ARStack61850 should not import the browser/control-plane layer.

The integration target is a narrow native adapter:

```text
ARStack canonical/application layer
             |
        IDmsClientAdapter
             |
       DMS services
             |
        codec/transport
```

Where practical, common canonical primitives may later move to a small shared library. Until that is justified, keep the repositories decoupled and test them over the wire.

## Compatibility strategy

Netbeheer Nederland's public implementation is treated as:

- a behavioral oracle for the current preliminary profile;
- a source of attributed public schema/test vectors when licensing permits;
- an optional external interoperability endpoint.

It is not linked, embedded, spawned, downloaded or required by the native runtime.


## Scale architecture

The correctness-oriented canonical tree remains the semantic source, but million-point operation requires data-oriented indexes around it and eventually a compact indexed storage representation.

Target runtime path:

```text
external reference
      |
reference index
      v
stable SignalId
      |
mutable value store
      |
reverse subscription index
      v
interested RcbIds only
      |
bounded BufTm accumulators / scheduler
      |
report builder from DataSet member IDs
      |
bounded egress + optional BRCB journal
```

The high-cardinality implementation must avoid:

- full-model scans for ordinary point reads/updates;
- full-RCB scans after every point mutation;
- one timer/thread per RCB;
- repeated reference-string parsing in resolved hot paths;
- whole-model JSON serialization for ordinary browser navigation.

Model topology should become mostly immutable after finalization. Mutable values/report state are stored separately so updates do not copy or lock large metadata structures.

The browser/control plane becomes cursor/paged/lazy at large scale. A slow browser remains outside the protocol critical path.

See [SCALABILITY.md](SCALABILITY.md) for benchmark gates and [../AGENTS.md](../AGENTS.md) for mandatory implementation rules.

## Engineering change policy

Early phases are not permission to create throwaway production paths.

When a temporary/simple test implementation is needed:

1. keep it behind a test/example boundary;
2. label it explicitly;
3. do not wire production services through it;
4. preserve the stable-ID/index/backpressure migration path.

Any change to concurrency, indexing, buffering or ownership must document its complexity and overload behavior.
