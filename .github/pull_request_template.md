## Summary

Describe the behavior/invariant changed by this PR.

## Architecture / complexity

- Hot-path complexity before:
- Hot-path complexity after:
- New indexes or caches:
- Ownership/concurrency model:
- Why this does not introduce a known million-scale blocker:

## Resource bounds / overload behavior

List every externally influenced resource added or changed.

- Queue/table/journal/cache limit:
- Backpressure/rejection/eviction behavior:
- Maximum message/response impact:
- Slow-client behavior:

## Allocation / copy review

- [ ] No accidental whole-model copy on a hot path
- [ ] No avoidable per-point/per-message heap allocation in steady state
- [ ] No unnecessary wire-buffer copy chain
- [ ] String/reference duplication is bounded or justified

## Protocol correctness

- [ ] Canonical semantics remain outside transport/UI
- [ ] No hidden polling fallback for event-driven services
- [ ] State transitions are validated
- [ ] Malformed peer input fails safely
- [ ] Golden/interoperability vectors updated when relevant

## Scale review

- [ ] No new full-model scan in an ordinary point read/update path
- [ ] No new full-RCB scan in per-point report fanout
- [ ] No one-thread-per-session/RCB/timer design
- [ ] Large collections are paged/cursored/bounded
- [ ] Change preserves the stable-ID/index path in `docs/SCALABILITY.md`

## Tests

- [ ] Unit tests
- [ ] Protocol/vector tests
- [ ] Real transport integration tests
- [ ] Fault/reconnect tests where relevant
- [ ] Linux CI
- [ ] Windows CI
- [ ] Benchmark/performance evidence where hot-path behavior changed

## Documentation

- [ ] Roadmap updated if phase status changed
- [ ] Architecture/engineering docs updated if ownership or complexity changed
- [ ] README performance claims remain evidence-based

## Reviewer gate

This PR follows:

- `AGENTS.md`
- `docs/ENGINEERING_STANDARD.md`
- `docs/SCALABILITY.md`
- `docs/IMPLEMENTATION_PHASES.md`
