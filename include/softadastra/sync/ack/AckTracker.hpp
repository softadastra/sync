/**
 *
 *  @file AckTracker.hpp
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

#ifndef SOFTADASTRA_SYNC_ACK_TRACKER_HPP
#define SOFTADASTRA_SYNC_ACK_TRACKER_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <softadastra/core/Core.hpp>
#include <softadastra/sync/types/AckStatus.hpp>

namespace softadastra::sync::ack
{
  namespace types = softadastra::sync::types;
  namespace core_time = softadastra::core::time;

  /**
   * @brief Tracks sync operations waiting for acknowledgement.
   *
   * AckTracker keeps a small in-memory table of operations that have been sent
   * and are waiting for acknowledgement from a remote peer.
   *
   * It is used by:
   * - SyncEngine
   * - SyncScheduler
   * - retry logic
   * - diagnostics
   *
   * AckTracker does not send network messages.
   * It only tracks acknowledgement state and timeout eligibility.
   */
  class AckTracker
  {
  public:
    /**
     * @brief Metadata for one tracked acknowledgement.
     */
    struct AckEntry
    {
      /**
       * @brief Sync operation id being tracked.
       */
      std::string sync_id{};

      /**
       * @brief Time when tracking started.
       */
      core_time::Timestamp tracked_at{};

      /**
       * @brief Time when the acknowledgement expires.
       */
      core_time::Timestamp expires_at{};

      /**
       * @brief Current acknowledgement status.
       */
      types::AckStatus status{types::AckStatus::Waiting};

      /**
       * @brief Creates an empty acknowledgement entry.
       */
      AckEntry() = default;

      /**
       * @brief Creates an acknowledgement entry.
       *
       * @param id Sync operation id.
       * @param tracked Tracking start timestamp.
       * @param expires Expiration timestamp.
       */
      AckEntry(
          std::string id,
          core_time::Timestamp tracked,
          core_time::Timestamp expires)
          : sync_id(std::move(id)),
            tracked_at(tracked),
            expires_at(expires),
            status(types::AckStatus::Waiting)
      {
      }

      /**
       * @brief Returns true if this entry is waiting for acknowledgement.
       *
       * @return true when status is Waiting.
       */
      [[nodiscard]] bool is_waiting() const noexcept
      {
        return types::is_waiting(status);
      }

      /**
       * @brief Returns true if this entry was acknowledged.
       *
       * @return true when status is Received.
       */
      [[nodiscard]] bool acknowledged() const noexcept
      {
        return status == types::AckStatus::Received;
      }

      /**
       * @brief Returns true if this entry timed out.
       *
       * @return true when status is TimedOut.
       */
      [[nodiscard]] bool timed_out() const noexcept
      {
        return status == types::AckStatus::TimedOut;
      }

      /**
       * @brief Returns true if this entry reached a terminal ack state.
       *
       * @return true for Received and TimedOut.
       */
      [[nodiscard]] bool terminal() const noexcept
      {
        return types::is_terminal(status);
      }

      /**
       * @brief Returns true if the entry is structurally valid.
       *
       * @return true when id and timestamps are usable.
       */
      [[nodiscard]] bool is_valid() const noexcept
      {
        return !sync_id.empty() &&
               tracked_at.is_valid() &&
               expires_at.is_valid() &&
               types::is_valid(status);
      }
    };

    /**
     * @brief Internal map type.
     */
    using Map = std::unordered_map<std::string, AckEntry>;

    /**
     * @brief Creates an empty acknowledgement tracker.
     */
    AckTracker() = default;

    /**
     * @brief Starts tracking an operation awaiting acknowledgement.
     *
     * If the sync id already exists, its entry is replaced.
     *
     * @param sync_id Sync operation id.
     * @param now Current timestamp.
     * @param timeout Acknowledgement timeout.
     */
    void track(
        std::string sync_id,
        core_time::Timestamp now,
        core_time::Duration timeout)
    {
      if (sync_id.empty() || !now.is_valid())
      {
        return;
      }

      const auto expires =
          core_time::Timestamp::from_millis(
              now.millis() + timeout.millis());

      AckEntry entry{
          sync_id,
          now,
          expires};

      entries_[entry.sync_id] = std::move(entry);
    }

    /**
     * @brief Starts tracking an operation using the current timestamp.
     *
     * @param sync_id Sync operation id.
     * @param timeout Acknowledgement timeout.
     */
    void track(std::string sync_id, core_time::Duration timeout)
    {
      track(
          std::move(sync_id),
          core_time::Timestamp::now(),
          timeout);
    }

    /**
     * @brief Marks an acknowledgement as received.
     *
     * @param sync_id Sync operation id.
     * @return true if an entry was found and updated.
     */
    bool ack(const std::string &sync_id)
    {
      auto it = entries_.find(sync_id);

      if (it == entries_.end())
      {
        return false;
      }

      it->second.status = types::AckStatus::Received;
      return true;
    }

    /**
     * @brief Marks an acknowledgement as timed out.
     *
     * @param sync_id Sync operation id.
     * @return true if an entry was found and updated.
     */
    bool mark_timed_out(const std::string &sync_id)
    {
      auto it = entries_.find(sync_id);

      if (it == entries_.end())
      {
        return false;
      }

      it->second.status = types::AckStatus::TimedOut;
      return true;
    }

    /**
     * @brief Stops tracking an operation.
     *
     * @param sync_id Sync operation id.
     * @return true if an entry was removed.
     */
    bool erase(const std::string &sync_id)
    {
      return entries_.erase(sync_id) > 0;
    }

    /**
     * @brief Returns true if an operation is currently tracked.
     *
     * @param sync_id Sync operation id.
     * @return true if found.
     */
    [[nodiscard]] bool contains(const std::string &sync_id) const
    {
      return entries_.find(sync_id) != entries_.end();
    }

    /**
     * @brief Returns true if an operation is waiting for acknowledgement.
     *
     * @param sync_id Sync operation id.
     * @return true if found and status is Waiting.
     */
    [[nodiscard]] bool is_waiting(const std::string &sync_id) const
    {
      const auto *entry = find(sync_id);
      return entry != nullptr && entry->is_waiting();
    }

    /**
     * @brief Returns true if an operation has been acknowledged.
     *
     * @param sync_id Sync operation id.
     * @return true if found and status is Received.
     */
    [[nodiscard]] bool acknowledged(const std::string &sync_id) const
    {
      const auto *entry = find(sync_id);
      return entry != nullptr && entry->acknowledged();
    }

    /**
     * @brief Gets one tracked entry by copy.
     *
     * @param sync_id Sync operation id.
     * @return AckEntry if found, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<AckEntry>
    get(const std::string &sync_id) const
    {
      const auto *entry = find(sync_id);

      if (entry == nullptr)
      {
        return std::nullopt;
      }

      return *entry;
    }

    /**
     * @brief Finds one tracked entry without copying.
     *
     * @param sync_id Sync operation id.
     * @return Entry pointer, or nullptr if missing.
     */
    [[nodiscard]] const AckEntry *
    find(const std::string &sync_id) const noexcept
    {
      const auto it = entries_.find(sync_id);

      if (it == entries_.end())
      {
        return nullptr;
      }

      return &it->second;
    }

    /**
     * @brief Finds one tracked entry without copying.
     *
     * @param sync_id Sync operation id.
     * @return Entry pointer, or nullptr if missing.
     */
    [[nodiscard]] AckEntry *
    find(const std::string &sync_id) noexcept
    {
      const auto it = entries_.find(sync_id);

      if (it == entries_.end())
      {
        return nullptr;
      }

      return &it->second;
    }

    /**
     * @brief Collects all expired acknowledgements.
     *
     * Waiting entries whose expiration time is reached are marked as TimedOut
     * and returned by copy.
     *
     * @param now Current timestamp.
     * @return Expired acknowledgement entries.
     */
    [[nodiscard]] std::vector<AckEntry>
    collect_expired(core_time::Timestamp now)
    {
      std::vector<AckEntry> expired;

      if (!now.is_valid())
      {
        return expired;
      }

      for (auto &[_, entry] : entries_)
      {
        if (!entry.is_waiting())
        {
          continue;
        }

        if (now < entry.expires_at)
        {
          continue;
        }

        entry.status = types::AckStatus::TimedOut;
        expired.push_back(entry);
      }

      return expired;
    }

    /**
     * @brief Collects all expired acknowledgements using the current time.
     *
     * @return Expired acknowledgement entries.
     */
    [[nodiscard]] std::vector<AckEntry> collect_expired()
    {
      return collect_expired(core_time::Timestamp::now());
    }

    /**
     * @brief Removes all acknowledged entries.
     *
     * @return Number of removed entries.
     */
    std::size_t prune_received()
    {
      return prune_by_status(types::AckStatus::Received);
    }

    /**
     * @brief Removes all timed-out entries.
     *
     * @return Number of removed entries.
     */
    std::size_t prune_timed_out()
    {
      return prune_by_status(types::AckStatus::TimedOut);
    }

    /**
     * @brief Returns the number of tracked entries.
     *
     * @return Entry count.
     */
    [[nodiscard]] std::size_t size() const noexcept
    {
      return entries_.size();
    }

    /**
     * @brief Returns true if nothing is tracked.
     *
     * @return true when empty.
     */
    [[nodiscard]] bool empty() const noexcept
    {
      return entries_.empty();
    }

    /**
     * @brief Returns all tracked entries.
     *
     * @return Read-only internal map.
     */
    [[nodiscard]] const Map &all() const noexcept
    {
      return entries_;
    }

    /**
     * @brief Clears all tracked entries.
     */
    void clear() noexcept
    {
      entries_.clear();
    }

  private:
    /**
     * @brief Removes entries matching a status.
     *
     * @param status Status to remove.
     * @return Number of removed entries.
     */
    std::size_t prune_by_status(types::AckStatus status)
    {
      std::size_t removed = 0;

      for (auto it = entries_.begin(); it != entries_.end();)
      {
        if (it->second.status == status)
        {
          it = entries_.erase(it);
          ++removed;
        }
        else
        {
          ++it;
        }
      }

      return removed;
    }

  private:
    Map entries_{};
  };

} // namespace softadastra::sync::ack

#endif // SOFTADASTRA_SYNC_ACK_TRACKER_HPP
