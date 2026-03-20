# Changelog - softadastra/sync

All notable changes to the **sync module** are documented in this file.

The `sync` module is responsible for **deterministic convergence between peers** in a local-first system.
It orchestrates operations coming from WAL, network, and local state to ensure **eventual consistency under failure**.

---

## [0.1.0] - Initial Sync Engine

### Added

* Core `SyncEngine` orchestrator
* `Operation` abstraction:

  * Logical representation of changes
  * Independent from transport format
* Basic `SyncPlanner`:

  * Detect missing operations
  * Prepare outbound operations
* `SyncApplier`:

  * Apply incoming operations
  * Basic validation
* `PendingQueue`:

  * Track outgoing and incoming operations

### Guarantees

* Operations can be applied in a consistent order
* Basic convergence between two peers
* Integration with WAL for replay

### Design

* No dependency on filesystem internals
* No direct network handling
* Pure orchestration logic

---

## [0.1.1] - Stability Improvements

### Improved

* Idempotent operation application
* Safer handling of duplicate operations
* More consistent sync state tracking

### Fixed

* Edge cases where operations could be applied twice incorrectly
* Missing validation checks for malformed operations

---

## [0.2.0] - Convergence Improvements

### Added

* `Reconciler` component:

  * Detect divergence between peers
  * Apply basic resolution strategy (last-write-wins)
* Version tracking for operations
* Basic state comparison helpers

### Improved

* Deterministic behavior across peers
* Reduced inconsistency during concurrent updates

---

## [0.3.0] - Recovery & Replay

### Added

* Full WAL-driven recovery integration
* Resume sync from last known sequence
* Replay-safe operation application

### Improved

* Faster convergence after restart
* More predictable recovery flow

### Fixed

* Incorrect behavior when replaying partial operation sets
* State inconsistencies after crash recovery

---

## [0.4.0] - Network Interaction Refinement

### Added

* Clear separation between:

  * Planning
  * Application
  * Transport interaction
* Sync request/response patterns (abstract)

### Improved

* Handling of out-of-order operations
* Better tolerance to network delays

---

## [0.5.0] - Robustness Under Failure

### Added

* Duplicate detection mechanisms
* Retry-aware sync logic
* Handling of partially applied states

### Improved

* Stability under intermittent connectivity
* Reduced divergence windows between peers

---

## [0.6.0] - Performance Improvements

### Added

* Batch processing of operations
* Queue optimization

### Improved

* Reduced overhead in sync cycles
* Faster processing of large operation sets

---

## [0.7.0] - Extraction Ready

### Added

* Namespace stabilization (`softadastra::sync`)
* Public API cleanup
* Clear module boundaries

### Improved

* Decoupling from application-specific assumptions
* Prepared for reuse in:

  * Softadastra Sync OS
  * SDK
  * Embedded systems

---

## Roadmap

### Planned

* Multi-peer synchronization
* CRDT-based conflict resolution
* Vector clocks / causal ordering
* Partial sync (delta-based)
* Compression and streaming
* End-to-end encryption support

---

## Philosophy

The sync engine is the **brain of the system**.

> The goal is not to be fast first.
> The goal is to be correct under all conditions.

---

## Rules

* Never assume network order
* Always support replay
* Always be idempotent
* Always converge

---

## Summary

The `sync` module ensures:

* Eventual consistency
* Deterministic convergence
* Recovery after failure
* Correctness under unreliable networks

It is the **core intelligence of Softadastra**.
