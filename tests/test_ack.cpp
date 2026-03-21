/*
 * test_ack.cpp
 */

#include <cassert>
#include <string>
#include <vector>

#include <softadastra/sync/ack/AckTracker.hpp>
#include <softadastra/sync/types/AckStatus.hpp>

namespace sync_ack = softadastra::sync::ack;
namespace sync_types = softadastra::sync::types;

static void test_default_tracker_is_empty()
{
  sync_ack::AckTracker tracker;

  assert(tracker.empty());
  assert(tracker.size() == 0);
  assert(!tracker.contains("op-1"));
  assert(!tracker.is_waiting("op-1"));
  assert(!tracker.get("op-1").has_value());
}

static void test_track_creates_waiting_entry()
{
  sync_ack::AckTracker tracker;

  tracker.track("op-1", 1000, 5000);

  assert(!tracker.empty());
  assert(tracker.size() == 1);
  assert(tracker.contains("op-1"));
  assert(tracker.is_waiting("op-1"));

  const auto entry = tracker.get("op-1");
  assert(entry.has_value());
  assert(entry->sync_id == "op-1");
  assert(entry->tracked_at == 1000);
  assert(entry->expires_at == 6000);
  assert(entry->status == sync_types::AckStatus::Waiting);
}

static void test_ack_marks_entry_as_received()
{
  sync_ack::AckTracker tracker;

  tracker.track("op-1", 1000, 5000);

  const bool ok = tracker.ack("op-1");
  assert(ok);

  const auto entry = tracker.get("op-1");
  assert(entry.has_value());
  assert(entry->status == sync_types::AckStatus::Received);
  assert(!tracker.is_waiting("op-1"));
}

static void test_ack_returns_false_for_missing_entry()
{
  sync_ack::AckTracker tracker;

  assert(!tracker.ack("missing"));
}

static void test_erase_removes_entry()
{
  sync_ack::AckTracker tracker;

  tracker.track("op-1", 1000, 5000);
  tracker.track("op-2", 2000, 5000);

  assert(tracker.erase("op-1"));
  assert(!tracker.contains("op-1"));
  assert(tracker.contains("op-2"));
  assert(tracker.size() == 1);

  assert(!tracker.erase("missing"));
}

static void test_collect_expired_marks_only_expired_waiting_entries()
{
  sync_ack::AckTracker tracker;

  tracker.track("op-1", 1000, 2000); // expires at 3000
  tracker.track("op-2", 2000, 5000); // expires at 7000
  tracker.track("op-3", 3000, 1000); // expires at 4000

  assert(tracker.ack("op-2"));

  const std::vector<sync_ack::AckTracker::AckEntry> expired =
      tracker.collect_expired(4500);

  assert(expired.size() == 2);

  bool found_op1 = false;
  bool found_op3 = false;

  for (const auto &entry : expired)
  {
    if (entry.sync_id == "op-1")
    {
      found_op1 = true;
      assert(entry.status == sync_types::AckStatus::TimedOut);
    }
    else if (entry.sync_id == "op-3")
    {
      found_op3 = true;
      assert(entry.status == sync_types::AckStatus::TimedOut);
    }
    else
    {
      assert(false && "Unexpected expired ack entry");
    }
  }

  assert(found_op1);
  assert(found_op3);

  const auto op1 = tracker.get("op-1");
  const auto op2 = tracker.get("op-2");
  const auto op3 = tracker.get("op-3");

  assert(op1.has_value());
  assert(op2.has_value());
  assert(op3.has_value());

  assert(op1->status == sync_types::AckStatus::TimedOut);
  assert(op2->status == sync_types::AckStatus::Received);
  assert(op3->status == sync_types::AckStatus::TimedOut);
}

static void test_prune_received_removes_only_acknowledged_entries()
{
  sync_ack::AckTracker tracker;

  tracker.track("op-1", 1000, 5000);
  tracker.track("op-2", 2000, 5000);
  tracker.track("op-3", 3000, 5000);

  assert(tracker.ack("op-1"));
  assert(tracker.ack("op-3"));

  const std::size_t removed = tracker.prune_received();
  assert(removed == 2);

  assert(!tracker.contains("op-1"));
  assert(tracker.contains("op-2"));
  assert(!tracker.contains("op-3"));
  assert(tracker.size() == 1);

  const auto remaining = tracker.get("op-2");
  assert(remaining.has_value());
  assert(remaining->status == sync_types::AckStatus::Waiting);
}

static void test_clear_removes_everything()
{
  sync_ack::AckTracker tracker;

  tracker.track("op-1", 1000, 5000);
  tracker.track("op-2", 2000, 5000);

  assert(!tracker.empty());

  tracker.clear();

  assert(tracker.empty());
  assert(tracker.size() == 0);
  assert(!tracker.contains("op-1"));
  assert(!tracker.contains("op-2"));
}

static void test_retracking_same_sync_id_replaces_existing_entry()
{
  sync_ack::AckTracker tracker;

  tracker.track("op-1", 1000, 2000);
  tracker.track("op-1", 5000, 3000);

  assert(tracker.size() == 1);

  const auto entry = tracker.get("op-1");
  assert(entry.has_value());
  assert(entry->tracked_at == 5000);
  assert(entry->expires_at == 8000);
  assert(entry->status == sync_types::AckStatus::Waiting);
}

int main()
{
  test_default_tracker_is_empty();
  test_track_creates_waiting_entry();
  test_ack_marks_entry_as_received();
  test_ack_returns_false_for_missing_entry();
  test_erase_removes_entry();
  test_collect_expired_marks_only_expired_waiting_entries();
  test_prune_received_removes_only_acknowledged_entries();
  test_clear_removes_everything();
  test_retracking_same_sync_id_replaces_existing_entry();

  return 0;
}
