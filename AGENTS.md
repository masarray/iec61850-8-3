# AGENTS.md — Engineering Contract

This file is the repository-wide implementation contract for humans and coding agents.

The project is not allowed to evolve as a throwaway proof-of-concept. Every change must preserve a path to a production-grade, high-scale IEC 61850-8-3 implementation.

## 1. Non-negotiable rules

### 1.1 No naive production code

Do not merge an implementation merely because it works for the sample IED.

Production paths MUST NOT rely on:

- repeated full-model scans in a hot path;
- unbounded vectors, queues, maps, traces, journals, retries, or request tables;
- per-message or per-point thread creation;
- recursive traversal where model depth or breadth can make stack/resource usage unbounded;
- per-update heap allocation when batching, pooling, arenas, or reusable buffers are practical;
- hidden polling that imitates an event-driven IEC service;
- blocking browser, logger, inspector, or disk I/O on the protocol service path;
- global locks around unrelated sessions or unrelated model partitions;
- ad-hoc string parsing in every request when references can be resolved once into stable IDs;
- exceptions as routine flow control in throughput-critical loops;
- copying complete models or high-cardinality collections to serve one UI request;
- JSON endpoints that require serializing the entire model for ordinary navigation;
- “temporary” architecture that is known to require a rewrite in the next phase.

A deliberately simple implementation is acceptable only when it is outside a production path, explicitly labeled as a test fixture, and cannot silently become the runtime implementation.

### 1.2 Correctness before micro-optimization; architecture before both

Optimization starts at data flow and ownership design, not after the code becomes slow.

Order of work:

1. define canonical semantics and invariants;
2. define ownership and concurrency;
3. define resource bounds and backpressure;
4. define indexes and hot-path complexity;
5. implement with measurable instrumentation;
6. validate protocol correctness;
7. benchmark representative workloads;
8. optimize measured bottlenecks without weakening semantics.

### 1.3 Bounded by design

Every externally influenced resource needs an explicit bound or admission policy:

- BER message size, TLV count, nesting depth;
- WebSocket frame/message size;
- sessions and outstanding invokes;
- worker queues;
- trace buffers;
- report observation buffers;
- BRCB journals;
- report aggregation buckets;
- control-plane page size;
- reconnect/retry state;
- caches.

When the bound is reached, behavior must be deterministic: reject, shed, backpressure, paginate, or evict according to a documented policy.

### 1.4 No invented scale promises

“Millions of signals” is a **design metaphor for high-cardinality, non-laggy operation**, not a mandatory numeric product claim or certification target.

The engineering requirement is structural:

- no avoidable global scans on hot update/report paths;
- bounded memory/queues/journals;
- indexed lookup after model finalization;
- incremental/paged browser APIs;
- backpressure instead of runaway allocation;
- schedulers that process due work instead of scanning idle state;
- observers/UI that cannot freeze protocol work.

Representative benchmarks remain useful as regression tools, but the repository does not need to prove an arbitrary fixed signal count unless a future release explicitly makes a quantitative claim.

If a README/release ever makes a quantitative performance claim, then that claim must be accompanied by reproducible environment/workload details.

## 2. Target architecture

The long-term runtime is data-oriented and headless:

```text
Wire I/O
  |
bounded ingress
  |
decode / validation
  |
typed service dispatch
  |
single-writer or partitioned canonical state
  |
stable IDs + indexed model
  |
event fanout / report engine
  |
bounded egress
```

Browser/UI and diagnostics are observers. They never own protocol lifecycle or model truth.

## 3. High-cardinality design contract

The runtime must be designed so larger engineering models remain responsive and do not freeze the protocol engine or browser.

No fixed point count is required by this contract. The design must preserve a clean path to:

- stable numeric object/signal IDs;
- string interning when cardinality warrants it;
- compact metadata/value separation;
- arena/PMR allocation for large immutable model builds when useful;
- O(1) average or O(log N) indexed reference lookup;
- reverse indexes from signal/member IDs to interested DataSets/RCBs;
- paged/cursor APIs;
- batch mutation;
- bounded queues and journals;
- incremental serialization;
- sharded/partitioned state only if profiling later justifies it.

The important acceptance criterion is architectural: ordinary reads, updates, reporting, scheduling and UI navigation must scale with the work actually affected, not with the entire loaded model whenever avoidable.

## 4. Complexity rules

For code on request/report/update hot paths, PRs must state expected complexity.

Examples:

- signal lookup: O(1) average using an index, not O(N) tree traversal;
- RCB fanout: O(number of interested RCBs), not O(all RCBs);
- DataSet value read: O(number of members);
- trace insertion: O(1);
- bounded journal insertion: amortized O(1);
- cursor page retrieval: O(page size) after indexed positioning.

An O(N) scan is acceptable only when N is intentionally small/bounded or the operation is a cold administrative path. Document that assumption.

## 5. Memory and allocation rules

Prefer:

- value types for small canonical data;
- `std::span` / `std::string_view` for non-owning views with safe lifetime;
- move semantics across ownership boundaries;
- reusable buffers for encode/decode;
- arenas or `std::pmr` for large immutable model builds when introduced;
- interned references and stable IDs instead of duplicating long strings;
- contiguous structures for bulk iteration.

Avoid:

- shared ownership by default;
- one heap allocation per signal update;
- duplicating full reference strings in multiple large indexes;
- retaining wire payloads after canonicalization unless tracing policy requires it.

## 6. Concurrency rules

The protocol/model state must have a documented ownership model.

Current direction:

- I/O callbacks do minimal work;
- protocol/service work is moved to bounded workers;
- canonical mutation is serialized or partitioned deliberately;
- observer paths use copies/snapshots/ring buffers and cannot block the protocol path;
- periodic reporting is scheduled; it never creates one thread per timer/RCB.

If concurrency is changed, include race tests and explain lock ordering or single-writer guarantees.

## 7. Reporting rules

Reporting is event-driven.

Never silently fall back to cyclic `GetDataValues` when an RCB subscription is selected.

The report engine must preserve:

- `RptEna`;
- `TrgOps`;
- `BufTm`;
- `IntgPd`;
- GI command semantics;
- per-member reason-for-inclusion;
- `SqNum`;
- `ConfRev`;
- BRCB EntryID / PurgeBuf / overflow behavior;
- bounded buffering and deterministic replay.

At high cardinality, use reverse subscription indexes. Do not scan every RCB after each point mutation.

## 8. Control-plane and browser rules

The control plane must remain usable with very large models.

Required direction:

- pagination/cursors;
- server-side search/filter;
- incremental tree loading;
- report/trace cursors or push streams;
- bounded response sizes;
- no default “download the whole high-cardinality model as JSON”.

UI polish must not move protocol logic into JavaScript.

## 9. Test hierarchy

Every substantial protocol feature should have, where applicable:

1. pure unit tests;
2. golden BER/vector tests;
3. malformed/fuzz tests;
4. server-core semantic tests;
5. real transport integration tests;
6. reconnect/fault tests;
7. performance regression tests;
8. long-running soak tests.

Linux and Windows behavior must remain equivalent.

## 10. Performance work

Do not optimize from intuition alone.

For performance-sensitive changes:

- establish a baseline;
- use Release builds;
- measure p50/p95/p99 where latency matters;
- record allocation/memory behavior;
- profile CPU before structural tuning;
- compare before/after with identical workloads;
- reject benchmark tricks that remove real protocol work.

Performance regression thresholds and high-cardinality profiles are defined in [docs/SCALABILITY.md](docs/SCALABILITY.md).

## 11. Pull request minimum

A serious PR should answer:

- What invariant is added or changed?
- What is the hot-path complexity?
- What resource is bounded and how?
- What happens under overload?
- What tests prove semantics?
- What interoperability evidence exists?
- Does it add allocations/copies/locks to a hot path?
- Does it preserve the high-cardinality architecture path?
- Does documentation/roadmap need updating?

## 12. Definition of Done

A phase is not complete because the happy-path demo works.

A phase is complete only when its documented exit gate is satisfied, tests are green, limits are explicit, failure behavior is defined, and the implementation does not knowingly require architectural rollback in the next phase.

The authoritative end-to-end plan is [docs/IMPLEMENTATION_PHASES.md](docs/IMPLEMENTATION_PHASES.md).
