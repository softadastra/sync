# softadastra/sync

> Local-first synchronization primitives for reliable Softadastra products.

`softadastra/sync` provides the synchronization orchestration layer of the Softadastra C++ stack.

Softadastra builds reliability-first products for local-first, offline-first, and distributed applications. This module moves durable local operations between nodes without embedding network transport.

## Purpose

`softadastra/sync` exists to coordinate local-first synchronization.

It is used by higher-level SDKs, product APIs, and distributed Softadastra infrastructure.

The core rule is simple:

> Persist locally first. Sync later.

It is designed to be:

- Local-first
- Transport-agnostic
- Deterministic
- Retry-aware
- Observable
- Product-ready

## What it provides

This module provides synchronization primitives such as:

- Sync operations
- Sync envelopes
- Outbox management
- Deterministic send queues
- Acknowledgement tracking
- Remote operation application
- Conflict resolution policies
- Sync engine orchestration
- Observable sync state

## What it does not do

`softadastra/sync` does not contain:

- Peer discovery
- Socket handling
- HTTP transport
- P2P transport
- Encryption
- Distributed consensus
- Product-specific logic

It prepares, tracks, retries, and applies sync operations. Another layer decides how those operations are transported.

## Core model

```txt
Local operation
      |
Persist to local store
      |
Queue for sync
      |
Send through transport
      |
Ack / retry / apply remote
```

The sync module keeps synchronization logic deterministic while leaving transport outside the core engine.

## Where it fits

```txt
Softadastra products
        |
SDKs and product APIs
        |
softadastra/sync
        |
softadastra/store
        |
softadastra/wal
        |
softadastra/core
```

`softadastra/sync` depends on `softadastra/store`, `softadastra/wal`, and `softadastra/core`. It provides synchronization orchestration for higher-level products and adapters.

## Installation

```bash
vix add @softadastra/sync
```

## Usage

```cpp
#include <softadastra/sync/Sync.hpp>
#include <softadastra/store/Store.hpp>
```

## Example

```cpp
#include <softadastra/store/Store.hpp>
#include <softadastra/sync/Sync.hpp>

#include <iostream>

int main()
{
    softadastra::store::engine::StoreEngine store{
        softadastra::store::core::StoreConfig::durable("data/store.wal")
    };

    auto config = softadastra::sync::core::SyncConfig::durable("node-a");

    softadastra::sync::core::SyncContext context{store, config};
    softadastra::sync::engine::SyncEngine sync{context};

    auto operation = softadastra::store::core::Operation::put(
        softadastra::store::types::Key{"user:1"},
        softadastra::store::types::Value::from_string("Gaspard")
    );

    auto submitted = sync.submit_local_operation(operation);

    if (submitted.is_err())
    {
        std::cout << submitted.error().message() << "\n";
        return 1;
    }

    auto batch = sync.next_batch();

    std::cout << "Queued operations: " << batch.size() << "\n";
    return 0;
}
```

## Requirements

- C++20
- `softadastra/core`
- `softadastra/wal`
- `softadastra/store`

## Documentation

For the full documentation, visit [docs.softadastra.com](https://docs.softadastra.com).

## License

Licensed under the Apache License, Version 2.0.
