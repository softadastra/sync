/*
 * test_applier.cpp
 */

#include <cassert>
#include <string>

#include <softadastra/store/core/Operation.hpp>
#include <softadastra/store/core/StoreConfig.hpp>
#include <softadastra/store/engine/StoreEngine.hpp>
#include <softadastra/store/types/Key.hpp>
#include <softadastra/store/types/OperationType.hpp>
#include <softadastra/store/types/Value.hpp>
#include <softadastra/sync/applier/RemoteApplier.hpp>
#include <softadastra/sync/core/SyncConfig.hpp>
#include <softadastra/sync/core/SyncContext.hpp>
#include <softadastra/sync/core/SyncOperation.hpp>
#include <softadastra/sync/types/ConflictPolicy.hpp>
#include <softadastra/sync/types/SyncDirection.hpp>

namespace store_core = softadastra::store::core;
namespace store_engine = softadastra::store::engine;
namespace store_types = softadastra::store::types;

namespace sync_applier = softadastra::sync::applier;
namespace sync_core = softadastra::sync::core;
namespace sync_types = softadastra::sync::types;

static store_types::Value make_value(const std::string &text)
{
  store_types::Value value;
  value.data.assign(text.begin(), text.end());
  return value;
}

static sync_core::SyncOperation make_remote_op(
    const std::string &sync_id,
    const std::string &origin,
    const std::string &key,
    std::uint64_t timestamp,
    store_types::OperationType type,
    const std::string &value = "")
{
  sync_core::SyncOperation sync_op;
  sync_op.sync_id = sync_id;
  sync_op.origin_node_id = origin;
  sync_op.version = 0;
  sync_op.timestamp = timestamp + 100;
  sync_op.direction = sync_types::SyncDirection::RemoteToLocal;

  sync_op.op.type = type;
  sync_op.op.key = store_types::Key{key};
  sync_op.op.timestamp = timestamp;

  if (type == store_types::OperationType::Put)
  {
    sync_op.op.value = make_value(value);
  }

  return sync_op;
}

static sync_core::SyncContext make_context(store_engine::StoreEngine &store,
                                           sync_types::ConflictPolicy policy,
                                           const std::string &node_id = "node-a")
{
  static sync_core::SyncConfig config;

  config.node_id = node_id;
  config.conflict_policy = policy;

  sync_core::SyncContext ctx;
  ctx.store = &store;
  ctx.config = &config;

  return ctx;
}

static void test_apply_remote_put_on_empty_store()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);

  auto ctx = make_context(store, sync_types::ConflictPolicy::LastWriteWins);
  sync_applier::RemoteApplier applier(ctx);

  auto sync_op = make_remote_op("op-1", "node-b", "k1", 1000,
                                store_types::OperationType::Put,
                                "hello");

  auto result = applier.apply_remote(sync_op);

  assert(result.success);
  assert(result.applied);
  assert(!result.ignored);

  const auto entry = store.get(store_types::Key{"k1"});
  assert(entry.has_value());
  assert(entry->value.size() == 5);
  assert(entry->timestamp == 1000);
}

static void test_remote_newer_overwrites_local()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);

  store_core::Operation local;
  local.type = store_types::OperationType::Put;
  local.key = store_types::Key{"k1"};
  local.value = make_value("old");
  local.timestamp = 1000;
  store.apply_operation(local);

  auto ctx = make_context(store, sync_types::ConflictPolicy::LastWriteWins);
  sync_applier::RemoteApplier applier(ctx);

  auto sync_op = make_remote_op("op-2", "node-b", "k1", 5000,
                                store_types::OperationType::Put,
                                "new");

  auto result = applier.apply_remote(sync_op);

  assert(result.success);
  assert(result.applied);

  const auto entry = store.get(store_types::Key{"k1"});
  assert(entry.has_value());
  assert(entry->timestamp == 5000);
}

static void test_remote_older_is_ignored()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);

  store_core::Operation local;
  local.type = store_types::OperationType::Put;
  local.key = store_types::Key{"k1"};
  local.value = make_value("local");
  local.timestamp = 4000;
  store.apply_operation(local);

  auto ctx = make_context(store, sync_types::ConflictPolicy::LastWriteWins);
  sync_applier::RemoteApplier applier(ctx);

  auto sync_op = make_remote_op("op-3", "node-b", "k1", 2000,
                                store_types::OperationType::Put,
                                "remote");

  auto result = applier.apply_remote(sync_op);

  assert(result.success);
  assert(result.ignored);

  const auto entry = store.get(store_types::Key{"k1"});
  assert(entry.has_value());
  assert(entry->timestamp == 4000);
}

static void test_equal_timestamp_tie_break_remote_wins()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);

  store_core::Operation local;
  local.type = store_types::OperationType::Put;
  local.key = store_types::Key{"k1"};
  local.value = make_value("local");
  local.timestamp = 3000;
  store.apply_operation(local);

  auto ctx = make_context(store, sync_types::ConflictPolicy::LastWriteWins, "node-a");
  sync_applier::RemoteApplier applier(ctx);

  auto sync_op = make_remote_op("op-4", "node-b", "k1", 3000,
                                store_types::OperationType::Put,
                                "remote");

  auto result = applier.apply_remote(sync_op);

  assert(result.applied);

  const auto entry = store.get(store_types::Key{"k1"});
  assert(entry.has_value());
  assert(entry->timestamp == 3000);
}

static void test_keep_local_policy_blocks_remote()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);

  store_core::Operation local;
  local.type = store_types::OperationType::Put;
  local.key = store_types::Key{"k1"};
  local.value = make_value("local");
  local.timestamp = 5000;
  store.apply_operation(local);

  auto ctx = make_context(store, sync_types::ConflictPolicy::KeepLocal);
  sync_applier::RemoteApplier applier(ctx);

  auto sync_op = make_remote_op("op-5", "node-b", "k1", 6000,
                                store_types::OperationType::Put,
                                "remote");

  auto result = applier.apply_remote(sync_op);

  assert(result.ignored);

  const auto entry = store.get(store_types::Key{"k1"});
  assert(entry.has_value());
  assert(entry->timestamp == 5000);
}

static void test_delete_operation_applied()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);

  store_core::Operation local;
  local.type = store_types::OperationType::Put;
  local.key = store_types::Key{"k1"};
  local.value = make_value("value");
  local.timestamp = 1000;
  store.apply_operation(local);

  auto ctx = make_context(store, sync_types::ConflictPolicy::LastWriteWins);
  sync_applier::RemoteApplier applier(ctx);

  sync_core::SyncOperation remote_delete;
  remote_delete.sync_id = "op-delete-1";
  remote_delete.origin_node_id = "node-b";
  remote_delete.version = 2;
  remote_delete.timestamp = 5000;
  remote_delete.direction = sync_types::SyncDirection::RemoteToLocal;

  remote_delete.op.type = store_types::OperationType::Delete;
  remote_delete.op.key = store_types::Key{"k1"};
  remote_delete.op.timestamp = 5000;

  auto result = applier.apply_remote(remote_delete);

  assert(result.success);
  assert(result.applied);

  const auto entry = store.get(store_types::Key{"k1"});
  assert(!entry.has_value());
}

int main()
{
  test_apply_remote_put_on_empty_store();
  test_remote_newer_overwrites_local();
  test_remote_older_is_ignored();
  test_equal_timestamp_tie_break_remote_wins();
  test_keep_local_policy_blocks_remote();
  test_delete_operation_applied();

  return 0;
}
