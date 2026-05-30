/*
 * test_conflict.cpp
 */

#include <cassert>
#include <cstdint>
#include <optional>
#include <string>

#include <softadastra/core/Core.hpp>
#include <softadastra/store/core/Entry.hpp>
#include <softadastra/store/core/Operation.hpp>
#include <softadastra/store/types/Key.hpp>
#include <softadastra/store/types/OperationType.hpp>
#include <softadastra/store/types/Value.hpp>
#include <softadastra/sync/conflict/ConflictResolver.hpp>
#include <softadastra/sync/types/ConflictPolicy.hpp>

namespace store_core = softadastra::store::core;
namespace store_types = softadastra::store::types;
namespace sync_conflict = softadastra::sync::conflict;
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

static store_core::Entry make_entry(
    const std::string &key,
    std::uint64_t timestamp,
    std::uint64_t version = 0)
{
  store_core::Entry entry;
  entry.key = store_types::Key::from(key);
  entry.value = make_value("local");
  entry.timestamp = ts(timestamp);
  entry.version = version;
  return entry;
}

static store_core::Operation make_operation(
    const std::string &key,
    std::uint64_t timestamp)
{
  store_core::Operation op(
      store_types::OperationType::Put,
      store_types::Key::from(key),
      make_value("remote"),
      ts(timestamp));

  return op;
}

static void test_no_local_entry_applies_remote()
{
  const std::optional<store_core::Entry> local = std::nullopt;
  const auto remote = make_operation("k1", 1000);

  const auto res = sync_conflict::ConflictResolver::resolve(
      local,
      remote,
      sync_types::ConflictPolicy::LastWriteWins,
      "node-a",
      "node-b");

  assert(res.apply_remote);
  assert(!res.keep_local);
  assert(!res.conflict_detected);
}

static void test_keep_local_policy_always_keeps_local()
{
  const auto local = make_entry("k1", 2000);
  const auto remote = make_operation("k1", 3000);

  const auto res = sync_conflict::ConflictResolver::resolve(
      local,
      remote,
      sync_types::ConflictPolicy::KeepLocal,
      "node-a",
      "node-b");

  assert(!res.apply_remote);
  assert(res.keep_local);
  assert(res.conflict_detected);
}

static void test_keep_remote_policy_always_applies_remote()
{
  const auto local = make_entry("k1", 3000);
  const auto remote = make_operation("k1", 1000);

  const auto res = sync_conflict::ConflictResolver::resolve(
      local,
      remote,
      sync_types::ConflictPolicy::KeepRemote,
      "node-a",
      "node-b");

  assert(res.apply_remote);
  assert(!res.keep_local);
  assert(res.conflict_detected);
}

static void test_manual_policy_defaults_to_keep_local()
{
  const auto local = make_entry("k1", 3000);
  const auto remote = make_operation("k1", 4000);

  const auto res = sync_conflict::ConflictResolver::resolve(
      local,
      remote,
      sync_types::ConflictPolicy::Manual,
      "node-a",
      "node-b");

  assert(!res.apply_remote);
  assert(res.keep_local);
  assert(res.conflict_detected);
}

static void test_last_write_wins_remote_newer()
{
  const auto local = make_entry("k1", 1000);
  const auto remote = make_operation("k1", 2000);

  const auto res = sync_conflict::ConflictResolver::resolve(
      local,
      remote,
      sync_types::ConflictPolicy::LastWriteWins,
      "node-a",
      "node-b");

  assert(res.apply_remote);
  assert(!res.keep_local);
  assert(res.conflict_detected);
}

static void test_last_write_wins_local_newer()
{
  const auto local = make_entry("k1", 3000);
  const auto remote = make_operation("k1", 2000);

  const auto res = sync_conflict::ConflictResolver::resolve(
      local,
      remote,
      sync_types::ConflictPolicy::LastWriteWins,
      "node-a",
      "node-b");

  assert(!res.apply_remote);
  assert(res.keep_local);
  assert(res.conflict_detected);
}

static void test_last_write_wins_equal_timestamp_tie_break_remote_wins()
{
  const auto local = make_entry("k1", 2000);
  const auto remote = make_operation("k1", 2000);

  const auto res = sync_conflict::ConflictResolver::resolve(
      local,
      remote,
      sync_types::ConflictPolicy::LastWriteWins,
      "node-a",
      "node-b"); // "node-b" > "node-a"

  assert(res.apply_remote);
  assert(!res.keep_local);
  assert(res.conflict_detected);
}

static void test_last_write_wins_equal_timestamp_tie_break_local_wins()
{
  const auto local = make_entry("k1", 2000);
  const auto remote = make_operation("k1", 2000);

  const auto res = sync_conflict::ConflictResolver::resolve(
      local,
      remote,
      sync_types::ConflictPolicy::LastWriteWins,
      "node-z",
      "node-a"); // "node-a" < "node-z"

  assert(!res.apply_remote);
  assert(res.keep_local);
  assert(res.conflict_detected);
}

static void test_delete_operation_respects_timestamp()
{
  store_core::Operation remote(
      store_types::OperationType::Delete,
      store_types::Key::from("k1"),
      store_types::Value{},
      ts(5000));

  const auto local = make_entry("k1", 4000);

  const auto res = sync_conflict::ConflictResolver::resolve(
      local,
      remote,
      sync_types::ConflictPolicy::LastWriteWins,
      "node-a",
      "node-b");

  assert(res.apply_remote);
}

int main()
{
  test_no_local_entry_applies_remote();
  test_keep_local_policy_always_keeps_local();
  test_keep_remote_policy_always_applies_remote();
  test_manual_policy_defaults_to_keep_local();
  test_last_write_wins_remote_newer();
  test_last_write_wins_local_newer();
  test_last_write_wins_equal_timestamp_tie_break_remote_wins();
  test_last_write_wins_equal_timestamp_tie_break_local_wins();
  test_delete_operation_respects_timestamp();

  return 0;
}
