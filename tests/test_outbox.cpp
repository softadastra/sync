/*
 * test_outbox.cpp
 */

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include <softadastra/core/Core.hpp>
#include <softadastra/store/core/Operation.hpp>
#include <softadastra/store/types/Key.hpp>
#include <softadastra/store/types/OperationType.hpp>
#include <softadastra/store/types/Value.hpp>
#include <softadastra/sync/core/SyncEnvelope.hpp>
#include <softadastra/sync/core/SyncOperation.hpp>
#include <softadastra/sync/outbox/Outbox.hpp>
#include <softadastra/sync/outbox/OutboxEntry.hpp>
#include <softadastra/sync/types/AckStatus.hpp>
#include <softadastra/sync/types/SyncDirection.hpp>
#include <softadastra/sync/types/SyncStatus.hpp>

namespace store_core = softadastra::store::core;
namespace store_types = softadastra::store::types;
namespace sync_core = softadastra::sync::core;
namespace sync_outbox = softadastra::sync::outbox;
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

static store_core::Operation make_store_operation(
    const std::string &sync_id,
    std::uint64_t op_timestamp)
{
  return store_core::Operation(
      store_types::OperationType::Put,
      store_types::Key::from("key-" + sync_id),
      make_value("value-" + sync_id),
      ts(op_timestamp));
}

static sync_outbox::OutboxEntry make_entry(
    const std::string &sync_id,
    std::uint64_t version,
    std::uint64_t op_timestamp,
    std::uint64_t next_retry_at = 0)
{
  auto operation = make_store_operation(sync_id, op_timestamp);

  sync_core::SyncOperation sync_operation(
      sync_id,
      "node-a",
      version,
      operation,
      ts(op_timestamp + 100),
      sync_types::SyncDirection::LocalToRemote);

  sync_core::SyncEnvelope envelope;
  envelope.operation = sync_operation;
  envelope.status = sync_types::SyncStatus::Pending;
  envelope.ack_status = sync_types::AckStatus::None;
  envelope.retry_count = 0;
  envelope.last_attempt_at = core_time::Timestamp{};

  if (next_retry_at > 0)
  {
    envelope.next_retry_at = ts(next_retry_at);
  }
  else
  {
    envelope.next_retry_at = core_time::Timestamp{};
  }

  sync_outbox::OutboxEntry entry;
  entry.envelope = envelope;

  return entry;
}

static void test_default_outbox_is_empty()
{
  sync_outbox::Outbox outbox;

  assert(outbox.empty());
  assert(outbox.size() == 0);
  assert(!outbox.find("missing").has_value());
  assert(!outbox.peek_next(ts(1)).has_value());
}

static void test_push_and_find_entry()
{
  sync_outbox::Outbox outbox;
  outbox.push(make_entry("node-a-1", 1, 1000));

  assert(!outbox.empty());
  assert(outbox.size() == 1);

  const auto found = outbox.find("node-a-1");
  assert(found.has_value());
  assert(found->sync_id() == "node-a-1");
  assert(found->is_valid());
  assert(found->envelope.operation.version == 1);
  assert(found->envelope.operation.operation.key.value() == "key-node-a-1");
}

static void test_peek_next_returns_first_ready_entry()
{
  sync_outbox::Outbox outbox;
  outbox.push(make_entry("node-a-1", 1, 1000, 0));
  outbox.push(make_entry("node-a-2", 2, 2000, 5000));

  const auto next_at_1 = outbox.peek_next(ts(1));
  assert(next_at_1.has_value());
  assert(next_at_1->sync_id() == "node-a-1");

  const auto next_at_6000 = outbox.peek_next(ts(6000));
  assert(next_at_6000.has_value());
  assert(next_at_6000->sync_id() == "node-a-1");
}

static void test_next_batch_returns_only_ready_entries()
{
  sync_outbox::Outbox outbox;
  outbox.push(make_entry("node-a-1", 1, 1000, 0));
  outbox.push(make_entry("node-a-2", 2, 2000, 0));
  outbox.push(make_entry("node-a-3", 3, 3000, 9000));

  const std::vector<sync_outbox::OutboxEntry> batch =
      outbox.next_batch(ts(1000), 10);

  assert(batch.size() == 2);
  assert(batch[0].sync_id() == "node-a-1");
  assert(batch[1].sync_id() == "node-a-2");
}

static void test_mark_queued_changes_status()
{
  sync_outbox::Outbox outbox;
  outbox.push(make_entry("node-a-1", 1, 1000));

  const bool ok = outbox.mark_queued("node-a-1");
  assert(ok);

  const auto found = outbox.find("node-a-1");
  assert(found.has_value());
  assert(found->envelope.status == sync_types::SyncStatus::Queued);
  assert(found->envelope.ack_status == sync_types::AckStatus::None);
}

static void test_mark_in_flight_sets_ack_tracking_fields()
{
  sync_outbox::Outbox outbox;
  outbox.push(make_entry("node-a-1", 1, 1000));

  const bool ok = outbox.mark_in_flight("node-a-1", true);
  assert(ok);

  const auto found = outbox.find("node-a-1");
  assert(found.has_value());
  assert(found->envelope.status == sync_types::SyncStatus::InFlight);
  assert(found->envelope.ack_status == sync_types::AckStatus::Waiting);
  assert(found->envelope.last_attempt_at.is_valid());
  assert(found->envelope.retry_count == 1);
}

static void test_mark_acked_and_applied()
{
  sync_outbox::Outbox outbox;
  outbox.push(make_entry("node-a-1", 1, 1000));

  assert(outbox.mark_acked("node-a-1"));

  auto found = outbox.find("node-a-1");
  assert(found.has_value());
  assert(found->envelope.status == sync_types::SyncStatus::Acknowledged);
  assert(found->envelope.ack_status == sync_types::AckStatus::Received);

  assert(outbox.mark_applied("node-a-1"));

  found = outbox.find("node-a-1");
  assert(found.has_value());
  assert(found->envelope.status == sync_types::SyncStatus::Applied);
}

static void test_mark_failed_sets_timeout_and_retry_time()
{
  sync_outbox::Outbox outbox;
  outbox.push(make_entry("node-a-1", 1, 1000));

  assert(outbox.mark_failed(
      "node-a-1",
      ts(8000),
      duration(3000)));

  const auto found = outbox.find("node-a-1");
  assert(found.has_value());
  assert(found->envelope.status == sync_types::SyncStatus::Failed);
  assert(found->envelope.ack_status == sync_types::AckStatus::None);
  assert(found->envelope.next_retry_at.millis() == 11000);
  assert(found->retryable());
}

static void test_requeue_expired_moves_waiting_entries_back_to_queued()
{
  sync_outbox::Outbox outbox;

  auto a = make_entry("node-a-1", 1, 1000);
  auto b = make_entry("node-a-2", 2, 2000);

  a.envelope.status = sync_types::SyncStatus::InFlight;
  a.envelope.ack_status = sync_types::AckStatus::Waiting;
  a.envelope.next_retry_at = ts(5000);

  b.envelope.status = sync_types::SyncStatus::InFlight;
  b.envelope.ack_status = sync_types::AckStatus::Waiting;
  b.envelope.next_retry_at = ts(15000);

  outbox.push(std::move(a));
  outbox.push(std::move(b));

  const std::size_t count =
      outbox.requeue_expired(ts(7000), duration(3000));

  assert(count == 1);

  const auto first = outbox.find("node-a-1");
  const auto second = outbox.find("node-a-2");

  assert(first.has_value());
  assert(second.has_value());

  assert(first->envelope.status == sync_types::SyncStatus::Queued);
  assert(first->envelope.ack_status == sync_types::AckStatus::None);
  assert(first->envelope.next_retry_at.millis() == 10000);

  assert(second->envelope.status == sync_types::SyncStatus::InFlight);
  assert(second->envelope.ack_status == sync_types::AckStatus::Waiting);
  assert(second->envelope.next_retry_at.millis() == 15000);
}

static void test_erase_removes_entry()
{
  sync_outbox::Outbox outbox;
  outbox.push(make_entry("node-a-1", 1, 1000));
  outbox.push(make_entry("node-a-2", 2, 2000));

  assert(outbox.erase("node-a-1"));
  assert(!outbox.find("node-a-1").has_value());
  assert(outbox.find("node-a-2").has_value());
  assert(outbox.size() == 1);
}

static void test_prune_completed_removes_acknowledged_and_applied()
{
  sync_outbox::Outbox outbox;

  auto a = make_entry("node-a-1", 1, 1000);
  auto b = make_entry("node-a-2", 2, 2000);
  auto c = make_entry("node-a-3", 3, 3000);

  a.envelope.status = sync_types::SyncStatus::Acknowledged;
  b.envelope.status = sync_types::SyncStatus::Applied;
  c.envelope.status = sync_types::SyncStatus::Queued;

  outbox.push(std::move(a));
  outbox.push(std::move(b));
  outbox.push(std::move(c));

  const std::size_t removed = outbox.prune_completed();
  assert(removed == 2);
  assert(outbox.size() == 1);

  const auto remaining = outbox.find("node-a-3");
  assert(remaining.has_value());
  assert(remaining->envelope.status == sync_types::SyncStatus::Queued);
}

int main()
{
  test_default_outbox_is_empty();
  test_push_and_find_entry();
  test_peek_next_returns_first_ready_entry();
  test_next_batch_returns_only_ready_entries();
  test_mark_queued_changes_status();
  test_mark_in_flight_sets_ack_tracking_fields();
  test_mark_acked_and_applied();
  test_mark_failed_sets_timeout_and_retry_time();
  test_requeue_expired_moves_waiting_entries_back_to_queued();
  test_erase_removes_entry();
  test_prune_completed_removes_acknowledged_and_applied();

  return 0;
}
