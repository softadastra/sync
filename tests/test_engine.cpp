/*
 * test_engine.cpp
 */

#include <cassert>
#include <string>
#include <vector>

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

static store_types::Value make_value(const std::string &text)
{
  store_types::Value value;
  value.data.assign(text.begin(), text.end());
  return value;
}

static sync_core::SyncContext make_context(store_engine::StoreEngine &store)
{
  static sync_core::SyncConfig config;

  config.node_id = "node-a";
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

static store_core::Operation make_local_op(
    const std::string &key,
    const std::string &value,
    std::uint64_t ts)
{
  store_core::Operation op;
  op.type = store_types::OperationType::Put;
  op.key = store_types::Key{key};
  op.value = make_value(value);
  op.timestamp = ts;
  return op;
}

static sync_core::SyncOperation make_remote_sync_op(
    const std::string &id,
    const std::string &origin,
    const std::string &key,
    std::uint64_t ts)
{
  sync_core::SyncOperation op;
  op.sync_id = id;
  op.origin_node_id = origin;
  op.version = 0;
  op.timestamp = ts + 100;
  op.direction = sync_types::SyncDirection::RemoteToLocal;

  op.op.type = store_types::OperationType::Put;
  op.op.key = store_types::Key{key};
  op.op.value = make_value("remote");
  op.op.timestamp = ts;

  return op;
}

static void test_submit_local_operation_creates_outbox_entry()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);
  auto ctx = make_context(store);
  sync_engine::SyncEngine engine(ctx);

  auto op = make_local_op("k1", "v1", 1000);

  auto sync_op = engine.submit_local_operation(op);

  assert(sync_op.valid());
  assert(sync_op.direction == sync_types::SyncDirection::LocalToRemote);
  assert(sync_op.origin_node_id == "node-a");

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

  engine.submit_local_operation(make_local_op("k1", "v1", 1000));

  auto batch = engine.next_batch();

  assert(batch.size() == 1);
  assert(batch[0].operation.sync_id.size() > 0);

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

  auto sync_op = engine.submit_local_operation(make_local_op("k1", "v1", 1000));

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

  auto result = engine.receive_remote_operation(remote);

  assert(result.applied);

  const auto entry = store.get(store_types::Key{"k1"});
  assert(entry.has_value());
  assert(entry->timestamp == 2000);
}

static void test_retry_expired_requeues_operations()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);
  auto ctx = make_context(store);
  sync_engine::SyncEngine engine(ctx);

  engine.submit_local_operation(make_local_op("k1", "v1", 1000));

  auto batch = engine.next_batch();
  assert(batch.size() == 1);

  const std::string id = batch[0].operation.sync_id;

  const std::size_t requeued = engine.retry_expired();

  assert(requeued == 0); // not expired yet
  assert(engine.ack_tracker().contains(id));
}

static void test_prune_completed_removes_applied_entries()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);
  auto ctx = make_context(store);
  sync_engine::SyncEngine engine(ctx);

  auto sync_op = engine.submit_local_operation(make_local_op("k1", "v1", 1000));

  auto batch = engine.next_batch();
  const std::string id = batch[0].operation.sync_id;

  engine.receive_ack(id);

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

  engine.submit_local_operation(make_local_op("k1", "v1", 1000));

  const auto &state1 = engine.state();
  assert(state1.outbox_size == 1);

  engine.next_batch();

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
