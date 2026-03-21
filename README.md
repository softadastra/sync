# softadastra/sync

> Deterministic synchronization engine for local-first systems.

The `sync` module is the **brain of Softadastra**.

It is responsible for:

> Converging multiple devices toward the same state, despite failures, disconnections, and unordered events.

---

## Purpose

The goal of `softadastra/sync` is simple:

> Take local operations and remote operations, and ensure all peers converge to the same result.

---

## Core Principle

> Order does not matter. Convergence does.

The system must produce the same final state:

* Regardless of operation order
* Despite network interruptions
* Across different devices

---

## Responsibilities

The `sync` module provides:

* Sync planning (what to send / what to receive)
* Operation comparison
* Conflict handling (basic in MVP)
* Application of remote operations
* Replay coordination with WAL
* Recovery after disconnection

---

## What this module does NOT do

* No filesystem watching (fs module)
* No durability (wal module)
* No network transport (transport module)
* No raw storage (store module)

👉 It orchestrates everything, but owns no low-level concern.

---

## Design Principles

### 1. Deterministic convergence

All peers must reach the same state.

---

### 2. Idempotency

Applying the same operation multiple times must be safe.

---

### 3. Replay-driven

State is reconstructed from operations, not assumptions.

---

### 4. Failure-first

The system must work under:

* Disconnections
* Delays
* Partial updates

## Core Components

### Operation

Represents a logical change.

Examples:

* File created
* File updated
* File deleted

Includes:

* Identifier
* Version / sequence
* Payload

### SyncEngine

The main orchestrator.

Responsible for:

* Driving synchronization cycles
* Coordinating modules
* Managing sync state

### SyncPlanner

Decides:

* What operations are missing
* What needs to be sent
* What needs to be requested

### SyncApplier

Applies incoming operations:

* Validates operations
* Applies changes
* Ensures idempotency

### Reconciler

Handles inconsistencies:

* Resolves divergence
* Ensures convergence
* Applies conflict rules (basic in MVP)

### PendingQueue

Tracks:

* Operations waiting to be sent
* Operations waiting to be applied

## Example Flow

### Local change

1. File changes detected (fs)
2. Operation created
3. Operation written to WAL
4. Operation enters sync queue
5. Planner schedules it for sending
6. Transport sends it

### Remote change

1. Operation received (transport)
2. Sync validates it
3. SyncApplier applies it
4. Store updates file
5. Metadata updated

### Recovery

1. Node restarts
2. WAL is replayed
3. Sync rebuilds state
4. Missing operations are requested
5. System converges

---

## Dependencies

### Internal

* softadastra/core
* softadastra/wal
* softadastra/metadata
* softadastra/transport
* softadastra/store
* softadastra/fs

## Guarantees

The sync module ensures:

* Eventual consistency
* Deterministic convergence
* No lost operations
* Recovery after interruption

## Failure Model

Designed to handle:

* Offline peers
* Network partitions
* Out-of-order messages
* Duplicate operations

## MVP Scope

* 2 peers
* LAN only
* Basic operations
* Simple conflict strategy (last-write-wins)

## Roadmap

* Multi-peer synchronization
* Advanced conflict resolution (CRDT / vector clocks)
* Partial sync (delta transfer)
* Sync over unreliable networks
* Compression and batching
* End-to-end encryption

## Rules

* Never trust ordering from network
* Never apply without validation
* Always support replay
* Always converge

## Philosophy

The sync engine is not about speed.

> It is about correctness under failure.

## Summary

* Orchestrates synchronization
* Ensures convergence
* Handles failures
* Coordinates all modules

## Installation

```bash
vix add @softadastra/sync
vix deps
```

## License

See root LICENSE file.
