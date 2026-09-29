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
- [x] true event-driven dchg reporting from canonical value changes
- [x] qchg reporting from canonical Quality changes
- [x] dupd reporting for same-value data updates
- [x] BufTm event coalescing with per-member reason preservation
- [x] periodic integrity reporting on the native scheduler
- [x] sequence-number continuity and ConfRev advancement tests
- [x] bounded BRCB journal with EntryID, PurgeBuf and BufOvfl
- [x] BRCB replay by EntryID, including association reconnect
- [x] bounded delivered-report observation API
- [x] engineer-focused browser RCB tree and live Reports workspace
- [x] quality mutation controls for deterministic qchg testing
- [x] report delivery uses the DMS transport path; no hidden cyclic read fallback

Exit: a simulator value/quality change, GI, data update or integrity timer reaches a subscribed client through a real unconfirmed report and is visible in the browser report monitor.

## P4S — Scale foundation before client growth

Detailed audit: [SCALABILITY_AUDIT.md](SCALABILITY_AUDIT.md)

- [ ] stable ID/index layer for resolved model access
- [ ] reverse signal/member -> RCB subscription index
- [ ] remove per-update full-RCB scans
- [ ] replace periodic all-RCB scheduler scan with due-time scheduling
- [ ] global BRCB journal budget in addition to per-RCB bounds
- [ ] paged/lazy model, signal, DataSet and RCB APIs
- [ ] cursor-aware observer ring reads
- [ ] deterministic representative large-model regression profile and baseline

Exit: known high-cardinality blockers are removed before the P5 client duplicates or depends on them. P9 remains the broader performance-hardening phase; no fixed signal-count certification is required.

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


## P9 — High-scale architecture and performance hardening

Detailed plan: [SCALABILITY.md](SCALABILITY.md)

- [ ] stable numeric IDs for high-cardinality entities
- [ ] string interning / compact reference storage
- [ ] immutable metadata separated from mutable value storage
- [ ] reference -> ID indexes
- [ ] reverse signal -> interested RCB index
- [ ] paged/lazy model and signal APIs
- [ ] scalable high-cardinality timer scheduler
- [ ] reusable/pool-backed hot-path buffers where measurement justifies them
- [ ] deterministic large-model generator
- [ ] machine-readable benchmark runner
- [ ] DataSet/RCB/session stress profiles
- [ ] memory/latency/throughput report
- [ ] 24-hour soak and regression budgets

Exit: high-cardinality hot paths are indexed/bounded, browser APIs are incremental, schedulers avoid global scans, and representative regression profiles show no freezing, runaway memory, or structural performance cliff. A numeric scale claim is optional and only published if separately measured.

## P10 — Security and operational hardening

- [ ] threat model
- [ ] management-plane auth strategy for non-lab deployment
- [ ] TLS/security profile mapping when applicable
- [ ] admission control/rate limiting
- [ ] fuzz campaigns and sanitizer suites
- [ ] dependency/SBOM workflow
- [ ] secure lab-vs-production configuration boundary

Exit: deployment assumptions, trust boundaries and supported secure modes are explicit and tested.

## P11 — Packaging and release engineering

- [ ] versioned Windows/Linux packages
- [ ] service/daemon operation
- [ ] structured logs and metrics
- [ ] clean shutdown/restart behavior
- [ ] upgrade/config compatibility policy
- [ ] artifact provenance and SBOM
- [ ] user-facing release documentation

Exit: a fresh engineering workstation can install, run and diagnose the engine without source checkout.

## P12 — Conformance readiness / stable 1.0

- [ ] standard-clause/service support matrix
- [ ] conformance statement/profile
- [ ] positive/negative conformance-test mapping
- [ ] recognized test-lab preparation if applicable
- [ ] unsupported optional-service declaration
- [ ] API compatibility and deprecation policy
- [ ] final interoperability report
- [ ] final performance/scalability report
- [ ] final security report

Exit: supported production profile is explicit, reproducible, interoperable and passes all mandatory project gates.

## Completion authority

[IMPLEMENTATION_PHASES.md](IMPLEMENTATION_PHASES.md) is the detailed phase-completion contract.

[../AGENTS.md](../AGENTS.md) and [ENGINEERING_STANDARD.md](ENGINEERING_STANDARD.md) are mandatory for every phase: no phase may introduce known naive hot paths, unbounded resource growth or architecture that knowingly requires rollback later.
