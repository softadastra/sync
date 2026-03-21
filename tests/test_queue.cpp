/*
 * test_queue.cpp
 */

#include <cassert>
#include <string>
#include <vector>

#include <softadastra/store/core/Operation.hpp>
#include <softadastra/store/types/Key.hpp>
#include <softadastra/store/types/OperationType.hpp>
#include <softadastra/store/types/Value.hpp>
#include <softadastra/sync/core/SyncEnvelope.hpp>
#include <softadastra/sync/core/SyncOperation.hpp>
#include <softadastra/sync/queue/SyncQueue.hpp>
#include <softadastra/sync/types/AckStatus.hpp>
#include <softadastra/sync/types/SyncDirection.hpp>
#include <softadastra/sync/types/SyncStatus.hpp>

namespace store_types = softadastra::store::types;
namespace sync_core = softadastra::sync::core;
namespace sync_queue = softadastra::sync::queue;
namespace sync_types = softadastra::sync::types;

static store_types::Value make_value(const std::string &text)
{
  store_types::Value value;
  value.data.assign(text.begin(), text.end());
  return value;
}

static sync_core::SyncEnvelope make_envelope(
    const std::string &sync_id,
    std::uint64_t version,
    std::uint64_t sync_timestamp,
    const std::string &key_suffix)
{
  sync_core::SyncOperation op;
  op.sync_id = sync_id;
  op.origin_node_id = "node-a";
  op.version = version;
  op.timestamp = sync_timestamp;
  op.direction = sync_types::SyncDirection::LocalToRemote;

  op.op.type = store_types::OperationType::Put;
  op.op.key = store_types::Key{"key-" + key_suffix};
  op.op.value = make_value("value-" + key_suffix);
  op.op.timestamp = sync_timestamp - 10;

  sync_core::SyncEnvelope envelope;
  envelope.operation = op;
  envelope.status = sync_types::SyncStatus::Queued;
  envelope.ack_status = sync_types::AckStatus::None;
  envelope.retry_count = 0;
  envelope.last_attempt_at = 0;
  envelope.next_retry_at = 0;

  return envelope;
}

static void test_default_queue_is_empty()
{
  sync_queue::SyncQueue queue;

  assert(queue.empty());
  assert(queue.size() == 0);
  assert(!queue.front().has_value());
  assert(!queue.pop().has_value());
  assert(queue.peek_batch(10).empty());
  assert(queue.pop_batch(10).empty());
}

static void test_push_orders_by_version()
{
  sync_queue::SyncQueue queue;

  queue.push(make_envelope("node-a-3", 3, 3000, "3"));
  queue.push(make_envelope("node-a-1", 1, 1000, "1"));
  queue.push(make_envelope("node-a-2", 2, 2000, "2"));

  assert(queue.size() == 3);

  const auto front = queue.front();
  assert(front.has_value());
  assert(front->operation.sync_id == "node-a-1");

  const auto entries = queue.entries();
  assert(entries[0].operation.sync_id == "node-a-1");
  assert(entries[1].operation.sync_id == "node-a-2");
  assert(entries[2].operation.sync_id == "node-a-3");
}

static void test_push_orders_by_timestamp_when_version_equal()
{
  sync_queue::SyncQueue queue;

  queue.push(make_envelope("node-a-2", 5, 2000, "2"));
  queue.push(make_envelope("node-a-1", 5, 1000, "1"));
  queue.push(make_envelope("node-a-3", 5, 3000, "3"));

  const auto entries = queue.entries();

  assert(entries.size() == 3);
  assert(entries[0].operation.sync_id == "node-a-1");
  assert(entries[1].operation.sync_id == "node-a-2");
  assert(entries[2].operation.sync_id == "node-a-3");
}

static void test_push_orders_by_sync_id_when_version_and_timestamp_equal()
{
  sync_queue::SyncQueue queue;

  queue.push(make_envelope("node-a-3", 7, 4000, "3"));
  queue.push(make_envelope("node-a-1", 7, 4000, "1"));
  queue.push(make_envelope("node-a-2", 7, 4000, "2"));

  const auto entries = queue.entries();

  assert(entries.size() == 3);
  assert(entries[0].operation.sync_id == "node-a-1");
  assert(entries[1].operation.sync_id == "node-a-2");
  assert(entries[2].operation.sync_id == "node-a-3");
}

static void test_front_does_not_remove_entry()
{
  sync_queue::SyncQueue queue;
  queue.push(make_envelope("node-a-1", 1, 1000, "1"));

  const auto first = queue.front();
  const auto second = queue.front();

  assert(first.has_value());
  assert(second.has_value());
  assert(first->operation.sync_id == "node-a-1");
  assert(second->operation.sync_id == "node-a-1");
  assert(queue.size() == 1);
}

static void test_pop_removes_entry()
{
  sync_queue::SyncQueue queue;
  queue.push(make_envelope("node-a-1", 1, 1000, "1"));
  queue.push(make_envelope("node-a-2", 2, 2000, "2"));

  const auto first = queue.pop();
  assert(first.has_value());
  assert(first->operation.sync_id == "node-a-1");
  assert(queue.size() == 1);

  const auto second = queue.pop();
  assert(second.has_value());
  assert(second->operation.sync_id == "node-a-2");
  assert(queue.empty());

  assert(!queue.pop().has_value());
}

static void test_peek_batch_returns_without_removing()
{
  sync_queue::SyncQueue queue;
  queue.push(make_envelope("node-a-1", 1, 1000, "1"));
  queue.push(make_envelope("node-a-2", 2, 2000, "2"));
  queue.push(make_envelope("node-a-3", 3, 3000, "3"));

  const std::vector<sync_core::SyncEnvelope> batch = queue.peek_batch(2);

  assert(batch.size() == 2);
  assert(batch[0].operation.sync_id == "node-a-1");
  assert(batch[1].operation.sync_id == "node-a-2");
  assert(queue.size() == 3);
}

static void test_pop_batch_removes_entries()
{
  sync_queue::SyncQueue queue;
  queue.push(make_envelope("node-a-1", 1, 1000, "1"));
  queue.push(make_envelope("node-a-2", 2, 2000, "2"));
  queue.push(make_envelope("node-a-3", 3, 3000, "3"));

  const std::vector<sync_core::SyncEnvelope> batch = queue.pop_batch(2);

  assert(batch.size() == 2);
  assert(batch[0].operation.sync_id == "node-a-1");
  assert(batch[1].operation.sync_id == "node-a-2");

  assert(queue.size() == 1);

  const auto remaining = queue.front();
  assert(remaining.has_value());
  assert(remaining->operation.sync_id == "node-a-3");
}

static void test_contains_and_erase()
{
  sync_queue::SyncQueue queue;
  queue.push(make_envelope("node-a-1", 1, 1000, "1"));
  queue.push(make_envelope("node-a-2", 2, 2000, "2"));

  assert(queue.contains("node-a-1"));
  assert(queue.contains("node-a-2"));
  assert(!queue.contains("node-a-3"));

  assert(queue.erase("node-a-1"));
  assert(!queue.contains("node-a-1"));
  assert(queue.contains("node-a-2"));
  assert(queue.size() == 1);

  assert(!queue.erase("node-a-9"));
}

static void test_clear_empties_queue()
{
  sync_queue::SyncQueue queue;
  queue.push(make_envelope("node-a-1", 1, 1000, "1"));
  queue.push(make_envelope("node-a-2", 2, 2000, "2"));

  assert(!queue.empty());

  queue.clear();

  assert(queue.empty());
  assert(queue.size() == 0);
  assert(!queue.front().has_value());
}

int main()
{
  test_default_queue_is_empty();
  test_push_orders_by_version();
  test_push_orders_by_timestamp_when_version_equal();
  test_push_orders_by_sync_id_when_version_and_timestamp_equal();
  test_front_does_not_remove_entry();
  test_pop_removes_entry();
  test_peek_batch_returns_without_removing();
  test_pop_batch_removes_entries();
  test_contains_and_erase();
  test_clear_empties_queue();

  return 0;
}
