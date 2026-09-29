# Scalability and Performance Architecture

## 1. Scope

The long-term scalability target is a **million-point class IEC 61850 model**.

For this project, “million-point” means the runtime can load, index, browse, read, update, and selectively report from at least **1,000,000 leaf data attributes/signals** while staying bounded and responsive.

It does not mean one million concurrent WebSocket sessions.

All numbers below are **acceptance targets**, not current performance claims.

## 2. Workload dimensions

Performance must be measured across separate axes.

### Model scale

Profiles:

- S: 10,000 leaf signals
- M: 100,000 leaf signals
- L: 1,000,000 leaf signals
- XL research: 5,000,000 leaf signals

### DataSet/RCB scale

Representative large profile:

- 10,000 DataSets;
- 100,000 total DataSet memberships or more;
- 10,000 RCBs;
- sparse subscription fanout for ordinary points;
- intentionally dense worst-case profile tested separately.

### Session scale

Do not conflate with model scale.

Initial performance profiles:

- 1 client;
- 32 concurrent clients;
- 128 concurrent clients;
- higher counts only after connection/session memory is measured.

### Update scale

Measure:

- isolated writes;
- batches of 100 / 1,000 / 10,000 points;
- repeated same-value updates (dupd);
- quality changes;
- bursty dchg;
- integrity report storms;
- GI across small and large DataSets.

## 3. Required architectural evolution

The current string-rich tree is appropriate for correctness work but is not the final million-point representation.

Before million-scale certification, implement:

### 3.1 Stable IDs

Resolve external object references to compact IDs.

Example:

```cpp
using SignalId = std::uint32_t;
using DataSetId = std::uint32_t;
using RcbId = std::uint32_t;
```

Hot paths should operate on IDs.

### 3.2 String interning

Store repeated names/reference fragments once.

A one-million-point model must not duplicate full path strings in every relationship table.

### 3.3 Split metadata and values

Mostly immutable topology/metadata should be separate from mutable signal values.

This improves:

- cache locality;
- snapshot strategy;
- batch updates;
- memory accounting.

### 3.4 Contiguous storage

Favor compact vectors/tables for high-cardinality entities.

Pointer-heavy object graphs are harder to traverse efficiently and increase allocator overhead.

### 3.5 Build-time indexes

At model-finalization time, build:

- reference -> object/signal ID;
- DataSet -> member IDs;
- signal ID -> interested RCB IDs;
- RCB -> DataSet ID;
- logical hierarchy child ranges;
- optional search index for engineering UI.

### 3.6 Incremental browser API

A million-point model must never require one giant `/api/model` response.

Required APIs:

- tree children by parent ID;
- paged signal query;
- server-side search;
- DataSet members page;
- RCB page;
- report cursor/push channel.

## 4. Complexity targets

| Operation | Target |
|---|---|
| reference lookup | O(1) average or O(log N) |
| point read by ID | O(1) |
| point update by ID | O(1) before subscription fanout |
| affected RCB lookup | O(number of interested RCBs) |
| DataSet read | O(member count) |
| trace insert | O(1) |
| BRCB append/evict | amortized O(1) |
| report cursor advance | O(page size) |
| tree child query | O(number of children returned) |

A global model/RCB scan in the per-point update path fails the million-scale design gate.

## 5. Allocation targets

Once the high-scale storage layer is introduced:

- steady-state point update: zero heap allocations on the common path where practical;
- report aggregation: reuse bounded buckets/buffers;
- encode path: reusable/reservable buffers;
- trace/report observation: fixed-capacity rings;
- model construction: arena/PMR strategy evaluated and benchmarked.

## 6. Benchmark acceptance gates

Benchmarks must publish environment and raw summary data.

### Gate A — model build

For L profile (1,000,000 leaf signals):

- model builds successfully in a bounded process;
- no pathological super-linear growth;
- reference index build completes;
- memory high-water mark recorded;
- no stack overflow from hierarchy construction/traversal.

No hard time/memory number is claimed until the optimized storage implementation lands. The first P9 benchmark run establishes the baseline and then the project locks regression budgets.

### Gate B — lookup

On the L model:

- random lookup latency distribution is recorded for at least 1,000,000 operations;
- p99 must remain stable as N grows from M to L, consistent with indexed complexity;
- no repeated path tokenization/tree walk in the resolved-ID hot path.

### Gate C — mutation throughput

Measure single-point and batched updates:

- dchg;
- dupd;
- qchg.

Publish:

- updates/s;
- p50/p95/p99 enqueue-to-apply latency;
- allocations/update;
- CPU utilization;
- queue high-water mark.

### Gate D — report fanout

Measure:

- sparse subscriptions;
- dense subscriptions;
- BufTm coalescing;
- integrity timer load;
- GI.

The sparse profile must scale with interested subscriptions, not all RCBs.

### Gate E — control plane

With the L model:

- first browser page does not serialize the entire model;
- tree expansion returns bounded pages;
- search is server-side;
- report/trace viewers use cursors or push streaming;
- slow/paused browsers do not increase engine memory without bound.

### Gate F — soak

Minimum production-readiness soak profile:

- 24 hours;
- repeated connect/disconnect;
- mixed reads, reports, scenarios;
- bounded memory after warm-up;
- no monotonic queue/journal growth;
- no lost scheduler liveness;
- no deadlock.

Longer 72-hour soak becomes a release-candidate gate later.

## 7. Regression budgets

After P9 baseline is established, CI/performance infrastructure should maintain separate noise-aware thresholds.

Recommended policy:

- correctness benchmark failure: always fail;
- >10% repeatable throughput regression: investigate/block unless justified;
- >10% repeatable p99 latency regression: investigate/block unless justified;
- >10% repeatable memory increase on the same profile: investigate/block unless justified.

Do not apply noisy microsecond thresholds blindly on shared hosted runners. Stable performance gates may run on controlled hardware.

## 8. Report engine scale plan

Current correctness implementation may iterate structures that are small in the sample model.

Million-scale target:

```text
SignalId update
   |
reverse index
   |
small set of RcbId
   |
per-RCB bounded BufTm accumulator
   |
due queue/timing wheel
   |
report builder using DataSet member IDs
   |
bounded egress / BRCB journal
```

### Integrity scheduler

If RCB count becomes large, use a timing wheel or efficient timer heap rather than checking every RCB at a fixed polling tick.

The scheduler may wake periodically, but it must not poll IEC signal values to implement reporting.

## 9. Memory-budget methodology

Do not guess memory support from `sizeof` alone.

Measure:

- allocator overhead;
- string/index storage;
- model metadata;
- mutable values;
- memberships;
- RCB state;
- session state;
- journals;
- encode/decode buffers;
- traces/observer buffers.

Publish bytes per leaf signal for the standard benchmark model.

The optimization objective is compact enough that one million signals fit comfortably on an engineering workstation with operating margin, not merely “does not crash”.

## 10. Benchmark implementation plan

P9 introduces a dedicated benchmark executable/profile generator, not ad-hoc timing inside unit tests.

It must support deterministic seeds and emit machine-readable results.

Planned commands conceptually:

```text
benchmark model-build --signals 1000000
benchmark lookup --signals 1000000 --ops 1000000
benchmark mutate --signals 1000000 --updates 5000000 --batch 1000
benchmark report --signals 1000000 --rcbs 10000 --fanout sparse
benchmark soak --profile production --hours 24
```

The exact CLI may evolve, but reproducibility is mandatory.

## 11. What may be claimed today

Current milestones prove protocol/runtime behavior on deterministic lab models.

The repository must NOT yet state “validated for one million points” until P9 measurements are green.

The important requirement today is that P0–P8 code follows [../AGENTS.md](../AGENTS.md) and does not knowingly lock the engine into algorithms or ownership patterns that require a complete rewrite to reach P9.
