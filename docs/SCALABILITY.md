# Scalability and Responsiveness Architecture

## 1. Scope

This project treats phrases such as “millions of data points” as shorthand for a **high-cardinality engineering workload**. It is not a mandatory fixed-number certification target.

The engineering requirement is that larger models remain responsive and predictable:

- the protocol runtime must not freeze because the browser is slow;
- ordinary reads/updates must not repeatedly scan the whole model;
- report fanout must touch interested subscriptions rather than every RCB;
- timers must process due work rather than repeatedly scanning all timers;
- queues, journals, traces and observer feeds must remain bounded;
- browser/model APIs must become incremental as data volume grows;
- overload must create deterministic backpressure, rejection or eviction instead of runaway memory.

Representative generated workloads are used for regression and architecture validation. Their exact size may change as the implementation evolves.

## 2. Workload dimensions

Scale is multi-dimensional and must not be reduced to one point count.

### Model cardinality

Measure a configurable set of generated models:

- small correctness profile;
- medium engineering profile;
- large stress/regression profile;
- optional extreme research profile.

The actual point counts are benchmark configuration, not a product promise.

### DataSet/RCB cardinality

Exercise:

- many DataSets with sparse membership;
- large DataSets;
- many RCBs;
- sparse signal-to-RCB fanout;
- deliberately dense worst-case fanout;
- mixed BRCB/URCB states.

### Session cardinality

Session scale is independent from model size.

Exercise:

- one client;
- multiple concurrent clients;
- reconnect bursts;
- slow clients;
- backpressured clients.

### Update patterns

Exercise:

- isolated dchg;
- same-value dupd;
- qchg;
- batched mutations;
- bursty updates;
- BufTm coalescing;
- GI;
- periodic integrity;
- BRCB replay.

## 3. Mandatory architecture

### 3.1 Stable IDs after model finalization

External references are convenient at boundaries. Internal hot paths should resolve them to stable IDs.

Example direction:

```cpp
using SignalId = std::uint32_t;
using DataSetId = std::uint32_t;
using RcbId = std::uint32_t;
```

After resolution, updates/report fanout should operate on IDs rather than repeatedly splitting reference strings.

### 3.2 Indexed lookup

Build indexes during model finalization:

- reference -> object/signal ID;
- parent -> child ranges;
- DataSet -> member IDs;
- RCB -> DataSet ID;
- signal/member ID -> interested RCB IDs;
- optional engineering-search index.

Target complexity:

| Operation | Architectural target |
|---|---|
| resolved point read | O(1) |
| resolved point update | O(1) before fanout |
| reference lookup | O(1) average or O(log N) |
| affected RCB lookup | O(interested subscriptions) |
| DataSet read | O(member count) |
| BRCB append/evict | amortized O(1) |
| trace/report ring append | O(1) |
| page retrieval | O(page size) |

### 3.3 Metadata/value separation

Topology is mostly immutable after finalization.

Separate:

- names/types/hierarchy/configuration metadata;
- mutable signal values/quality/timestamp;
- DataSet memberships;
- RCB runtime state;
- session state;
- observer buffers.

This makes updates cheap and prevents large metadata copies.

### 3.4 Compact storage

Prefer contiguous tables for high-cardinality runtime entities.

Evaluate, when useful:

- string interning;
- compact reference fragments;
- arenas / `std::pmr` for model construction;
- reusable encode buffers;
- pooled report aggregation buckets.

Do not introduce pooling merely for fashion; keep it where profiling and allocation behavior justify it.

## 4. Report-engine scale model

Desired path:

```text
SignalId update
    |
reverse subscription index
    |
affected RcbIds only
    |
bounded BufTm accumulators
    |
due-time scheduler
    |
DataSet member IDs
    |
report builder / encoder
    |
bounded egress / BRCB journal
```

This replaces correctness-first patterns such as scanning every RCB after each point mutation.

## 5. Timer/scheduler strategy

Do not create one thread per RCB or one timer object that requires global polling.

Preferred direction:

- one scheduler thread or a small bounded set;
- min-heap for moderate due-time cardinality;
- timing wheel only if later profiling justifies it;
- versioned/stale timer entries rather than expensive in-place heap mutation where useful;
- monotonic clock for scheduling;
- batch processing for simultaneously due work.

The scheduler may wake periodically, but it must not cyclically read IEC values to simulate event-driven reporting.

## 6. Buffer and memory policy

Every high-cardinality resource has both a local and, where necessary, global budget.

Examples:

- ingress/service queues;
- outstanding request tables;
- trace ring;
- report observation ring;
- per-RCB BRCB journal;
- global BRCB journal budget;
- BufTm pending groups;
- HTTP response/page size;
- WebSocket message size;
- cached search/index data.

When a limit is reached, behavior must be explicit:

- reject;
- backpressure;
- overwrite oldest observer data;
- evict according to BRCB semantics;
- return pagination/cursor continuation;
- disconnect abusive peers when required.

## 7. Browser/control-plane responsiveness

The Workbench must not request or render the whole model by default when the model is large.

Required direction:

- root/children lazy tree APIs;
- server-side signal search;
- page/cursor parameters;
- DataSet member paging;
- RCB paging;
- report/trace cursor reads or push stream;
- bounded JSON response size;
- cancellation/debounce for search;
- virtualized tables when row counts become large.

A slow browser must consume its own bounded observer path and must never stall DMS service processing.

## 8. Allocation and copy policy

Common-path goals:

- resolve references once and pass IDs internally;
- batch updates;
- reuse report aggregation structures;
- reserve/reuse encoder buffers where useful;
- move wire buffers into transport;
- avoid copying the full model for one HTTP request;
- avoid storing full reference strings redundantly in every relationship table.

Zero allocation on every hot path is not an ideological requirement. Avoidable allocation and copying are.

## 9. Representative regression profiles

Performance tests are engineering tools, not fixed product certifications.

Useful deterministic profiles should cover:

- model build/index finalization;
- random reference lookup;
- ID-based read/update;
- sparse report fanout;
- dense report fanout;
- BufTm burst coalescing;
- integrity scheduler load;
- DataSet reads;
- BRCB append/replay;
- browser page/search;
- connect/disconnect churn.

Each benchmark should emit machine-readable results so regressions can be compared over time.

## 10. What blocks a merge

A change should be redesigned before merge if it introduces, without a documented bounded reason:

- global model scans in a per-point hot path;
- all-RCB scans for each update;
- all-RCB polling for ordinary scheduler ticks;
- unbounded queues/journals/retries;
- whole-model copies for ordinary browser navigation;
- one thread per client/RCB/timer;
- hidden polling as reporting;
- UI/logging/disk waits on the protocol service path;
- repeated parsing of a reference that has already been resolved.

## 11. What may be claimed

Today, the repository may state that its architecture is **designed for high-cardinality responsive operation** and that it uses bounded/indexed/event-driven patterns as those layers are completed.

It should not publish a specific point-count performance claim unless a release explicitly chooses to measure and document that number.

The goal is professional architecture from the beginning, not marketing around an arbitrary count.
