/**
 *
 *  @file Sync.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Softadastra.
 *  All rights reserved.
 *  https://github.com/softadastra/softadastra
 *
 *  Licensed under the Apache License, Version 2.0.
 *
 *  Softadastra Sync
 *
 */

#ifndef SOFTADASTRA_SYNC_HPP
#define SOFTADASTRA_SYNC_HPP

/**
 * @brief Main public aggregator header for the Softadastra Sync module.
 *
 * Include this file when you want access to the complete Sync public API.
 *
 * @code
 * #include <softadastra/sync/Sync.hpp>
 *
 * using namespace softadastra;
 *
 * int main()
 * {
 *   auto config = sync::core::SyncConfig::durable("node-a");
 *
 *   // Create StoreEngine separately, then build SyncContext.
 *   // sync::core::SyncContext context{store, config};
 *   // sync::engine::SyncEngine engine{context};
 *
 *   return 0;
 * }
 * @endcode
 */

/* Ack */
#include <softadastra/sync/ack/AckTracker.hpp>

/* Applier */
#include <softadastra/sync/applier/RemoteApplier.hpp>

/* Conflict */
#include <softadastra/sync/conflict/ConflictResolver.hpp>

/* Core */
#include <softadastra/sync/core/SyncConfig.hpp>
#include <softadastra/sync/core/SyncContext.hpp>
#include <softadastra/sync/core/SyncEnvelope.hpp>
#include <softadastra/sync/core/SyncOperation.hpp>
#include <softadastra/sync/core/SyncState.hpp>

/* Engine */
#include <softadastra/sync/engine/SyncEngine.hpp>

/* Outbox */
#include <softadastra/sync/outbox/Outbox.hpp>
#include <softadastra/sync/outbox/OutboxEntry.hpp>

/* Queue */
#include <softadastra/sync/queue/SyncQueue.hpp>

/* Scheduler */
#include <softadastra/sync/scheduler/SyncScheduler.hpp>

/* Types */
#include <softadastra/sync/types/AckStatus.hpp>
#include <softadastra/sync/types/ConflictPolicy.hpp>
#include <softadastra/sync/types/SyncDirection.hpp>
#include <softadastra/sync/types/SyncStatus.hpp>

/* Utils */
#include <softadastra/sync/utils/SyncIdGenerator.hpp>

#endif // SOFTADASTRA_SYNC_HPP
