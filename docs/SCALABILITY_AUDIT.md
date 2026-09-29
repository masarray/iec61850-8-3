# Current Scalability Audit

Date: 2026-09-29

This audit exists to prevent the project from confusing a correct laboratory implementation with a high-cardinality, responsive implementation.

The P0–P4 feature set is functionally strong, but several current data paths are intentionally correctness-oriented and must be replaced **before additional feature growth makes them architectural debt**.

## Severity legend

- **S0** — correctness/safety blocker
- **S1** — must be refactored before high-scale work and preferably before P5 grows the same pattern
- **S2** — bounded today but should be improved for large deployments

## 1. Per-update RCB scan — S1

Current behavior:

`ServerCore::enqueue_reports_for_changes` walks logical devices, logical nodes and every RCB, then checks DataSet membership.

Complexity is approximately:

```text
O(all RCBs × membership checks)
```

per mutation.

This is acceptable for the deterministic lab model but fails the high-cardinality design contract.

Required replacement:

```text
SignalId
  |
reverse subscription index
  +--> RcbId
  +--> RcbId
```

Build the reverse index when the model/configuration is finalized or when DataSet/RCB configuration changes.

Target update complexity:

```text
O(number of affected subscriptions)
```

## 2. Integrity scheduler scans all RCBs — S1

Current `poll_scheduled_reports` checks all RCBs from a fixed scheduler tick.

At high RCB cardinality this turns idle time into repeated O(N) work.

Required replacement:

- min-heap for moderate timer counts, or
- hierarchical timing wheel if benchmarked timer cardinality/turnover justifies it.

The runtime must wake for due work, not repeatedly inspect every RCB.

## 3. String/tree lookup — S1

Current model accessors resolve references by:

- parsing strings;
- splitting paths;
- scanning vectors in hierarchy levels.

Examples include data attribute, DataSet and RCB lookup.

Required replacement path:

1. finalize topology;
2. assign stable IDs;
3. build reference -> ID index;
4. use IDs on internal hot paths;
5. retain string lookup only at external/API boundaries.

The final resolved read/update path should not tokenize the same reference repeatedly.

## 4. Whole-model snapshot/copy — S1

Current `ServerRuntime::model_snapshot()` copies an `IedModel`.

That is clean and safe for the small laboratory model but cannot be the normal browser/read strategy for a large model.

Required replacement:

- immutable topology generation/snapshot handle;
- paged query interfaces;
- narrow value pages;
- tree children by stable parent ID;
- server-side filtering/search.

A browser request must not copy the complete model.

## 5. Whole-model JSON endpoint — S1

Current `GET /api/model` serializes the whole model.

This must remain a small-lab convenience endpoint only.

Large-scale API direction:

- `/api/model/root`
- `/api/model/children?parent=<id>&cursor=...`
- `/api/signals?query=...&cursor=...`
- `/api/datasets/<id>/members?cursor=...`
- `/api/rcbs?cursor=...`

Every response must have a bounded maximum page size.

## 6. Observer snapshot copies — S2

Current trace/report snapshot APIs copy their bounded buffers before filtering by cursor.

Memory is bounded, so this is safe, but large observer capacities make it inefficient.

Required evolution:

- cursor-aware ring access;
- copy only matching page;
- optional push stream;
- slow UI cannot retain engine memory.

## 7. BRCB journal budget — S1

Current BRCB journal is bounded per RCB, but a large number of RCBs can multiply the bound into a very large global memory footprint.

Required replacement:

- per-RCB limit;
- global journal memory/entry budget;
- deterministic eviction/overflow semantics;
- memory accounting;
- optional segmented/pool-backed storage;
- benchmark bytes/report and bytes/RCB.

A local bound is not sufficient if the product of all local bounds is unbounded operationally.

## 8. Full reference strings inside high-cardinality relationships — S1

Current typed structures favor readability and correctness, with strings stored in DataSet/report relationships.

At high cardinality, relationship tables should store stable IDs, with names/references interned in metadata.

Required direction:

```text
DataSet -> vector<SignalId>
Rcb     -> DataSetId
Signal  -> compact value slot
```

The wire/UI boundary resolves IDs back to references only when needed.

## 9. Report payload materialization — S2

Reports currently materialize typed values and references before encoding/observation.

This is semantically clean.

Optimization direction after profiling:

- build report plan from member IDs;
- reserve output capacity;
- avoid duplicate intermediate copies;
- keep observer/report recording optional and bounded;
- do not retain full report payload indefinitely.

## 10. Current good foundations

The following are already aligned with the scale contract:

- bounded BER parser limits;
- bounded service-worker queue;
- bounded trace buffer;
- bounded report observation buffer;
- bounded BRCB journal at the individual-RCB level;
- one scheduler thread rather than one thread per RCB;
- browser does not own protocol lifecycle;
- event-driven reporting without hidden data polling;
- request work leaves the I/O callback;
- deterministic server-core ownership.

These should be preserved while high-cardinality internals evolve.

# P4S — Scale Foundation Refactor

P4S is inserted immediately after P4 and before broad P5 client feature growth.

The goal is not to claim a fixed numeric validation target yet. The goal is to remove known algorithms that would force architectural rollback later.

## P4S.1 Stable identity/index layer

- [ ] introduce strong stable ID types;
- [ ] model finalization step;
- [ ] reference -> stable-ID indexes;
- [ ] direct indexed DataSet/RCB lookup;
- [ ] avoid repeated path parsing after resolution.

## P4S.2 Reverse report subscription index

- [ ] signal/member -> RCB reverse index;
- [ ] update only interested RCBs;
- [ ] rebuild/incrementally update index after relevant configuration change;
- [ ] tests proving unrelated RCBs are not visited.

## P4S.3 Scalable timer scheduler

- [ ] replace all-RCB periodic scan;
- [ ] due-time heap or timing wheel;
- [ ] stale timer/version handling;
- [ ] scheduler-lag metric;
- [ ] deterministic virtual-time tests.

## P4S.4 Bounded global journal accounting

- [ ] per-RCB limit retained;
- [ ] global memory/entry budget;
- [ ] deterministic overflow;
- [ ] metrics/high-water mark.

## P4S.5 Incremental engineering API

- [ ] keep current `/api/model` only as small-model compatibility endpoint;
- [ ] add lazy tree endpoints;
- [ ] paged signal search;
- [ ] paged DataSet/RCB views;
- [ ] cursor-aware trace/report ring reads.

## P4S.6 Performance baseline

Before P5:

- [ ] deterministic generated large-model profile;
- [ ] lookup benchmark;
- [ ] mutation benchmark;
- [ ] sparse RCB fanout benchmark;
- [ ] scheduler benchmark;
- [ ] browser/API page benchmark.

The 100k profile is an early architecture regression gate. P9 remains the broader scale/performance hardening phase; no fixed point-count certification is required.

## P4S exit gate

P4S is complete when:

- point updates do not scan all RCBs;
- scheduled reporting does not scan all RCBs each tick;
- resolved hot paths can use indexed IDs;
- ordinary browser navigation is paged/bounded;
- BRCB storage has a global bound;
- a representative generated large model can execute the baseline benchmark suite without architectural pathologies.

Only after this gate should P5 copy/extend these data-access patterns into the native client.
