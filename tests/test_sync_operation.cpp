/*
 * test_sync_operation.cpp
 */

#include <cassert>
#include <cstdint>
#include <string>

#include <softadastra/core/Core.hpp>
#include <softadastra/store/core/Operation.hpp>
#include <softadastra/store/types/Key.hpp>
#include <softadastra/store/types/OperationType.hpp>
#include <softadastra/store/types/Value.hpp>
#include <softadastra/sync/core/SyncOperation.hpp>
#include <softadastra/sync/types/SyncDirection.hpp>

namespace store_core = softadastra::store::core;
namespace store_types = softadastra::store::types;
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

static store_core::Operation make_put_operation(
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

static store_core::Operation make_delete_operation(
    const std::string &key,
    std::uint64_t timestamp)
{
  return store_core::Operation(
      store_types::OperationType::Delete,
      store_types::Key::from(key),
      store_types::Value{},
      ts(timestamp));
}

static void test_default_sync_operation_is_invalid()
{
  sync_core::SyncOperation op;

  assert(op.sync_id.empty());
  assert(op.origin_node_id.empty());
  assert(op.version == 0);
  assert(!op.timestamp.is_valid());
  assert(op.direction == sync_types::SyncDirection::Unknown);
  assert(!op.is_valid());
  assert(!op.is_local());
  assert(!op.is_remote());
}

static void test_local_sync_operation_is_valid()
{
  auto store_operation = make_put_operation(
      "users/1",
      "alice",
      1709999999);

  sync_core::SyncOperation op(
      "node-a-1",
      "node-a",
      42,
      store_operation,
      ts(1710000000),
      sync_types::SyncDirection::LocalToRemote);

  assert(op.is_valid());
  assert(op.is_local());
  assert(!op.is_remote());

  assert(op.sync_id == "node-a-1");
  assert(op.origin_node_id == "node-a");
  assert(op.version == 42);
  assert(op.timestamp.millis() == 1710000000);

  assert(op.operation.type == store_types::OperationType::Put);
  assert(op.operation.key.value() == "users/1");
  assert(op.operation.value.size() == 5);
  assert(op.operation.value.to_string() == "alice");
  assert(op.operation.timestamp.millis() == 1709999999);
}

static void test_remote_sync_operation_is_valid()
{
  auto store_operation = make_delete_operation(
      "users/9",
      1799999999);

  sync_core::SyncOperation op(
      "node-b-7",
      "node-b",
      77,
      store_operation,
      ts(1800000000),
      sync_types::SyncDirection::RemoteToLocal);

  assert(op.is_valid());
  assert(!op.is_local());
  assert(op.is_remote());

  assert(op.operation.type == store_types::OperationType::Delete);
  assert(op.operation.key.value() == "users/9");
  assert(op.operation.value.empty());
}

static void test_missing_sync_id_is_invalid()
{
  auto store_operation = make_put_operation(
      "users/1",
      "alice",
      1000);

  sync_core::SyncOperation op(
      "",
      "node-a",
      1,
      store_operation,
      ts(2000),
      sync_types::SyncDirection::LocalToRemote);

  assert(!op.is_valid());
}

static void test_missing_origin_node_id_is_invalid()
{
  auto store_operation = make_put_operation(
      "users/1",
      "alice",
      1000);

  sync_core::SyncOperation op(
      "node-a-2",
      "",
      1,
      store_operation,
      ts(2000),
      sync_types::SyncDirection::LocalToRemote);

  assert(!op.is_valid());
}

static void test_unknown_direction_is_invalid()
{
  auto store_operation = make_put_operation(
      "users/1",
      "alice",
      1000);

  sync_core::SyncOperation op(
      "node-a-3",
      "node-a",
      1,
      store_operation,
      ts(2000),
      sync_types::SyncDirection::Unknown);

  assert(!op.is_valid());
}

int main()
{
  test_default_sync_operation_is_invalid();
  test_local_sync_operation_is_valid();
  test_remote_sync_operation_is_valid();
  test_missing_sync_id_is_invalid();
  test_missing_origin_node_id_is_invalid();
  test_unknown_direction_is_invalid();

  return 0;
}
