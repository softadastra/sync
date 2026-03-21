/*
 * Outbox.hpp
 */

#ifndef SOFTADASTRA_SYNC_OUTBOX_HPP
#define SOFTADASTRA_SYNC_OUTBOX_HPP

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <softadastra/sync/outbox/OutboxEntry.hpp>
#include <softadastra/sync/types/SyncStatus.hpp>
#include <softadastra/sync/types/AckStatus.hpp>

namespace softadastra::sync::outbox
{
  namespace types = softadastra::sync::types;

  /**
   * @brief In-memory outbox for pending sync operations
   *
   * The outbox is the source of truth for operations that
   * still need to be sent, retried, or acknowledged.
   *
   * This first version is intentionally simple:
   * - in-memory only
   * - vector-backed
   * - linear lookups
   *
   * It can later evolve toward indexed or persistent storage.
   */
  class Outbox
  {
  public:
    /**
     * @brief Insert a new outbox entry
     */
    void push(const OutboxEntry &entry)
    {
      entries_.push_back(entry);
    }

    /**
     * @brief Insert a new outbox entry by move
     */
    void push(OutboxEntry &&entry)
    {
      entries_.push_back(std::move(entry));
    }

    /**
     * @brief Return true if the outbox is empty
     */
    bool empty() const noexcept
    {
      return entries_.empty();
    }

    /**
     * @brief Number of stored entries
     */
    std::size_t size() const noexcept
    {
      return entries_.size();
    }

    /**
     * @brief Remove all entries
     */
    void clear()
    {
      entries_.clear();
    }

    /**
     * @brief Find an entry by sync id
     */
    std::optional<OutboxEntry> find(const std::string &sync_id) const
    {
      const auto it = find_it(sync_id);
      if (it == entries_.end())
      {
        return std::nullopt;
      }

      return *it;
    }

    /**
     * @brief Return the first ready entry without removing it
     */
    std::optional<OutboxEntry> peek_next(std::uint64_t now_ms) const
    {
      for (const auto &entry : entries_)
      {
        if (entry.ready(now_ms))
        {
          return entry;
        }
      }

      return std::nullopt;
    }

    /**
     * @brief Return up to max_count ready entries without removing them
     */
    std::vector<OutboxEntry> next_batch(std::uint64_t now_ms,
                                        std::size_t max_count) const
    {
      std::vector<OutboxEntry> batch;
      batch.reserve(max_count);

      for (const auto &entry : entries_)
      {
        if (!entry.ready(now_ms))
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
     * @brief Mark an entry as queued
     */
    bool mark_queued(const std::string &sync_id)
    {
      auto it = find_it(sync_id);
      if (it == entries_.end())
      {
        return false;
      }

      it->envelope.status = types::SyncStatus::Queued;
      return true;
    }

    /**
     * @brief Mark an entry as in flight
     */
    bool mark_in_flight(const std::string &sync_id,
                        std::uint64_t now_ms,
                        std::uint64_t ack_timeout_ms)
    {
      auto it = find_it(sync_id);
      if (it == entries_.end())
      {
        return false;
      }

      it->envelope.status = types::SyncStatus::InFlight;
      it->envelope.ack_status = types::AckStatus::Waiting;
      it->envelope.last_attempt_at = now_ms;
      it->envelope.next_retry_at = now_ms + ack_timeout_ms;
      ++it->envelope.retry_count;

      return true;
    }

    /**
     * @brief Mark an entry as acknowledged
     */
    bool mark_acked(const std::string &sync_id)
    {
      auto it = find_it(sync_id);
      if (it == entries_.end())
      {
        return false;
      }

      it->envelope.status = types::SyncStatus::Acknowledged;
      it->envelope.ack_status = types::AckStatus::Received;
      return true;
    }

    /**
     * @brief Mark an entry as applied
     */
    bool mark_applied(const std::string &sync_id)
    {
      auto it = find_it(sync_id);
      if (it == entries_.end())
      {
        return false;
      }

      it->envelope.status = types::SyncStatus::Applied;
      return true;
    }

    /**
     * @brief Mark an entry as failed and schedule retry
     */
    bool mark_failed(const std::string &sync_id,
                     std::uint64_t now_ms,
                     std::uint64_t retry_interval_ms)
    {
      auto it = find_it(sync_id);
      if (it == entries_.end())
      {
        return false;
      }

      it->envelope.status = types::SyncStatus::Failed;
      it->envelope.ack_status = types::AckStatus::TimedOut;
      it->envelope.next_retry_at = now_ms + retry_interval_ms;
      return true;
    }

    /**
     * @brief Requeue all expired waiting entries
     *
     * Returns the number of entries moved back to queued state.
     */
    std::size_t requeue_expired(std::uint64_t now_ms,
                                std::uint64_t retry_interval_ms)
    {
      std::size_t count = 0;

      for (auto &entry : entries_)
      {
        const bool expired =
            entry.envelope.ack_status == types::AckStatus::Waiting &&
            now_ms >= entry.envelope.next_retry_at;

        if (!expired)
        {
          continue;
        }

        entry.envelope.status = types::SyncStatus::Queued;
        entry.envelope.ack_status = types::AckStatus::TimedOut;
        entry.envelope.next_retry_at = now_ms + retry_interval_ms;
        ++count;
      }

      return count;
    }

    /**
     * @brief Remove one entry by sync id
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
     * @brief Remove all acknowledged or applied entries
     *
     * Returns the number of removed entries.
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
     * @brief Read-only access to all entries
     */
    const std::vector<OutboxEntry> &entries() const noexcept
    {
      return entries_;
    }

  private:
    using Container = std::vector<OutboxEntry>;
    using Iterator = Container::iterator;
    using ConstIterator = Container::const_iterator;

    Iterator find_it(const std::string &sync_id)
    {
      return std::find_if(
          entries_.begin(),
          entries_.end(),
          [&](const OutboxEntry &entry)
          {
            return entry.sync_id() == sync_id;
          });
    }

    ConstIterator find_it(const std::string &sync_id) const
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
    std::vector<OutboxEntry> entries_;
  };

} // namespace softadastra::sync::outbox

#endif
