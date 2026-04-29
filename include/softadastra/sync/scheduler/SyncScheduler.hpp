/**
 *
 *  @file SyncScheduler.hpp
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

#ifndef SOFTADASTRA_SYNC_SCHEDULER_HPP
#define SOFTADASTRA_SYNC_SCHEDULER_HPP

#include <cstddef>
#include <vector>

#include <softadastra/sync/core/SyncEnvelope.hpp>
#include <softadastra/sync/engine/SyncEngine.hpp>

namespace softadastra::sync::scheduler
{

  namespace sync_core = softadastra::sync::core;
  namespace sync_engine = softadastra::sync::engine;

  /**
   * @brief Manual deterministic scheduler for SyncEngine.
   *
   * SyncScheduler drives the sync engine one step at a time.
   *
   * This first version is intentionally simple:
   * - no threads
   * - no timers
   * - no background worker
   * - deterministic tick order
   *
   * Responsibilities:
   * - retry expired acknowledgements
   * - fetch the next batch ready for transport
   * - optionally prune completed entries
   *
   * Transport remains outside the scheduler.
   */
  class SyncScheduler
  {
  public:
    /**
     * @brief Result of one scheduler tick.
     */
    struct TickResult
    {
      /**
       * @brief Number of expired operations requeued.
       */
      std::size_t retried_count{0};

      /**
       * @brief Number of completed entries pruned.
       */
      std::size_t pruned_count{0};

      /**
       * @brief Batch ready to send through transport.
       */
      std::vector<sync_core::SyncEnvelope> batch{};

      /**
       * @brief Returns true if the tick produced work.
       *
       * @return true when retry, prune, or batch work happened.
       */
      [[nodiscard]] bool has_work() const noexcept
      {
        return retried_count > 0 ||
               pruned_count > 0 ||
               !batch.empty();
      }

      /**
       * @brief Returns the number of envelopes returned in the batch.
       *
       * @return Batch size.
       */
      [[nodiscard]] std::size_t batch_size() const noexcept
      {
        return batch.size();
      }
    };

    /**
     * @brief Creates a scheduler for a sync engine.
     *
     * The scheduler does not own the engine.
     *
     * @param engine Sync engine to drive.
     */
    explicit SyncScheduler(sync_engine::SyncEngine &engine) noexcept
        : engine_(engine)
    {
    }

    /**
     * @brief Performs one scheduler cycle.
     *
     * Tick order:
     * - retry expired operations
     * - collect next batch to send
     * - optionally prune completed operations
     *
     * @param prune_completed Whether completed outbox entries should be pruned.
     * @return Tick result.
     */
    [[nodiscard]] TickResult tick(bool prune_completed = false)
    {
      TickResult result{};

      result.retried_count = engine_.retry_expired();
      result.batch = engine_.next_batch();

      if (prune_completed)
      {
        result.pruned_count = engine_.prune_completed();
      }

      return result;
    }

    /**
     * @brief Retries expired operations only.
     *
     * @return Number of operations requeued.
     */
    std::size_t retry_only()
    {
      return engine_.retry_expired();
    }

    /**
     * @brief Fetches the next batch only.
     *
     * @return Batch ready for transport.
     */
    [[nodiscard]] std::vector<sync_core::SyncEnvelope> next_batch()
    {
      return engine_.next_batch();
    }

    /**
     * @brief Prunes completed entries only.
     *
     * @return Number of removed entries.
     */
    std::size_t prune_completed()
    {
      return engine_.prune_completed();
    }

    /**
     * @brief Prunes failed entries only.
     *
     * @return Number of removed entries.
     */
    std::size_t prune_failed()
    {
      return engine_.prune_failed();
    }

    /**
     * @brief Returns the underlying sync engine.
     *
     * @return Sync engine reference.
     */
    [[nodiscard]] sync_engine::SyncEngine &engine() noexcept
    {
      return engine_;
    }

    /**
     * @brief Returns the underlying sync engine.
     *
     * @return Sync engine const reference.
     */
    [[nodiscard]] const sync_engine::SyncEngine &engine() const noexcept
    {
      return engine_;
    }

  private:
    sync_engine::SyncEngine &engine_;
  };

} // namespace softadastra::sync::scheduler

#endif // SOFTADASTRA_SYNC_SCHEDULER_HPP
