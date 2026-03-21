/*
 * test_sync_operation.cpp
 */

#include <cassert>
#include <string>

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

static store_types::Value make_value(const std::string &text)
{
  store_types::Value value;
  value.data.assign(text.begin(), text.end());
  return value;
}

static void test_default_sync_operation_is_invalid()
{
  sync_core::SyncOperation op;

  assert(op.sync_id.empty());
  assert(op.origin_node_id.empty());
  assert(op.version == 0);
  assert(op.timestamp == 0);
  assert(op.direction == sync_types::SyncDirection::Unknown);
  assert(!op.valid());
  assert(!op.is_local());
  assert(!op.is_remote());
}

static void test_local_sync_operation_is_valid()
{
  sync_core::SyncOperation op;
  op.sync_id = "node-a-1";
  op.origin_node_id = "node-a";
  op.version = 42;
  op.timestamp = 1710000000;
  op.direction = sync_types::SyncDirection::LocalToRemote;

  op.op.type = store_types::OperationType::Put;
  op.op.key = store_types::Key{"users/1"};
  op.op.value = make_value("alice");
  op.op.timestamp = 1709999999;

  assert(op.valid());
  assert(op.is_local());
  assert(!op.is_remote());

  assert(op.sync_id == "node-a-1");
  assert(op.origin_node_id == "node-a");
  assert(op.version == 42);
  assert(op.timestamp == 1710000000);

  assert(op.op.type == store_types::OperationType::Put);
  assert(op.op.key.value == "users/1");
  assert(op.op.value.size() == 5);
  assert(op.op.timestamp == 1709999999);
}

static void test_remote_sync_operation_is_valid()
{
  sync_core::SyncOperation op;
  op.sync_id = "node-b-7";
  op.origin_node_id = "node-b";
  op.version = 77;
  op.timestamp = 1800000000;
  op.direction = sync_types::SyncDirection::RemoteToLocal;

  op.op.type = store_types::OperationType::Delete;
  op.op.key = store_types::Key{"users/9"};
  op.op.timestamp = 1799999999;

  assert(op.valid());
  assert(!op.is_local());
  assert(op.is_remote());

  assert(op.op.type == store_types::OperationType::Delete);
  assert(op.op.key.value == "users/9");
  assert(op.op.value.empty());
}

static void test_missing_sync_id_is_invalid()
{
  sync_core::SyncOperation op;
  op.origin_node_id = "node-a";
  op.direction = sync_types::SyncDirection::LocalToRemote;

  assert(!op.valid());
}

static void test_missing_origin_node_id_is_invalid()
{
  sync_core::SyncOperation op;
  op.sync_id = "node-a-2";
  op.direction = sync_types::SyncDirection::LocalToRemote;

  assert(!op.valid());
}

static void test_unknown_direction_is_invalid()
{
  sync_core::SyncOperation op;
  op.sync_id = "node-a-3";
  op.origin_node_id = "node-a";
  op.direction = sync_types::SyncDirection::Unknown;

  assert(!op.valid());
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
