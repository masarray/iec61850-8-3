# Implementation Phases — Completion Plan

This is the end-to-end implementation plan from the current native engine to a mature IEC 61850-8-3 platform.

A phase is complete only when its exit gate is met. Checkboxes describe implementation state; exit gates describe engineering completion.

## P0 — Native core foundation

Status: complete foundation.

Scope:

- C++20 baseline;
- canonical value/quality/time primitives;
- bounded workers;
- bounded sessions/traces;
- deterministic tests.

Exit gate:

- no Python/Netbeheer runtime dependency in the native core;
- Windows + Linux CI green.

## P1 — Preliminary BER/DMS profile

Status: foundation complete, hardening continues.

Scope:

- pinned preliminary schema evidence;
- strict bounded BER;
- typed PDU model;
- association lifecycle;
- malformed input tests.

Remaining completion work:

- expand mutation/fuzz corpus;
- keep pinned schema provenance reproducible.

Exit gate:

- covered wire vectors round-trip;
- malformed messages fail safely and deterministically;
- fuzzing finds no known crash/UB in the supported subset.

## P2 — Standalone native server

Status: complete for current read/discovery scope.

Scope:

- native WebSocket;
- active/passive transport independent from IEC role;
- discovery;
- definition/value reads;
- server CLI;
- deterministic sample IED.

Exit gate:

- clean-machine server run without Netbeheer/Python;
- real transport integration tests pass.

## P3 — Headless Web Lab

Status: functional foundation.

Scope:

- HTTP management plane;
- health/readiness;
- model/signal engineering views;
- scenarios;
- trace inspector;
- browser-independent runtime.

Remaining maturation:

- push/event stream;
- release packaging;
- lazy/paged APIs before million-scale certification.

Exit gate:

- browser can be closed/reloaded without changing protocol runtime state;
- slow browser cannot block protocol work.

## P4 — DataSets and event-driven reporting

Status: implemented foundation on current development branch.

Scope:

- DataSets;
- BRCB/URCB;
- RptEna;
- GI;
- TrgOps;
- dchg/qchg/dupd;
- BufTm;
- integrity;
- SqNum/ConfRev;
- EntryID/PurgeBuf/BufOvfl;
- BRCB replay;
- Reports workspace;
- no hidden cyclic polling fallback.

Maturation before release:

- fault/reconnect matrix;
- larger RCB/DataSet stress tests;
- fuzz/malformed report-control messages;
- explicit scheduler/backpressure metrics.

Exit gate:

- change/GI/integrity reaches client as real unconfirmed report;
- reason-for-inclusion is correct;
- bounded buffering under overload;
- reconnect/replay behavior deterministic.

## P4S — Immediate scale-foundation refactor

Goal: remove known correctness-first algorithms that would become expensive architectural debt if P5 grew on top of them.

Current findings and exact targets are documented in [SCALABILITY_AUDIT.md](SCALABILITY_AUDIT.md).

Work:

- stable IDs and model finalization indexes;
- indexed external-reference resolution;
- reverse point/member -> RCB subscription index;
- due-time integrity scheduler rather than all-RCB tick scanning;
- global BRCB storage budget;
- paged/lazy engineering APIs;
- cursor-aware trace/report reads;
- deterministic 100k-signal architecture benchmark.

Exit gate:

- ordinary point update does not inspect every RCB;
- timer tick does not inspect every RCB;
- browser navigation does not require copying/serializing the complete model;
- high-cardinality buffers have both local and global budgets;
- 100k benchmark profile establishes a regression baseline.

This phase happens before broad P5 feature work so client/server implementations share scalable patterns instead of duplicating small-model assumptions.

## P5 — Native reference client

Goal: create an independent client implementation in this repository.

Work:

- headless client CLI;
- client session state machine;
- invoke-ID allocator and bounded outstanding table;
- association/release/reconnect;
- server/LD/LN/DO/DA discovery;
- canonical client-side model builder;
- GetDataDefinition/GetDataValues;
- DataSet directory/values;
- RCB discovery/configuration;
- GI;
- live unconfirmed report decode;
- reason/SqNum/ConfRev validation;
- browser Client workspace;
- correlated request/response/report inspector.

Professional implementation constraints:

- no linear scan of outstanding requests;
- no UI-owned association;
- bounded retry/reconnect;
- cancellation/timeouts;
- incremental discovery;
- model construction designed for future stable IDs.

Exit gate:

- native client and native server interoperate over real WebSocket;
- automated loopback covers discovery/read/DataSet/reporting/reconnect;
- no shared in-process shortcut can mask wire incompatibility.

## P6 — ARStack61850 integration

Goal: make ARStack a second independent IEC 61850-8-3 client.

Work:

- define `IDmsClientAdapter`-style boundary;
- map ARStack canonical semantics to DMS without importing Web UI code;
- association;
- discovery/model build;
- reads;
- DataSets;
- RCB/reporting;
- reconnect/fault behavior;
- parity tests against the standalone native server.

Exit gate:

- ARStack discovers and subscribes to this repo's server over the wire;
- results agree with native reference client for the same server/model;
- no Netbeheer process is required.

## P7 — External interoperability and resilience

Goal: prove behavior against independent implementations and failures.

Work:

- Netbeheer client -> native server, where compatible;
- native client -> Netbeheer server, where compatible;
- pinned external captures/vectors;
- connection loss;
- malformed frames;
- duplicate/late responses;
- sequence gaps;
- buffer overflow/replay;
- server restart;
- client restart;
- high latency/backpressure;
- long-running soak.

Exit gate:

- interoperability matrix published;
- known incompatibilities documented with traces;
- no critical crash/deadlock/leak in soak/fault suite.

## P8 — Standard evolution / multi-codec readiness

Goal: follow the evolving IEC 61850-8-3 specification without coupling canonical semantics to one preliminary encoding.

Work:

- formal profile/version capability model;
- DER codec if required by the finalized profile;
- JER codec if required;
- codec negotiation/version strategy;
- migration vectors between preliminary and later revisions;
- update security/conformance mapping as public standards stabilize.

Exit gate:

- canonical service/model layers remain encoding-independent;
- at least two mappings can coexist without duplicated application semantics when required.

## P9 — Million-point architecture and performance certification

Goal: prove large-model capability rather than merely designing for it.

Work:

- stable numeric IDs;
- interned strings;
- compact metadata/value separation;
- reference index;
- reverse signal -> DataSet/RCB subscription index;
- paged/lazy control-plane API;
- efficient high-cardinality scheduler;
- pooled/reused buffers;
- benchmark model generator;
- deterministic benchmark runner;
- CPU/memory profiles;
- 1,000,000-signal test profile;
- stress profiles for DataSets/RCBs/sessions;
- 24h soak;
- regression budgets.

See [SCALABILITY.md](SCALABILITY.md).

Exit gate:

- million-point L profile passes published benchmark criteria;
- hot-path complexity matches documented targets;
- browser remains responsive through paging/lazy load;
- memory/latency/throughput evidence is published.

Only after this gate may releases claim validated million-point capability.

## P10 — Security and operational hardening

Goal: move from engineering lab quality toward deployable infrastructure.

Work:

- threat model;
- authentication/authorization for management plane where deployment requires it;
- TLS/security-profile mapping when standardized/required;
- secure defaults;
- secret handling;
- rate limits/admission control;
- audit events;
- fuzz campaigns;
- ASan/UBSan/TSan-compatible suites where platform support allows;
- dependency/SBOM workflow;
- reproducible release inputs.

Exit gate:

- documented threat model;
- no known high-severity issue in supported configuration;
- deployment modes clearly separate lab-only vs secured operation.

## P11 — Packaging, service operation, and release engineering

Goal: make the software easy to run and maintain.

Work:

- Windows/Linux standalone artifacts;
- versioned config;
- service/daemon mode;
- clean shutdown;
- structured logs;
- metrics endpoint/export;
- crash diagnostics;
- upgrade compatibility policy;
- release notes;
- deterministic sample profiles;
- artifact provenance/SBOM.

Exit gate:

- a fresh user can install/run/diagnose the engine without source checkout;
- upgrades do not silently invalidate persistent configuration.

## P12 — Conformance-readiness and stable 1.0

Goal: reach a stable production contract when the IEC 61850-8-3 standard and test procedures are sufficiently stable.

Work:

- clause/service implementation matrix;
- conformance statement/profile;
- positive/negative test mapping;
- DNV or other recognized test-lab preparation if applicable and desired;
- documented unsupported optional services;
- API compatibility policy;
- deprecation process;
- performance certification report;
- interoperability report;
- security report.

Exit gate for 1.0:

- supported profile explicitly defined;
- all mandatory supported services pass the project test matrix;
- production performance/scalability gates pass;
- interoperability evidence published;
- release artifacts reproducible;
- no “experimental shortcut” remains on supported production paths.

## Cross-phase rule

Do not postpone architectural correctness to P9.

P9 is where million-scale capability is **proven and tuned**, not where avoidable O(N) scans, unbounded queues, UI-owned state, or per-update allocation are first recognized.

Every preceding phase must preserve the scale path defined in [../AGENTS.md](../AGENTS.md) and [ENGINEERING_STANDARD.md](ENGINEERING_STANDARD.md).
