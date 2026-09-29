# Engineering Standard

This document defines how the native IEC 61850-8-3 engine is designed, implemented, reviewed, optimized, and released.

It complements [../AGENTS.md](../AGENTS.md), which is the mandatory repository-wide coding contract.

## 1. Engineering objectives

The implementation must simultaneously optimize for:

1. protocol correctness;
2. deterministic behavior;
3. bounded resource usage;
4. high throughput and low tail latency;
5. large model scalability;
6. observability;
7. portability across Windows and Linux;
8. maintainability;
9. interoperability evidence;
10. security and robustness.

A feature that improves one dimension by silently sacrificing protocol semantics or boundedness is not an acceptable optimization.

## 2. Layering

Dependencies flow downward:

```text
Applications / Workbench / CLI
        |
Control plane / orchestration
        |
Client + Server service engines
        |
Canonical IEC 61850 domain
        |
Session / report / request state
        |
Codec
        |
Transport
```

Rules:

- transport code cannot own IEC model semantics;
- codec code cannot mutate application state;
- browser code cannot become protocol authority;
- test-oracle integrations cannot become runtime dependencies;
- ARStack integration will use a narrow native adapter, not the Web UI.

## 3. Canonical model strategy

### Current phase

The current typed tree is intentionally clear and strongly typed.

### Scale phase

For million-point workloads, migrate without changing external semantics toward:

- immutable topology after model build;
- compact stable IDs: `LdId`, `LnId`, `DoId`, `DaId`, `DataSetId`, `RcbId`;
- an intern table for repeated names/reference fragments;
- contiguous node tables instead of pointer-heavy ownership graphs where profiling justifies it;
- separate mutable value storage from mostly immutable metadata;
- reference-to-ID index built once;
- ID-to-node/value direct lookup;
- reverse membership indexes.

This avoids forcing every read/update through repeated string splitting and tree scans.

## 4. State ownership

Prefer a single-writer canonical mutation model until benchmarks prove it insufficient.

Benefits:

- deterministic ordering;
- simple `SqNum` and report causality;
- no distributed lock graph;
- reproducible tests.

Read-heavy consumers should use safe snapshots, immutable metadata, atomically published generations, or other measured strategies.

If scale requires partitioning, partition by stable ownership boundaries such as logical device or model shard, and preserve deterministic cross-shard event ordering where the protocol requires it.

## 5. Hot-path rules

### Decode

- validate framing/limits before deep parsing;
- bounded depth/child counts;
- reuse input buffers when lifetime allows;
- avoid materializing fields that are not needed by the selected service;
- never accept ambiguous/non-minimal BER encodings merely for convenience.

### Service dispatch

- typed dispatch;
- indexed object lookup;
- bounded outstanding requests;
- no blocking filesystem/UI/network operation except the intended transport response path.

### Model update

- resolve external reference to stable ID once when possible;
- batch mutations;
- publish one coherent generation for atomic scenarios;
- derive only affected reports.

### Report fanout

Long-term:

```text
SignalId
   |
reverse subscription index
   +--> RcbId A
   +--> RcbId B
```

Do not iterate over every RCB for each signal update when model size becomes material.

### Encode

- pre-size/reuse buffers where practical;
- move buffers into transport;
- avoid encode -> copy -> copy -> send chains;
- cap maximum report/message size and define segmentation before unbounded growth.

## 6. Error handling

Errors are classified:

- malformed peer input;
- unsupported service/profile;
- invalid state transition;
- capacity/backpressure;
- timeout;
- transport disconnect;
- internal invariant failure.

Peer errors must not crash the process.

Internal invariant failures should be highly visible in test/debug builds and converted to controlled failure boundaries in production paths where possible.

## 7. Backpressure

Every asynchronous boundary needs a full-queue policy.

Examples:

| Boundary | Policy direction |
|---|---|
| RX -> service worker | bounded queue, reject/disconnect on persistent abuse |
| model update ingress | bounded batch queue |
| report egress | bounded per-session queue |
| BRCB journal | bounded ring/segmented journal with overflow indication |
| trace | bounded overwrite ring |
| browser report feed | bounded cursor buffer; slow browser cannot block engine |

Never solve overload by allowing memory to grow until the OS intervenes.

## 8. Scheduling

Do not create one thread per RCB, timer, client, DataSet, or report.

Preferred direction:

- one or a small bounded scheduler set;
- timer heap/timing wheel depending measured cardinality;
- batch expiry processing;
- monotonic clock for scheduling;
- system clock only for protocol/user timestamps.

Periodic integrity scheduling must be independent from cyclic data polling.

## 9. Large response strategy

Large discovery/model responses require:

- continuation/cursor support;
- explicit page size;
- bounded serialization;
- streaming where applicable;
- cancellation;
- no construction of an enormous temporary JSON string for normal browsing.

The browser tree will move to lazy expansion for million-point models.

## 10. Observability

Minimum metrics direction:

- sessions active;
- associations accepted/rejected;
- outstanding requests;
- worker queue depth/high-water mark;
- backpressure drops/rejections;
- RX/TX bytes and messages;
- decode errors by class;
- report rate by reason;
- BufTm pending groups;
- BRCB journal utilization/overflows;
- scheduler lag;
- p50/p95/p99 request latency;
- memory high-water mark.

Metrics collection itself must be cheap and bounded.

## 11. Security and robustness

Even before a final security profile is adopted:

- enforce message limits;
- validate lengths before allocation;
- reject invalid state transitions;
- never trust peer-provided counts;
- cap decompression if compression is ever introduced;
- cap HTTP request/query sizes;
- avoid shelling out from protocol paths;
- do not execute downloaded code;
- fuzz codecs and control-plane parsers;
- run sanitizers where supported;
- document any unauthenticated lab-only management endpoint.

## 12. Portability

The supported baseline is modern C++20.

Avoid platform-specific behavior in canonical/service code.

OS-specific transport/system facilities live behind narrow adapters.

CI must keep at least:

- current Ubuntu;
- current Windows/MSVC.

Add macOS only when there is a concrete supported-user requirement.

## 13. API compatibility

Internal types may evolve rapidly before 1.0, but changes should still be intentional.

Rules:

- distinguish canonical API from UI/control-plane convenience API;
- version public wire/control-plane contracts when breaking evolution begins;
- avoid leaking implementation containers into public interfaces unless intentional;
- use stable IDs rather than raw pointers across async boundaries.

## 14. Benchmark-driven optimization

Performance is a release gate, not a final cleanup task.

Required benchmark families are defined in [SCALABILITY.md](SCALABILITY.md).

A performance-sensitive PR must not degrade an established benchmark materially without explanation and an explicit tradeoff.

## 15. Review checklist

Before merge:

- semantics verified?
- wire vectors updated if needed?
- failure paths tested?
- resource limits explicit?
- queue/journal/cursor bounded?
- hot-path complexity acceptable?
- avoidable copies/allocations removed?
- no accidental O(N) global scan?
- no hidden polling?
- thread/lock ownership clear?
- Linux + Windows green?
- roadmap/docs accurate?
- benchmark impact understood?
