# Roadmap

## Product contract

The primary deliverable of this repository is a **standalone IEC 61850-8-3 laboratory server/client engine that runs without the Netbeheer runtime**.

Netbeheer is optional interoperability evidence only. It is never a production/runtime dependency.

The first user-facing milestone is:

```text
Native DMS engine
      |
Headless server
      |
Local Web control plane
      |
Browser UI
```

Only after that standalone server is proven do we integrate IEC 61850-8-3 into ARStack61850 and use ARStack as an independent client against this repository's server.

## P0 — Native core

- [x] C++20 project baseline
- [x] canonical value/quality/time types
- [x] service taxonomy
- [x] bounded session state
- [x] worker abstraction
- [x] bounded trace buffer
- [x] unit-test baseline

## P1 — Preliminary DMS BER profile

- [x] pin public preliminary ASN.1 schema revision
- [~] provenance manifest + upstream git-blob SHA pinned; content SHA-256 when schema is imported
- [x] canonical TpaaPdu model (typed preliminary subset)
- [x] BER decoder foundation with strict size/length/depth limits
- [x] BER encoder foundation
- [x] associate / release / abort lifecycle foundation
- [~] starter malformed/truncation hardening tests; mutation corpus expansion remains
- [x] captured Netbeheer vectors as optional oracle evidence

Exit: our codec can reproduce and decode the pinned laboratory message vectors without loading Netbeheer code at runtime.

## P2 — Standalone native DMS server

- [x] WebSocket transport adapter (passive listen + active connect)
- [x] IEC server role independent of active/passive WebSocket role
- [x] native association state machine (in-memory ServerCore)
- [x] GetServerDirectory
- [x] GetLogicalDeviceDirectory
- [x] GetLogicalNodeDirectory (DataObject class)
- [x] GetDataDirectory
- [x] GetDataDefinition
- [x] GetDataValues
- [x] canonical in-memory IED model
- [x] deterministic sample IED
- [x] server CLI

Exit: the repository launches its own server from a clean machine with no Python/Netbeheer installation.

## P3 — Headless Web Lab

- [x] embedded/local HTTP control plane
- [x] transport lifecycle API while native runtime remains browser-independent
- [x] model/tree API
- [x] signal read/mutation API for simulator values
- [~] incremental trace cursor API + near-real-time browser follow; push stream remains
- [x] browser Server workspace
- [x] browser Inspector workspace
- [x] health/readiness endpoint
- [x] deterministic scenario runner with atomic model batch mutation
- [~] CI publishes standalone Linux/Windows server artifacts; release packaging remains

Exit: a user can launch one executable/service, open the browser, inspect the IED model, modify simulator values and inspect DMS traffic without Netbeheer.

## P4 — DataSets and event-driven reporting

- [x] static DataSet discovery, directory and values
- [x] canonical BRCB/URCB state model
- [x] Get/Set URCB and BRCB BER + native service handling
- [x] GI command semantics + full DataSet report evidence
- [x] trigger options encode/decode and state validation
- [x] unconfirmed Report BER encode/decode
- [~] event-driven report delivery over DMS transport + trace/API visibility; dedicated browser report stream remains
- [~] sequence increment + confRev carried in reports; continuity/fault tests remain
- [x] no hidden cyclic read fallback

Exit: a simulator value change reaches a subscribed client through a real report and updates the browser in real time.

## P5 — Native reference client in this repository

- [ ] headless client CLI
- [ ] browser Client workspace
- [ ] discovery/model builder
- [ ] live values
- [ ] DataSet browser
- [ ] Reporting state UX
- [ ] protocol inspector correlation

Exit: our own client and server interoperate independently, enabling deterministic self-tests and CI.

## P6 — ARStack61850 integration

- [ ] define a narrow DMS adapter boundary in ARStack61850
- [ ] reuse/share canonical semantics rather than Web UI code
- [ ] ARStack DMS client association
- [ ] ARStack discovery against this repository's native server
- [ ] GetDataValues parity checks
- [ ] DataSet parity checks
- [ ] event-driven reporting parity checks
- [ ] interoperability regression matrix: ARStack client <-> native DMS server
- [ ] optional reverse direction when ARStack server support is justified

Exit: ARStack61850 discovers, reads and subscribes to this repository's standalone IEC 61850-8-3 server without Netbeheer in the execution path.

## P7 — External interoperability evidence

- [ ] optional Netbeheer client -> our server
- [ ] our client -> optional Netbeheer server
- [ ] pinned external vectors
- [ ] fault/reconnect tests
- [ ] performance/latency evidence

Netbeheer remains a test oracle only.

## P8 — Standard evolution

- [ ] JER codec
- [ ] DER codec
- [ ] schema/version negotiation strategy
- [ ] security profile research
- [ ] conformance-test mapping when stable public procedures exist
