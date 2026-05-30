/*
 * test_recovery.cpp
 */

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>

#include <softadastra/core/Core.hpp>
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
namespace core_time = softadastra::core::time;

static core_time::Timestamp ts(std::uint64_t millis)
{
  return core_time::Timestamp::from_millis(millis);
}

static core_time::Duration duration(std::uint64_t millis)
{
  return core_time::Duration::from_millis(millis);
}

static store_types::Value make_value(const std::string &text)
{
  return store_types::Value::from_string(text);
}

static store_core::Operation make_put(
    const std::string &key,
    const std::string &value,
    std::uint64_t timestamp)
{
  return store_core::Operation(
      store_types::OperationType::Put,
      store_types::Key::from(key),
      make_value(value),
      ts(timestamp));
}

static store_core::Operation make_delete(
    const std::string &key,
    std::uint64_t timestamp)
{
  return store_core::Operation(
      store_types::OperationType::Delete,
      store_types::Key::from(key),
      store_types::Value{},
      ts(timestamp));
}

static sync_core::SyncOperation make_remote_put(
    const std::string &sync_id,
    const std::string &origin,
    const std::string &key,
    const std::string &value,
    std::uint64_t timestamp)
{
  auto operation = store_core::Operation(
      store_types::OperationType::Put,
      store_types::Key::from(key),
      make_value(value),
      ts(timestamp));

  return sync_core::SyncOperation(
      sync_id,
      origin,
      1,
      operation,
      ts(timestamp + 100),
      sync_types::SyncDirection::RemoteToLocal);
}

static sync_core::SyncContext make_context(
    store_engine::StoreEngine &store,
    const std::string &node_id = "node-a")
{
  static sync_core::SyncConfig config;

  config.node_id = node_id;
  config.batch_size = 10;
  config.require_ack = true;
  config.auto_queue = true;
  config.ack_timeout = duration(2000);
  config.retry_interval = duration(1000);
  config.max_retries = 3;

  return sync_core::SyncContext(store, config);
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

    assert(r1.is_ok());
    assert(r2.is_ok());
    assert(r3.is_ok());
    assert(r1.value().success);
    assert(r2.value().success);
    assert(r3.value().success);
  }

  {
    store_core::StoreConfig cfg;
    cfg.enable_wal = true;
    cfg.wal_path = wal_path;
    cfg.auto_flush = true;

    store_engine::StoreEngine recovered(cfg);

    const auto k1 = recovered.get(store_types::Key::from("k1"));
    const auto k2 = recovered.get(store_types::Key::from("k2"));

    assert(!k1.has_value());
    assert(k2.has_value());
    assert(k2->timestamp.millis() == 2000);
    assert(k2->value.size() == 4);
    assert(k2->value.to_string() == "beta");
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

    const auto remote =
        make_remote_put("remote-1", "node-b", "rk1", "payload", 4000);

    const auto result = engine.receive_remote_operation(remote);

    assert(result.is_ok());
    assert(result.value().success);
    assert(result.value().applied);

    const auto entry = store.get(store_types::Key::from("rk1"));
    assert(entry.has_value());
    assert(entry->timestamp.millis() == 4000);
    assert(entry->value.to_string() == "payload");
  }

  {
    store_core::StoreConfig cfg;
    cfg.enable_wal = true;
    cfg.wal_path = wal_path;
    cfg.auto_flush = true;

    store_engine::StoreEngine recovered(cfg);

    const auto entry = recovered.get(store_types::Key::from("rk1"));
    assert(entry.has_value());
    assert(entry->timestamp.millis() == 4000);
    assert(entry->value.size() == 7);
    assert(entry->value.to_string() == "payload");
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

    auto submitted = engine.submit_local_operation(
        make_put("k1", "v1", 5000));

    assert(submitted.is_ok());
    assert(submitted.value().is_valid());

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

    const auto entry = store.get(store_types::Key::from("k1"));
    assert(entry.has_value());
    assert(entry->timestamp.millis() == 5000);
    assert(entry->value.to_string() == "v1");

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

    auto submitted = engine.submit_local_operation(
        make_put("k1", "v1", 6000));

    assert(submitted.is_ok());

    auto batch = engine.next_batch();
    assert(batch.size() == 1);

    // Simulate crash before ack.
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

    const auto entry = store.get(store_types::Key::from("k1"));
    assert(entry.has_value());
    assert(entry->timestamp.millis() == 6000);
    assert(entry->value.to_string() == "v1");
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
