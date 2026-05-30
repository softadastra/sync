/*
 * test_applier.cpp
 */

#include <cassert>
#include <cstdint>
#include <string>

#include <softadastra/core/Core.hpp>
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

namespace core_time = softadastra::core::time;

static core_time::Timestamp ts(std::uint64_t millis)
{
  return core_time::Timestamp::from_millis(millis);
}

static store_types::Value make_value(const std::string &text)
{
  return store_types::Value::from_string(text);
}

static store_core::Operation make_store_operation(
    const std::string &key,
    std::uint64_t timestamp,
    store_types::OperationType type,
    const std::string &value = "")
{
  return store_core::Operation(
      type,
      store_types::Key::from(key),
      type == store_types::OperationType::Put
          ? make_value(value)
          : store_types::Value{},
      ts(timestamp));
}

static sync_core::SyncOperation make_remote_op(
    const std::string &sync_id,
    const std::string &origin,
    const std::string &key,
    std::uint64_t timestamp,
    store_types::OperationType type,
    const std::string &value = "")
{
  auto operation = make_store_operation(
      key,
      timestamp,
      type,
      value);

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
    sync_types::ConflictPolicy policy,
    const std::string &node_id = "node-a")
{
  static sync_core::SyncConfig config;

  config.node_id = node_id;
  config.conflict_policy = policy;

  return sync_core::SyncContext(store, config);
}

static void test_apply_remote_put_on_empty_store()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);

  auto ctx = make_context(store, sync_types::ConflictPolicy::LastWriteWins);
  sync_applier::RemoteApplier applier(ctx);

  auto sync_op = make_remote_op(
      "op-1",
      "node-b",
      "k1",
      1000,
      store_types::OperationType::Put,
      "hello");

  assert(sync_op.is_valid());

  auto result = applier.apply_remote(sync_op);
  assert(result.is_ok());
  assert(result.value().success);
  assert(result.value().applied);
  assert(!result.value().ignored);

  const auto entry = store.get(store_types::Key::from("k1"));
  assert(entry.has_value());
  assert(entry->value.size() == 5);
  assert(entry->value.to_string() == "hello");
  assert(entry->timestamp.millis() == 1000);
}

static void test_remote_newer_overwrites_local()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);

  auto local = make_store_operation(
      "k1",
      1000,
      store_types::OperationType::Put,
      "old");

  auto applied_local = store.apply_operation(local);
  assert(applied_local.is_ok());
  assert(applied_local.value().success);

  auto ctx = make_context(store, sync_types::ConflictPolicy::LastWriteWins);
  sync_applier::RemoteApplier applier(ctx);

  auto sync_op = make_remote_op(
      "op-2",
      "node-b",
      "k1",
      5000,
      store_types::OperationType::Put,
      "new");

  assert(sync_op.is_valid());

  auto result = applier.apply_remote(sync_op);
  assert(result.is_ok());
  assert(result.value().success);
  assert(result.value().applied);

  const auto entry = store.get(store_types::Key::from("k1"));
  assert(entry.has_value());
  assert(entry->timestamp.millis() == 5000);
  assert(entry->value.to_string() == "new");
}

static void test_remote_older_is_ignored()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);

  auto local = make_store_operation(
      "k1",
      4000,
      store_types::OperationType::Put,
      "local");

  auto applied_local = store.apply_operation(local);
  assert(applied_local.is_ok());
  assert(applied_local.value().success);

  auto ctx = make_context(store, sync_types::ConflictPolicy::LastWriteWins);
  sync_applier::RemoteApplier applier(ctx);

  auto sync_op = make_remote_op(
      "op-3",
      "node-b",
      "k1",
      2000,
      store_types::OperationType::Put,
      "remote");

  assert(sync_op.is_valid());

  auto result = applier.apply_remote(sync_op);
  assert(result.is_ok());
  assert(result.value().success);
  assert(result.value().ignored);

  const auto entry = store.get(store_types::Key::from("k1"));
  assert(entry.has_value());
  assert(entry->timestamp.millis() == 4000);
  assert(entry->value.to_string() == "local");
}

static void test_equal_timestamp_tie_break_remote_wins()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);

  auto local = make_store_operation(
      "k1",
      3000,
      store_types::OperationType::Put,
      "local");

  auto applied_local = store.apply_operation(local);
  assert(applied_local.is_ok());
  assert(applied_local.value().success);

  auto ctx = make_context(
      store,
      sync_types::ConflictPolicy::LastWriteWins,
      "node-a");

  sync_applier::RemoteApplier applier(ctx);

  auto sync_op = make_remote_op(
      "op-4",
      "node-b",
      "k1",
      3000,
      store_types::OperationType::Put,
      "remote");

  assert(sync_op.is_valid());

  auto result = applier.apply_remote(sync_op);
  assert(result.is_ok());
  assert(result.value().applied);

  const auto entry = store.get(store_types::Key::from("k1"));
  assert(entry.has_value());
  assert(entry->timestamp.millis() == 3000);
  assert(entry->value.to_string() == "remote");
}

static void test_keep_local_policy_blocks_remote()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);

  auto local = make_store_operation(
      "k1",
      5000,
      store_types::OperationType::Put,
      "local");

  auto applied_local = store.apply_operation(local);
  assert(applied_local.is_ok());
  assert(applied_local.value().success);

  auto ctx = make_context(store, sync_types::ConflictPolicy::KeepLocal);
  sync_applier::RemoteApplier applier(ctx);

  auto sync_op = make_remote_op(
      "op-5",
      "node-b",
      "k1",
      6000,
      store_types::OperationType::Put,
      "remote");

  assert(sync_op.is_valid());

  auto result = applier.apply_remote(sync_op);
  assert(result.is_ok());
  assert(result.value().ignored);

  const auto entry = store.get(store_types::Key::from("k1"));
  assert(entry.has_value());
  assert(entry->timestamp.millis() == 5000);
  assert(entry->value.to_string() == "local");
}

static void test_delete_operation_applied()
{
  store_core::StoreConfig cfg;
  cfg.enable_wal = false;

  store_engine::StoreEngine store(cfg);

  auto local = make_store_operation(
      "k1",
      1000,
      store_types::OperationType::Put,
      "value");

  auto applied_local = store.apply_operation(local);
  assert(applied_local.is_ok());
  assert(applied_local.value().success);

  auto remote_delete = make_remote_op(
      "op-delete-1",
      "node-b",
      "k1",
      5000,
      store_types::OperationType::Delete);

  assert(remote_delete.is_valid());

  auto ctx = make_context(store, sync_types::ConflictPolicy::LastWriteWins);
  sync_applier::RemoteApplier applier(ctx);

  auto result = applier.apply_remote(remote_delete);
  assert(result.is_ok());
  assert(result.value().success);
  assert(result.value().applied);

  const auto entry = store.get(store_types::Key::from("k1"));
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
