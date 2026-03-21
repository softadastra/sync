/*
 * test_recovery.cpp
 */

#include <cassert>
#include <cstdio>
#include <string>

#include <softadastra/store/core/Operation.hpp>
#include <softadastra/store/core/StoreConfig.hpp>
#include <softadastra/store/engine/StoreEngine.hpp>
#include <softadastra/store/types/Key.hpp>
#include <softadastra/store/types/OperationType.hpp>
#include <softadastra/store/types/Value.hpp>
#include <softadastra/sync/core/SyncConfig.hpp>
#include <softadastra/sync/core/SyncContext.hpp>
#include <softadastra/sync/core/SyncOperation.hpp>
#include <softadastra/sync/engine/SyncEngine.hpp>
#include <softadastra/sync/scheduler/SyncScheduler.hpp>
#include <softadastra/sync/types/SyncDirection.hpp>

namespace store_core = softadastra::store::core;
namespace store_engine = softadastra::store::engine;
namespace store_types = softadastra::store::types;
namespace sync_core = softadastra::sync::core;
namespace sync_engine = softadastra::sync::engine;
namespace sync_scheduler = softadastra::sync::scheduler;
namespace sync_types = softadastra::sync::types;

static store_types::Value make_value(const std::string &text)
{
  store_types::Value value;
  value.data.assign(text.begin(), text.end());
  return value;
}

static store_core::Operation make_put(
    const std::string &key,
    const std::string &value,
    std::uint64_t timestamp)
{
  store_core::Operation op;
  op.type = store_types::OperationType::Put;
  op.key = store_types::Key{key};
  op.value = make_value(value);
  op.timestamp = timestamp;
  return op;
}

static store_core::Operation make_delete(
    const std::string &key,
    std::uint64_t timestamp)
{
  store_core::Operation op;
  op.type = store_types::OperationType::Delete;
  op.key = store_types::Key{key};
  op.timestamp = timestamp;
  return op;
}

static sync_core::SyncOperation make_remote_put(
    const std::string &sync_id,
    const std::string &origin,
    const std::string &key,
    const std::string &value,
    std::uint64_t timestamp)
{
  sync_core::SyncOperation sync_op;
  sync_op.sync_id = sync_id;
  sync_op.origin_node_id = origin;
  sync_op.version = 0;
  sync_op.timestamp = timestamp + 100;
  sync_op.direction = sync_types::SyncDirection::RemoteToLocal;

  sync_op.op.type = store_types::OperationType::Put;
  sync_op.op.key = store_types::Key{key};
  sync_op.op.value = make_value(value);
  sync_op.op.timestamp = timestamp;

  return sync_op;
}

static sync_core::SyncContext make_context(store_engine::StoreEngine &store,
                                           const std::string &node_id = "node-a")
{
  static sync_core::SyncConfig config;

  config.node_id = node_id;
  config.batch_size = 10;
  config.require_ack = true;
  config.auto_queue = true;
  config.ack_timeout_ms = 2000;
  config.retry_interval_ms = 1000;
  config.max_retries = 3;

  sync_core::SyncContext ctx;
  ctx.store = &store;
  ctx.config = &config;

  return ctx;
}

static void test_store_recovers_local_operations_from_wal()
{
  const std::string wal_path = "test_sync_recovery_local.wal";
  std::remove(wal_path.c_str());

  {
    store_core::StoreConfig cfg;
    cfg.enable_wal = true;
    cfg.wal_path = wal_path;
    cfg.auto_flush = true;

    store_engine::StoreEngine store(cfg);

    const auto r1 = store.apply_operation(make_put("k1", "alpha", 1000));
    const auto r2 = store.apply_operation(make_put("k2", "beta", 2000));
    const auto r3 = store.apply_operation(make_delete("k1", 3000));

    assert(r1.success);
    assert(r2.success);
    assert(r3.success);
  }

  {
    store_core::StoreConfig cfg;
    cfg.enable_wal = true;
    cfg.wal_path = wal_path;
    cfg.auto_flush = true;

    store_engine::StoreEngine recovered(cfg);

    const auto k1 = recovered.get(store_types::Key{"k1"});
    const auto k2 = recovered.get(store_types::Key{"k2"});

    assert(!k1.has_value());
    assert(k2.has_value());
    assert(k2->timestamp == 2000);
    assert(k2->value.size() == 4);
  }

  std::remove(wal_path.c_str());
}

static void test_store_recovers_remote_applied_operation_from_wal()
{
  const std::string wal_path = "test_sync_recovery_remote.wal";
  std::remove(wal_path.c_str());

  {
    store_core::StoreConfig cfg;
    cfg.enable_wal = true;
    cfg.wal_path = wal_path;
    cfg.auto_flush = true;

    store_engine::StoreEngine store(cfg);
    auto ctx = make_context(store, "node-a");
    sync_engine::SyncEngine engine(ctx);

    const auto remote = make_remote_put("remote-1", "node-b", "rk1", "payload", 4000);
    const auto result = engine.receive_remote_operation(remote);

    assert(result.success);
    assert(result.applied);

    const auto entry = store.get(store_types::Key{"rk1"});
    assert(entry.has_value());
    assert(entry->timestamp == 4000);
  }

  {
    store_core::StoreConfig cfg;
    cfg.enable_wal = true;
    cfg.wal_path = wal_path;
    cfg.auto_flush = true;

    store_engine::StoreEngine recovered(cfg);

    const auto entry = recovered.get(store_types::Key{"rk1"});
    assert(entry.has_value());
    assert(entry->timestamp == 4000);
    assert(entry->value.size() == 7);
  }

  std::remove(wal_path.c_str());
}

static void test_outbox_is_not_recovered_after_restart_in_v1()
{
  const std::string wal_path = "test_sync_recovery_outbox.wal";
  std::remove(wal_path.c_str());

  {
    store_core::StoreConfig cfg;
    cfg.enable_wal = true;
    cfg.wal_path = wal_path;
    cfg.auto_flush = true;

    store_engine::StoreEngine store(cfg);
    auto ctx = make_context(store, "node-a");
    sync_engine::SyncEngine engine(ctx);

    auto submitted = engine.submit_local_operation(make_put("k1", "v1", 5000));
    (void)submitted;

    assert(engine.outbox().size() == 1);
    assert(engine.state().outbox_size == 1);
  }

  {
    store_core::StoreConfig cfg;
    cfg.enable_wal = true;
    cfg.wal_path = wal_path;
    cfg.auto_flush = true;

    store_engine::StoreEngine store(cfg);
    auto ctx = make_context(store, "node-a");
    sync_engine::SyncEngine engine(ctx);

    const auto entry = store.get(store_types::Key{"k1"});
    assert(entry.has_value());
    assert(entry->timestamp == 5000);

    // V1 behavior: outbox lives only in memory and is empty after restart.
    assert(engine.outbox().size() == 0);
    assert(engine.queue().size() == 0);
    assert(engine.ack_tracker().size() == 0);
  }

  std::remove(wal_path.c_str());
}

static void test_scheduler_after_restart_sees_no_pending_sync_entries_in_v1()
{
  const std::string wal_path = "test_sync_recovery_scheduler.wal";
  std::remove(wal_path.c_str());

  {
    store_core::StoreConfig cfg;
    cfg.enable_wal = true;
    cfg.wal_path = wal_path;
    cfg.auto_flush = true;

    store_engine::StoreEngine store(cfg);
    auto ctx = make_context(store, "node-a");
    sync_engine::SyncEngine engine(ctx);

    engine.submit_local_operation(make_put("k1", "v1", 6000));
    auto batch = engine.next_batch();
    assert(batch.size() == 1);

    // Simulate crash before ack
  }

  {
    store_core::StoreConfig cfg;
    cfg.enable_wal = true;
    cfg.wal_path = wal_path;
    cfg.auto_flush = true;

    store_engine::StoreEngine store(cfg);
    auto ctx = make_context(store, "node-a");
    sync_engine::SyncEngine engine(ctx);
    sync_scheduler::SyncScheduler scheduler(engine);

    const auto tick = scheduler.tick(true);

    // V1 has no persistent sync outbox/ack recovery yet.
    assert(tick.retried_count == 0);
    assert(tick.pruned_count == 0);
    assert(tick.batch.empty());

    const auto entry = store.get(store_types::Key{"k1"});
    assert(entry.has_value());
    assert(entry->timestamp == 6000);
  }

  std::remove(wal_path.c_str());
}

int main()
{
  test_store_recovers_local_operations_from_wal();
  test_store_recovers_remote_applied_operation_from_wal();
  test_outbox_is_not_recovered_after_restart_in_v1();
  test_scheduler_after_restart_sees_no_pending_sync_entries_in_v1();

  return 0;
}
