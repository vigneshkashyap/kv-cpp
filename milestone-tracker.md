Design Doc Template

1) Context
What problem this milestone solves and why now.

2) Goals / Non-Goals
Goals:

Non-Goals: (explicitly list what you’re deferring)

3) SLOs / Constraints
Durability, latency targets, data size assumptions, hardware.

4) Interfaces
External API (KVEngine): put/get/del/flush/close semantics.

Module surfaces touched (wal/, recovery/, memtable/, sstable/, compaction/).

5) Data & Layout
Path layout (MANIFEST, wal-.log, sst-.sst).

Any on-disk framing/headers/footers (describe, don’t implement).

6) Invariants (must always hold)
Bullet list of safety/correctness guarantees.

7) Failure & Recovery
Enumerate failure points; for each, expected behavior and recovery order.

8) Concurrency Model
Readers/writers, locks/latches, background workers, admission control.

9) Testing Strategy
Unit, fuzz, crash/recovery matrix, deterministic seeds, acceptance checks.

10) Observability
Metrics you’ll expose for this milestone; what you’ll look at to verify.

11) Alternatives Considered (ADR)
Options, pros/cons, why selected.

12) “Done When”
Crisp pass/fail gates (e.g., “random truncation never replays a partial record”).

13) Open Questions / Risks
What needs a follow-up decision; risks + mitigations.

Milestone Tracker (checklist you can tick)
M0 Foundations: API surface, module boundaries, error model, path layout, ADR-000 “Why LSM over B-Tree for this project.”
M1 WAL: framing, checksum, fsync policy, monotonic seq; truncation/corruption tests; durability invariants.
M2 Recovery & MANIFEST: atomic manifest, boot order, idempotent replay; crash at each step.
M3 Memtable: structure choice, tombstones, flush thresholds; boundary correctness.
M4 SSTable v1 (write): sorted immutable file, footer, light block index; atomic publish.
M5 Read Path: probe order (mem → immutables → SSTs newest→oldest), tombstone masking.
M6 Compaction v1: tiered merge, tombstone GC, crash-safe compaction pipeline.
M7 Filters & Indexes: per-SST Bloom, in-mem block index; FP targets and measurements.
M8 Concurrency & Back-pressure: shared reads, single writer, background compactor, admission.
M9 Observability: metrics, structured logs, read-only CLI tools (dump/scan/check).
M10 Benchmarks: YCSB-ish mixes, dataset scales, perf tables + narrative.