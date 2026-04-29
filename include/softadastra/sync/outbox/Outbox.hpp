/**
 *
 *  @file Outbox.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Softadastra.
 *  All rights reserved.
 *  https://github.com/softadastra/softadastra
 *
 *  Licensed under the Apache License, Version 2.0.
 *
 *  Softadastra Sync
 *
 */

#ifndef SOFTADASTRA_SYNC_OUTBOX_HPP
#define SOFTADASTRA_SYNC_OUTBOX_HPP

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <softadastra/core/Core.hpp>
#include <softadastra/sync/outbox/OutboxEntry.hpp>
#include <softadastra/sync/types/AckStatus.hpp>
#include <softadastra/sync/types/SyncStatus.hpp>

namespace softadastra::sync::outbox
{
  namespace types = softadastra::sync::types;
  namespace core_time = softadastra::core::time;

  /**
   * @brief In-memory outbox for pending sync operations.
   *
   * Outbox is the local source of truth for sync operations that still need to
   * be sent, retried, acknowledged, or applied.
   *
   * This first version is intentionally simple:
   * - in-memory only
   * - vector-backed
   * - linear lookups
   * - deterministic ordering delegated to SyncQueue
   *
   * It can later evolve toward indexed or persistent storage without changing
   * the high-level SyncEngine API.
   */
  class Outbox
  {
  public:
    /**
     * @brief Internal container type.
     */
    using Container = std::vector<OutboxEntry>;

    /**
     * @brief Creates an empty outbox.
     */
    Outbox() = default;

    /**
     * @brief Inserts a new outbox entry.
     *
     * Invalid entries are ignored.
     *
     * @param entry Outbox entry.
     */
    void push(const OutboxEntry &entry)
    {
      if (!entry.is_valid())
      {
        return;
      }

      entries_.push_back(entry);
    }

    /**
     * @brief Inserts a new outbox entry by move.
     *
     * Invalid entries are ignored.
     *
     * @param entry Outbox entry.
     */
    void push(OutboxEntry &&entry)
    {
      if (!entry.is_valid())
      {
        return;
      }

      entries_.push_back(std::move(entry));
    }

    /**
     * @brief Inserts or replaces an entry by sync id.
     *
     * @param entry Outbox entry.
     */
    void upsert(OutboxEntry entry)
    {
      if (!entry.is_valid())
      {
        return;
      }

      auto it = find_it(entry.sync_id());

      if (it == entries_.end())
      {
        entries_.push_back(std::move(entry));
        return;
      }

      *it = std::move(entry);
    }

    /**
     * @brief Returns true if the outbox is empty.
     *
     * @return true when no entry is stored.
     */
    [[nodiscard]] bool empty() const noexcept
    {
      return entries_.empty();
    }

    /**
     * @brief Returns the number of stored entries.
     *
     * @return Entry count.
     */
    [[nodiscard]] std::size_t size() const noexcept
    {
      return entries_.size();
    }

    /**
     * @brief Removes all entries.
     */
    void clear() noexcept
    {
      entries_.clear();
    }

    /**
     * @brief Finds an entry by sync id.
     *
     * @param sync_id Sync operation id.
     * @return Entry if found, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<OutboxEntry>
    find(const std::string &sync_id) const
    {
      const auto it = find_it(sync_id);

      if (it == entries_.end())
      {
        return std::nullopt;
      }

      return *it;
    }

    /**
     * @brief Finds an entry by sync id without copying.
     *
     * @param sync_id Sync operation id.
     * @return Entry pointer, or nullptr if missing.
     */
    [[nodiscard]] const OutboxEntry *
    find_ptr(const std::string &sync_id) const noexcept
    {
      const auto it = find_it(sync_id);

      if (it == entries_.end())
      {
        return nullptr;
      }

      return &(*it);
    }

    /**
     * @brief Finds an entry by sync id without copying.
     *
     * @param sync_id Sync operation id.
     * @return Entry pointer, or nullptr if missing.
     */
    [[nodiscard]] OutboxEntry *
    find_ptr(const std::string &sync_id) noexcept
    {
      const auto it = find_it(sync_id);

      if (it == entries_.end())
      {
        return nullptr;
      }

      return &(*it);
    }

    /**
     * @brief Returns true if an entry exists.
     *
     * @param sync_id Sync operation id.
     * @return true if found.
     */
    [[nodiscard]] bool contains(const std::string &sync_id) const
    {
      return find_it(sync_id) != entries_.end();
    }

    /**
     * @brief Returns the first ready entry without removing it.
     *
     * @param now Current timestamp.
     * @return Ready entry, or std::nullopt if none is ready.
     */
    [[nodiscard]] std::optional<OutboxEntry>
    peek_next(core_time::Timestamp now) const
    {
      for (const auto &entry : entries_)
      {
        if (entry.ready(now))
        {
          return entry;
        }
      }

      return std::nullopt;
    }

    /**
     * @brief Returns the first ready entry using the current time.
     *
     * @return Ready entry, or std::nullopt if none is ready.
     */
    [[nodiscard]] std::optional<OutboxEntry> peek_next() const
    {
      return peek_next(core_time::Timestamp::now());
    }

    /**
     * @brief Returns up to max_count ready entries without removing them.
     *
     * @param now Current timestamp.
     * @param max_count Maximum number of entries.
     * @return Ready entry batch.
     */
    [[nodiscard]] std::vector<OutboxEntry>
    next_batch(core_time::Timestamp now, std::size_t max_count) const
    {
      std::vector<OutboxEntry> batch;
      batch.reserve(max_count);

      if (max_count == 0)
      {
        return batch;
      }

      for (const auto &entry : entries_)
      {
        if (!entry.ready(now))
        {
          continue;
        }

        batch.push_back(entry);

        if (batch.size() >= max_count)
        {
          break;
        }
      }

      return batch;
    }

    /**
     * @brief Returns up to max_count ready entries using the current time.
     *
     * @param max_count Maximum number of entries.
     * @return Ready entry batch.
     */
    [[nodiscard]] std::vector<OutboxEntry>
    next_batch(std::size_t max_count) const
    {
      return next_batch(core_time::Timestamp::now(), max_count);
    }

    /**
     * @brief Marks an entry as queued.
     *
     * @param sync_id Sync operation id.
     * @return true if an entry was found and updated.
     */
    bool mark_queued(const std::string &sync_id)
    {
      auto *entry = find_ptr(sync_id);

      if (entry == nullptr)
      {
        return false;
      }

      entry->mark_queued();
      return true;
    }

    /**
     * @brief Marks an entry as in-flight.
     *
     * The entry records the current attempt time and optionally waits for ack.
     *
     * @param sync_id Sync operation id.
     * @param require_ack Whether acknowledgement should be tracked.
     * @return true if an entry was found and updated.
     */
    bool mark_in_flight(
        const std::string &sync_id,
        bool require_ack = true)
    {
      auto *entry = find_ptr(sync_id);

      if (entry == nullptr)
      {
        return false;
      }

      entry->mark_in_flight(require_ack);
      return true;
    }

    /**
     * @brief Marks an entry as acknowledged.
     *
     * @param sync_id Sync operation id.
     * @return true if an entry was found and updated.
     */
    bool mark_acked(const std::string &sync_id)
    {
      auto *entry = find_ptr(sync_id);

      if (entry == nullptr)
      {
        return false;
      }

      entry->mark_acknowledged();
      return true;
    }

    /**
     * @brief Marks an entry as applied.
     *
     * @param sync_id Sync operation id.
     * @return true if an entry was found and updated.
     */
    bool mark_applied(const std::string &sync_id)
    {
      auto *entry = find_ptr(sync_id);

      if (entry == nullptr)
      {
        return false;
      }

      entry->mark_applied();
      return true;
    }

    /**
     * @brief Marks an entry as failed and schedules retry.
     *
     * @param sync_id Sync operation id.
     * @param now Current timestamp.
     * @param retry_interval Retry delay.
     * @return true if an entry was found and updated.
     */
    bool mark_failed(
        const std::string &sync_id,
        core_time::Timestamp now,
        core_time::Duration retry_interval)
    {
      auto *entry = find_ptr(sync_id);

      if (entry == nullptr || !now.is_valid())
      {
        return false;
      }

      entry->mark_failed();

      entry->schedule_retry_at(
          core_time::Timestamp::from_millis(
              now.millis() + retry_interval.millis()));

      return true;
    }

    /**
     * @brief Marks an entry as failed and schedules retry using current time.
     *
     * @param sync_id Sync operation id.
     * @param retry_interval Retry delay.
     * @return true if an entry was found and updated.
     */
    bool mark_failed(
        const std::string &sync_id,
        core_time::Duration retry_interval)
    {
      return mark_failed(
          sync_id,
          core_time::Timestamp::now(),
          retry_interval);
    }

    /**
     * @brief Requeues all expired waiting entries.
     *
     * Waiting entries whose retry/ack timeout has expired are marked timed out,
     * then moved back to Queued and scheduled with a retry delay.
     *
     * @param now Current timestamp.
     * @param retry_interval Retry delay.
     * @return Number of requeued entries.
     */
    std::size_t requeue_expired(
        core_time::Timestamp now,
        core_time::Duration retry_interval)
    {
      if (!now.is_valid())
      {
        return 0;
      }

      std::size_t count = 0;

      for (auto &entry : entries_)
      {
        const bool expired =
            entry.awaiting_ack() &&
            entry.envelope.next_retry_at.is_valid() &&
            !(now < entry.envelope.next_retry_at);

        if (!expired)
        {
          continue;
        }

        entry.mark_timed_out();
        entry.mark_queued();

        entry.schedule_retry_at(
            core_time::Timestamp::from_millis(
                now.millis() + retry_interval.millis()));

        ++count;
      }

      return count;
    }

    /**
     * @brief Requeues expired entries using the current time.
     *
     * @param retry_interval Retry delay.
     * @return Number of requeued entries.
     */
    std::size_t requeue_expired(core_time::Duration retry_interval)
    {
      return requeue_expired(
          core_time::Timestamp::now(),
          retry_interval);
    }

    /**
     * @brief Removes one entry by sync id.
     *
     * @param sync_id Sync operation id.
     * @return true if an entry was removed.
     */
    bool erase(const std::string &sync_id)
    {
      auto it = find_it(sync_id);

      if (it == entries_.end())
      {
        return false;
      }

      entries_.erase(it);
      return true;
    }

    /**
     * @brief Removes all acknowledged or applied entries.
     *
     * @return Number of removed entries.
     */
    std::size_t prune_completed()
    {
      const auto old_size = entries_.size();

      entries_.erase(
          std::remove_if(
              entries_.begin(),
              entries_.end(),
              [](const OutboxEntry &entry)
              {
                return entry.envelope.status == types::SyncStatus::Acknowledged ||
                       entry.envelope.status == types::SyncStatus::Applied;
              }),
          entries_.end());

      return old_size - entries_.size();
    }

    /**
     * @brief Removes all failed entries.
     *
     * @return Number of removed entries.
     */
    std::size_t prune_failed()
    {
      const auto old_size = entries_.size();

      entries_.erase(
          std::remove_if(
              entries_.begin(),
              entries_.end(),
              [](const OutboxEntry &entry)
              {
                return entry.envelope.status == types::SyncStatus::Failed;
              }),
          entries_.end());

      return old_size - entries_.size();
    }

    /**
     * @brief Returns read-only access to all entries.
     *
     * @return Outbox entries.
     */
    [[nodiscard]] const Container &entries() const noexcept
    {
      return entries_;
    }

  private:
    using Iterator = Container::iterator;
    using ConstIterator = Container::const_iterator;

    /**
     * @brief Finds an entry iterator by sync id.
     */
    [[nodiscard]] Iterator find_it(const std::string &sync_id)
    {
      return std::find_if(
          entries_.begin(),
          entries_.end(),
          [&](const OutboxEntry &entry)
          {
            return entry.sync_id() == sync_id;
          });
    }

    /**
     * @brief Finds an entry iterator by sync id.
     */
    [[nodiscard]] ConstIterator find_it(const std::string &sync_id) const
    {
      return std::find_if(
          entries_.begin(),
          entries_.end(),
          [&](const OutboxEntry &entry)
          {
            return entry.sync_id() == sync_id;
          });
    }

  private:
    Container entries_{};
  };

} // namespace softadastra::sync::outbox

#endif // SOFTADASTRA_SYNC_OUTBOX_HPP
