# Sync Guide

The Softadastra Sync module provides a local-first synchronization pipeline.

It moves durable local operations between nodes without owning the transport layer.

The core rule is:

> Persist locally first. Sync later.

## Why Softadastra needs Sync

`softadastra/wal` guarantees that operations are durable.

`softadastra/store` turns those durable operations into local state.

`softadastra/sync` moves those operations between nodes.

This makes it possible to build systems that continue working when:

- the network is unstable
- devices go offline
- operations must be retried
- acknowledgements arrive late
- peers reconnect later
- local state must remain usable before sync completes

The sync module does not replace the store.

It coordinates how store operations are propagated.

## What Sync guarantees

The Sync module helps guarantee:

- local operations are applied before being propagated
- sync operations carry stable metadata
- outbound operations are stored in an outbox
- send order is deterministic
- acknowledgement state can be tracked
- timed-out operations can be retried
- remote operations can be applied locally
- conflicts can be resolved deterministically

Sync does not guarantee network delivery by itself.

Transport is handled by another layer.

## What Sync does not do

The Sync module does not implement:

- sockets
- HTTP transport
- WebSocket transport
- P2P discovery
- encryption
- distributed consensus
- peer authentication
- persistent outbox storage

It prepares, tracks, retries, and applies sync operations.

Another layer sends them.

## Installation

```bash
vix add @softadastra/sync
```

## Main header

Use the public aggregator:

```cpp
#include <softadastra/sync/Sync.hpp>
```

For store integration:

```cpp
#include <softadastra/store/Store.hpp>
```

## Main concepts

The Sync module is built around these concepts:

SyncConfig
SyncContext
SyncOperation
SyncEnvelope
Outbox
SyncQueue
AckTracker
RemoteApplier
SyncEngine
SyncScheduler

## SyncConfig

SyncConfig controls the sync engine behavior.

```cpp
auto config =
    sync::core::SyncConfig::durable("node-a");
```

Durable defaults:

batch_size      = 64
max_retries     = 5
retry_interval  = 5 seconds
ack_timeout     = 10 seconds
conflict_policy = LastWriteWins
auto_queue      = true
require_ack     = true

Fast config for tests:

```cpp
auto config =
    sync::core::SyncConfig::fast("node-a");
```

You can customize it:

config.batch_size = 128;
config.max_retries = 3;
config.retry_interval = core::time::Duration::from_seconds(2);
config.ack_timeout = core::time::Duration::from_seconds(5);
config.require_ack = true;

## SyncContext

SyncContext connects Sync to the local store.

```cpp
store::engine::StoreEngine store{
    store::core::StoreConfig::durable("data/store.wal")};

auto config =
    sync::core::SyncConfig::durable("node-a");

sync::core::SyncContext context{store, config};

if (!context.is_valid())
{
  return 1;
}
```

The context does not own the store or config.

The caller must keep them alive while the sync engine uses them.

## SyncOperation

SyncOperation wraps a store operation with sync metadata.

It contains:

sync_id
origin_node_id
version
operation
timestamp
direction

Create a local operation:

```cpp
auto sync_operation =
    sync::core::SyncOperation::local(
        "node-a-1",
        "node-a",
        1,
        store_operation);
```

Create a remote operation:

```cpp
auto sync_operation =
    sync::core::SyncOperation::remote(
        "node-b-1",
        "node-b",
        1,
        store_operation);
```

Check direction:

```cpp
if (sync_operation.is_local()) {}
if (sync_operation.is_remote()) {}
```

## SyncEnvelope

SyncEnvelope wraps a SyncOperation with runtime pipeline state.

It tracks:

status
ack_status
retry_count
last_attempt_at
next_retry_at

```cpp
sync::core::SyncEnvelope envelope{sync_operation};

envelope.mark_queued();
envelope.mark_in_flight(true);
```

Helpers:

envelope.awaiting_ack();
envelope.acknowledged();
envelope.retryable();
envelope.ready_to_send();
envelope.is_valid();

## Outbox

```cpp
sync::outbox::Outbox outbox;

outbox.push(
    sync::outbox::OutboxEntry{envelope});
```

Find:

```cpp
auto entry = outbox.find("node-a-1");
```

Batch:

```cpp
auto batch = outbox.next_batch(
    core::time::Timestamp::now(),
    64);
```

Lifecycle:

outbox.mark_queued("node-a-1");
outbox.mark_in_flight("node-a-1", true);
outbox.mark_acked("node-a-1");
outbox.mark_applied("node-a-1");

## SyncQueue

Sorted by:

version
timestamp
sync_id

```cpp
sync::queue::SyncQueue queue;

queue.push(envelope);
auto next = queue.pop();
```

## AckTracker

```cpp
tracker.track("node-a-1", core::time::Duration::from_seconds(10));
tracker.ack("node-a-1");
```

Expired:

```cpp
auto expired = tracker.collect_expired();
```

## ConflictResolver

```cpp
auto resolution =
    sync::conflict::ConflictResolver::resolve(
        local_entry,
        remote_operation,
        sync::types::ConflictPolicy::LastWriteWins,
        "node-a",
        "node-b");
```

## RemoteApplier

```cpp
sync::applier::RemoteApplier applier{context};
auto result = applier.apply_remote(remote_sync_operation);
```

## SyncEngine

Main orchestration API.

```cpp
sync::engine::SyncEngine engine{context};
```

## Flow

Local:

1. Store apply
2. WAL persist
3. Create SyncOperation
4. Outbox
5. Queue
6. Send
7. Ack

Remote:

1. Receive
2. Validate
3. Resolve
4. Apply
5. Persist

## Summary

softadastra/sync is the local-first synchronization orchestration layer.

Its job:

move durable operations between nodes without embedding transport.

