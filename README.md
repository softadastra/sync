# softadastra/sync

> Local-first synchronization pipeline for Softadastra.

`softadastra/sync` is the orchestration layer that moves durable local operations between nodes.

It is built on top of:

- `softadastra/core`
- `softadastra/store`
- `softadastra/wal`

The core rule is:

> Persist locally first. Sync later.

## Purpose

The sync module coordinates local-first synchronization.

It helps Softadastra:

- submit local operations
- queue operations for transport
- track acknowledgements
- retry timed-out operations
- apply remote operations
- resolve conflicts deterministically
- expose observable sync state

The module does not implement network transport directly.

Transport is provided by higher-level modules or adapters.

## What this module does

`softadastra/sync` provides:

- sync operation metadata
- sync envelopes
- outbox management
- deterministic send queue
- acknowledgement tracking
- remote operation application
- conflict resolution policies
- manual scheduler
- sync engine orchestration

## What this module does not do

This module does not implement:

- peer discovery
- sockets
- HTTP transport
- P2P transport
- encryption
- distributed consensus
- long-term persistence of outbox state

The sync module prepares and tracks operations.

Another layer sends them.

## Design Principles

### Local-first

Local operations are applied to the local store first.

The store provides WAL-backed durability before sync.

### Transport-agnostic

`SyncEngine` returns batches of `SyncEnvelope`.

The caller decides how to send them.

### Deterministic

Queues are ordered by:

```
version
timestamp
sync_id
```

### Observable

SyncState exposes queue, outbox, ack, retry, and progress counters.

### Simple

The public flow is intentionally small:

```
submit_local_operation()
next_batch()
receive_ack()
receive_remote_operation()
retry_expired()
```

## Module Structure

```
include/softadastra/sync/
├── ack/
│   └── AckTracker.hpp
├── applier/
│   └── RemoteApplier.hpp
├── conflict/
│   └── ConflictResolver.hpp
├── core/
│   ├── SyncConfig.hpp
│   ├── SyncContext.hpp
│   ├── SyncEnvelope.hpp
│   ├── SyncOperation.hpp
│   └── SyncState.hpp
├── engine/
│   └── SyncEngine.hpp
├── outbox/
│   ├── Outbox.hpp
│   └── OutboxEntry.hpp
├── queue/
│   └── SyncQueue.hpp
├── scheduler/
│   └── SyncScheduler.hpp
├── types/
│   ├── AckStatus.hpp
│   ├── ConflictPolicy.hpp
│   ├── SyncDirection.hpp
│   └── SyncStatus.hpp
├── utils/
│   └── SyncIdGenerator.hpp
└── Sync.hpp
```

## Installation

```
vix add @softadastra/sync
```

## Main Header

```cpp
#include <softadastra/sync/Sync.hpp>
#include <softadastra/store/Store.hpp>
```

## Core Concepts

### SyncOperation

```cpp
auto sync_op = sync::core::SyncOperation::local(
    "node-a-1",
    "node-a",
    1,
    store_operation);
```

### SyncEnvelope

```cpp
sync::core::SyncEnvelope envelope{sync_op};
envelope.mark_queued();
envelope.mark_in_flight(true);
```

### Outbox

```cpp
sync::outbox::Outbox outbox;
outbox.push(sync::outbox::OutboxEntry{envelope});
```

### SyncQueue

```cpp
sync::queue::SyncQueue queue;
queue.push(envelope);
auto next = queue.pop();
```

### AckTracker

```cpp
tracker.track("node-a-1", core::time::Duration::from_seconds(10));
tracker.ack("node-a-1");
```

### SyncEngine

Main orchestration API.

## Basic Usage

```cpp
#include <softadastra/store/Store.hpp>
#include <softadastra/sync/Sync.hpp>

using namespace softadastra;

int main()
{
  store::engine::StoreEngine store{
      store::core::StoreConfig::durable("data/store.wal")};

  auto config =
      sync::core::SyncConfig::durable("node-a");

  sync::core::SyncContext context{store, config};

  sync::engine::SyncEngine engine{context};

  auto operation = store::core::Operation::put(
      store::types::Key{"user:1"},
      store::types::Value::from_string("Gaspard"));

  engine.submit_local_operation(operation);

  auto batch = engine.next_batch();

  return 0;
}
```

## Configuration

### Durable

```cpp
auto config = sync::core::SyncConfig::durable("node-a");
```

### Fast

```cpp
auto config = sync::core::SyncConfig::fast("node-a");
```

## Submit Local Operation

```cpp
auto submitted = engine.submit_local_operation(operation);
```

## Get Next Batch

```cpp
auto batch = engine.next_batch();
```

## Receive Ack

```cpp
engine.receive_ack("node-a-1");
```

## Apply Remote Operation

```cpp
engine.receive_remote_operation(remote_sync_operation);
```

## Conflict Resolution

Policies:

- LastWriteWins
- KeepLocal
- KeepRemote
- Manual

## Retry

```cpp
engine.retry_expired();
```

## Scheduler

```cpp
sync::scheduler::SyncScheduler scheduler{engine};
auto tick = scheduler.tick(true);
```

## Sync State

```cpp
const auto &state = engine.state();
```

## Transport Integration

The sync module does not send data itself.

## Error Handling

```cpp
if (submitted.is_err())
{
  const auto &error = submitted.error();
}
```

## Recommended Flow

Local:
1. Build operation
2. Submit
3. WAL persist
4. Queue
5. Send
6. Ack

Remote:
1. Receive
2. Validate
3. Resolve conflict
4. Apply
5. Persist

## Production Notes

Use durable config and external transport.

## Design Rules

- Persist locally first
- Keep transport outside
- Deterministic behavior
- Stable sync ids
- Retry deterministically

## Dependencies

- softadastra/core
- softadastra/store
- softadastra/wal

## Roadmap

- Persistent outbox
- Transport adapters
- Backoff
- Metrics
- Tracing

## Summary

softadastra/sync is the local-first sync orchestration layer.

It moves durable operations between nodes without embedding transport.

