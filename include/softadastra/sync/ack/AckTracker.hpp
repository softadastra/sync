/*
 * AckTracker.hpp
 */

#ifndef SOFTADASTRA_SYNC_ACK_TRACKER_HPP
#define SOFTADASTRA_SYNC_ACK_TRACKER_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <softadastra/sync/types/AckStatus.hpp>

namespace softadastra::sync::ack
{
  namespace types = softadastra::sync::types;

  /**
   * @brief Tracks sync operations waiting for acknowledgement
   */
  class AckTracker
  {
  public:
    /**
     * @brief Metadata for one tracked acknowledgement
     */
    struct AckEntry
    {
      std::string sync_id;
      std::uint64_t tracked_at{0};
      std::uint64_t expires_at{0};
      types::AckStatus status{types::AckStatus::Waiting};
    };

    /**
     * @brief Start tracking an operation awaiting acknowledgement
     */
    void track(const std::string &sync_id,
               std::uint64_t now_ms,
               std::uint64_t timeout_ms)
    {
      AckEntry entry;
      entry.sync_id = sync_id;
      entry.tracked_at = now_ms;
      entry.expires_at = now_ms + timeout_ms;
      entry.status = types::AckStatus::Waiting;

      entries_[sync_id] = std::move(entry);
    }

    /**
     * @brief Mark an acknowledgement as received
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
     * @brief Stop tracking an operation
     */
    bool erase(const std::string &sync_id)
    {
      return entries_.erase(sync_id) > 0;
    }

    /**
     * @brief Return true if the operation is currently tracked
     */
    bool contains(const std::string &sync_id) const
    {
      return entries_.find(sync_id) != entries_.end();
    }

    /**
     * @brief Return true if the operation is still waiting for ack
     */
    bool is_waiting(const std::string &sync_id) const
    {
      auto it = entries_.find(sync_id);
      if (it == entries_.end())
      {
        return false;
      }

      return it->second.status == types::AckStatus::Waiting;
    }

    /**
     * @brief Get one tracked entry
     */
    std::optional<AckEntry> get(const std::string &sync_id) const
    {
      auto it = entries_.find(sync_id);
      if (it == entries_.end())
      {
        return std::nullopt;
      }

      return it->second;
    }

    /**
     * @brief Collect all expired acknowledgements
     *
     * Expired waiting entries are marked as TimedOut and returned.
     */
    std::vector<AckEntry> collect_expired(std::uint64_t now_ms)
    {
      std::vector<AckEntry> expired;

      for (auto &[_, entry] : entries_)
      {
        if (entry.status != types::AckStatus::Waiting)
        {
          continue;
        }

        if (now_ms < entry.expires_at)
        {
          continue;
        }

        entry.status = types::AckStatus::TimedOut;
        expired.push_back(entry);
      }

      return expired;
    }

    /**
     * @brief Remove all entries already acknowledged
     *
     * Returns the number of removed entries.
     */
    std::size_t prune_received()
    {
      std::size_t removed = 0;

      for (auto it = entries_.begin(); it != entries_.end();)
      {
        if (it->second.status == types::AckStatus::Received)
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

    /**
     * @brief Number of tracked entries
     */
    std::size_t size() const noexcept
    {
      return entries_.size();
    }

    /**
     * @brief Return true if nothing is tracked
     */
    bool empty() const noexcept
    {
      return entries_.empty();
    }

    /**
     * @brief Clear all tracked entries
     */
    void clear()
    {
      entries_.clear();
    }

  private:
    std::unordered_map<std::string, AckEntry> entries_;
  };

} // namespace softadastra::sync::ack

#endif
