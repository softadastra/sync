/*
 * SyncQueue.hpp
 */

#ifndef SOFTADASTRA_SYNC_QUEUE_HPP
#define SOFTADASTRA_SYNC_QUEUE_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <softadastra/sync/core/SyncEnvelope.hpp>

namespace softadastra::sync::queue
{
  namespace core = softadastra::sync::core;

  /**
   * @brief In-memory scheduling queue for sync envelopes
   *
   * This queue is intentionally simple:
   * - in-memory only
   * - vector-backed
   * - sorted by version, then timestamp, then sync_id
   *
   * The outbox remains the source of truth.
   * This queue only helps determine send order.
   */
  class SyncQueue
  {
  public:
    /**
     * @brief Insert an envelope into the queue
     */
    void push(const core::SyncEnvelope &envelope)
    {
      queue_.push_back(envelope);
      sort_queue();
    }

    /**
     * @brief Insert an envelope into the queue by move
     */
    void push(core::SyncEnvelope &&envelope)
    {
      queue_.push_back(std::move(envelope));
      sort_queue();
    }

    /**
     * @brief Return true if queue is empty
     */
    bool empty() const noexcept
    {
      return queue_.empty();
    }

    /**
     * @brief Queue size
     */
    std::size_t size() const noexcept
    {
      return queue_.size();
    }

    /**
     * @brief Clear queue
     */
    void clear()
    {
      queue_.clear();
    }

    /**
     * @brief Return the first envelope without removing it
     */
    std::optional<core::SyncEnvelope> front() const
    {
      if (queue_.empty())
      {
        return std::nullopt;
      }

      return queue_.front();
    }

    /**
     * @brief Remove and return the first envelope
     */
    std::optional<core::SyncEnvelope> pop()
    {
      if (queue_.empty())
      {
        return std::nullopt;
      }

      core::SyncEnvelope value = std::move(queue_.front());
      queue_.erase(queue_.begin());
      return value;
    }

    /**
     * @brief Return up to max_count envelopes without removing them
     */
    std::vector<core::SyncEnvelope> peek_batch(std::size_t max_count) const
    {
      const std::size_t count = std::min(max_count, queue_.size());

      std::vector<core::SyncEnvelope> batch;
      batch.reserve(count);

      for (std::size_t i = 0; i < count; ++i)
      {
        batch.push_back(queue_[i]);
      }

      return batch;
    }

    /**
     * @brief Remove and return up to max_count envelopes
     */
    std::vector<core::SyncEnvelope> pop_batch(std::size_t max_count)
    {
      const std::size_t count = std::min(max_count, queue_.size());

      std::vector<core::SyncEnvelope> batch;
      batch.reserve(count);

      for (std::size_t i = 0; i < count; ++i)
      {
        batch.push_back(std::move(queue_[i]));
      }

      queue_.erase(queue_.begin(), queue_.begin() + static_cast<std::ptrdiff_t>(count));

      return batch;
    }

    /**
     * @brief Return true if an envelope with this sync id exists
     */
    bool contains(const std::string &sync_id) const
    {
      return find_index(sync_id).has_value();
    }

    /**
     * @brief Remove one envelope by sync id
     */
    bool erase(const std::string &sync_id)
    {
      const auto index = find_index(sync_id);
      if (!index.has_value())
      {
        return false;
      }

      queue_.erase(queue_.begin() + static_cast<std::ptrdiff_t>(*index));
      return true;
    }

    /**
     * @brief Read-only access to queued envelopes
     */
    const std::vector<core::SyncEnvelope> &entries() const noexcept
    {
      return queue_;
    }

  private:
    std::optional<std::size_t> find_index(const std::string &sync_id) const
    {
      for (std::size_t i = 0; i < queue_.size(); ++i)
      {
        if (queue_[i].operation.sync_id == sync_id)
        {
          return i;
        }
      }

      return std::nullopt;
    }

    void sort_queue()
    {
      std::sort(
          queue_.begin(),
          queue_.end(),
          [](const core::SyncEnvelope &a, const core::SyncEnvelope &b)
          {
            if (a.operation.version != b.operation.version)
            {
              return a.operation.version < b.operation.version;
            }

            if (a.operation.timestamp != b.operation.timestamp)
            {
              return a.operation.timestamp < b.operation.timestamp;
            }

            return a.operation.sync_id < b.operation.sync_id;
          });
    }

  private:
    std::vector<core::SyncEnvelope> queue_;
  };

} // namespace softadastra::sync::queue

#endif
