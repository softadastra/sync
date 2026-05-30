/*
 * test_engine.cpp
 */

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

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
#include <softadastra/sync/types/SyncDirection.hpp>

namespace store_core = softadastra::store::core;
namespace store_engine = softadastra::store::engine;
namespace store_types = softadastra::store::types;
namespace sync_core = softadastra::sync::core;
namespace sync_engine = softadastra::sync::engine;
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

static sync_core::SyncContext make_context(store_engine::StoreEngine &store)
{
  static sync_core::SyncConfig config;

  config.node_id = "node-a";
  config.batch_size = 10;
  config.require_ack = true;
  config.auto_queue = true;
  config.ack_timeout = duration(2000);
  config.retry_interval = duration(1000);
  config.max_retries = 3;

  return sync_core::SyncContext(store, config);
}

static store_core::Operation make_local_op(
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

static sync_core::SyncOperation make_remote_sync_op(
    const std::string &id,
    const std::string &origin,
    const std::string &key,
    std::uint64_t timestamp)
{
  auto operation = store_core::Operation(
      store_types::OperationType::Put,
      store_types::Key::from(key),
      make_value("remote"),
      ts(timestamp));

  return sync_core::SyncOperation(
      id,
      origin,
      1,
      operation,
      ts(timestamp + 100),
      sync_types::SyncDirection::RemoteToLocal);
}

static void test_submit_local_operation_creates_outbox_entry()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);
  auto ctx = make_context(store);
  sync_engine::SyncEngine engine(ctx);

  auto op = make_local_op("k1", "v1", 1000);

  auto sync_op_result = engine.submit_local_operation(op);
  assert(sync_op_result.is_ok());

  const auto sync_op = sync_op_result.value();

  assert(sync_op.is_valid());
  assert(sync_op.direction == sync_types::SyncDirection::LocalToRemote);
  assert(sync_op.origin_node_id == "node-a");
  assert(sync_op.operation.key.value() == "k1");
  assert(sync_op.operation.value.to_string() == "v1");

  const auto &outbox = engine.outbox();
  assert(outbox.size() == 1);
}

static void test_next_batch_marks_in_flight_and_tracks_ack()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);
  auto ctx = make_context(store);
  sync_engine::SyncEngine engine(ctx);

  auto sync_op_result = engine.submit_local_operation(
      make_local_op("k1", "v1", 1000));

  assert(sync_op_result.is_ok());
  assert(sync_op_result.value().is_valid());

  auto batch = engine.next_batch();

  assert(batch.size() == 1);
  assert(!batch[0].operation.sync_id.empty());

  const auto &ack = engine.ack_tracker();
  assert(ack.size() == 1);
}

static void test_receive_ack_clears_tracker_and_marks_applied()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);
  auto ctx = make_context(store);
  sync_engine::SyncEngine engine(ctx);

  auto sync_op_result = engine.submit_local_operation(
      make_local_op("k1", "v1", 1000));

  assert(sync_op_result.is_ok());

  auto batch = engine.next_batch();
  assert(batch.size() == 1);

  const std::string id = batch[0].operation.sync_id;

  const bool ok = engine.receive_ack(id);
  assert(ok);

  const auto &ack = engine.ack_tracker();
  assert(ack.size() == 0);
}

static void test_receive_remote_operation_applies_to_store()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);
  auto ctx = make_context(store);
  sync_engine::SyncEngine engine(ctx);

  auto remote = make_remote_sync_op("r1", "node-b", "k1", 2000);

  assert(remote.is_valid());

  auto result = engine.receive_remote_operation(remote);
  assert(result.is_ok());
  assert(result.value().applied);

  const auto entry = store.get(store_types::Key::from("k1"));
  assert(entry.has_value());
  assert(entry->timestamp.millis() == 2000);
  assert(entry->value.to_string() == "remote");
}

static void test_retry_expired_requeues_operations()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);
  auto ctx = make_context(store);
  sync_engine::SyncEngine engine(ctx);

  auto sync_op_result = engine.submit_local_operation(
      make_local_op("k1", "v1", 1000));

  assert(sync_op_result.is_ok());

  auto batch = engine.next_batch();
  assert(batch.size() == 1);

  const std::string id = batch[0].operation.sync_id;

  const std::size_t requeued = engine.retry_expired();

  assert(requeued == 0);
  assert(engine.ack_tracker().contains(id));
}

static void test_prune_completed_removes_applied_entries()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);
  auto ctx = make_context(store);
  sync_engine::SyncEngine engine(ctx);

  auto sync_op_result = engine.submit_local_operation(
      make_local_op("k1", "v1", 1000));

  assert(sync_op_result.is_ok());

  auto batch = engine.next_batch();
  assert(batch.size() == 1);

  const std::string id = batch[0].operation.sync_id;

  assert(engine.receive_ack(id));

  const std::size_t removed = engine.prune_completed();

  assert(removed == 1);
  assert(engine.outbox().size() == 0);
}

static void test_state_updates_correctly()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);
  auto ctx = make_context(store);
  sync_engine::SyncEngine engine(ctx);

  auto sync_op_result = engine.submit_local_operation(
      make_local_op("k1", "v1", 1000));

  assert(sync_op_result.is_ok());

  const auto &state1 = engine.state();
  assert(state1.outbox_size == 1);

  auto batch = engine.next_batch();
  assert(batch.size() == 1);

  const auto &state2 = engine.state();
  assert(state2.in_flight_count == 1);
}

int main()
{
  test_submit_local_operation_creates_outbox_entry();
  test_next_batch_marks_in_flight_and_tracks_ack();
  test_receive_ack_clears_tracker_and_marks_applied();
  test_receive_remote_operation_applies_to_store();
  test_retry_expired_requeues_operations();
  test_prune_completed_removes_applied_entries();
  test_state_updates_correctly();

  return 0;
}
