/**
 *
 *  @file SyncQueue.hpp
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

#ifndef SOFTADASTRA_SYNC_QUEUE_HPP
#define SOFTADASTRA_SYNC_QUEUE_HPP

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <softadastra/sync/core/SyncEnvelope.hpp>

namespace softadastra::sync::queue
{
  namespace core = softadastra::sync::core;

  /**
   * @brief In-memory scheduling queue for sync envelopes.
   *
   * SyncQueue keeps envelopes ordered for deterministic sending.
   *
   * It is intentionally simple:
   * - in-memory only
   * - vector-backed
   * - sorted by version, timestamp, then sync id
   *
   * The outbox remains the source of truth.
   * The queue only determines local scheduling order.
   */
  class SyncQueue
  {
  public:
    /**
     * @brief Internal queue container type.
     */
    using Container = std::vector<core::SyncEnvelope>;

    /**
     * @brief Creates an empty queue.
     */
    SyncQueue() = default;

    /**
     * @brief Inserts an envelope into the queue.
     *
     * Invalid envelopes are ignored.
     *
     * @param envelope Sync envelope.
     */
    void push(const core::SyncEnvelope &envelope)
    {
      if (!envelope.is_valid())
      {
        return;
      }

      queue_.push_back(envelope);
      sort_queue();
    }

    /**
     * @brief Inserts an envelope into the queue by move.
     *
     * Invalid envelopes are ignored.
     *
     * @param envelope Sync envelope.
     */
    void push(core::SyncEnvelope &&envelope)
    {
      if (!envelope.is_valid())
      {
        return;
      }

      queue_.push_back(std::move(envelope));
      sort_queue();
    }

    /**
     * @brief Inserts or replaces an envelope by sync id.
     *
     * @param envelope Sync envelope.
     */
    void upsert(core::SyncEnvelope envelope)
    {
      if (!envelope.is_valid())
      {
        return;
      }

      const auto index = find_index(envelope.operation.sync_id);

      if (index.has_value())
      {
        queue_[*index] = std::move(envelope);
      }
      else
      {
        queue_.push_back(std::move(envelope));
      }

      sort_queue();
    }

    /**
     * @brief Returns true if the queue is empty.
     *
     * @return true when no envelope is queued.
     */
    [[nodiscard]] bool empty() const noexcept
    {
      return queue_.empty();
    }

    /**
     * @brief Returns the queue size.
     *
     * @return Number of queued envelopes.
     */
    [[nodiscard]] std::size_t size() const noexcept
    {
      return queue_.size();
    }

    /**
     * @brief Clears all queued envelopes.
     */
    void clear() noexcept
    {
      queue_.clear();
    }

    /**
     * @brief Returns the first envelope without removing it.
     *
     * @return First envelope, or std::nullopt if empty.
     */
    [[nodiscard]] std::optional<core::SyncEnvelope> front() const
    {
      if (queue_.empty())
      {
        return std::nullopt;
      }

      return queue_.front();
    }

    /**
     * @brief Removes and returns the first envelope.
     *
     * @return First envelope, or std::nullopt if empty.
     */
    [[nodiscard]] std::optional<core::SyncEnvelope> pop()
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
     * @brief Returns up to max_count envelopes without removing them.
     *
     * @param max_count Maximum number of envelopes to return.
     * @return Batch of queued envelopes.
     */
    [[nodiscard]] std::vector<core::SyncEnvelope>
    peek_batch(std::size_t max_count) const
    {
      const std::size_t count =
          std::min(max_count, queue_.size());

      std::vector<core::SyncEnvelope> batch;
      batch.reserve(count);

      for (std::size_t i = 0; i < count; ++i)
      {
        batch.push_back(queue_[i]);
      }

      return batch;
    }

    /**
     * @brief Removes and returns up to max_count envelopes.
     *
     * @param max_count Maximum number of envelopes to return.
     * @return Batch of removed envelopes.
     */
    [[nodiscard]] std::vector<core::SyncEnvelope>
    pop_batch(std::size_t max_count)
    {
      const std::size_t count =
          std::min(max_count, queue_.size());

      std::vector<core::SyncEnvelope> batch;
      batch.reserve(count);

      for (std::size_t i = 0; i < count; ++i)
      {
        batch.push_back(std::move(queue_[i]));
      }

      queue_.erase(
          queue_.begin(),
          queue_.begin() + static_cast<std::ptrdiff_t>(count));

      return batch;
    }

    /**
     * @brief Returns true if an envelope with this sync id exists.
     *
     * @param sync_id Sync operation id.
     * @return true if found.
     */
    [[nodiscard]] bool contains(const std::string &sync_id) const
    {
      return find_index(sync_id).has_value();
    }

    /**
     * @brief Returns an envelope by sync id.
     *
     * @param sync_id Sync operation id.
     * @return Envelope if found, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<core::SyncEnvelope>
    get(const std::string &sync_id) const
    {
      const auto index = find_index(sync_id);

      if (!index.has_value())
      {
        return std::nullopt;
      }

      return queue_[*index];
    }

    /**
     * @brief Removes one envelope by sync id.
     *
     * @param sync_id Sync operation id.
     * @return true if an envelope was removed.
     */
    bool erase(const std::string &sync_id)
    {
      const auto index = find_index(sync_id);

      if (!index.has_value())
      {
        return false;
      }

      queue_.erase(
          queue_.begin() + static_cast<std::ptrdiff_t>(*index));

      return true;
    }

    /**
     * @brief Returns read-only access to queued envelopes.
     *
     * @return Queue entries.
     */
    [[nodiscard]] const Container &entries() const noexcept
    {
      return queue_;
    }

  private:
    /**
     * @brief Finds the index of an envelope by sync id.
     *
     * @param sync_id Sync operation id.
     * @return Index if found, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<std::size_t>
    find_index(const std::string &sync_id) const
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

    /**
     * @brief Sorts the queue deterministically.
     */
    void sort_queue()
    {
      std::sort(
          queue_.begin(),
          queue_.end(),
          [](const core::SyncEnvelope &a,
             const core::SyncEnvelope &b)
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
    Container queue_{};
  };

} // namespace softadastra::sync::queue

#endif // SOFTADASTRA_SYNC_QUEUE_HPP
