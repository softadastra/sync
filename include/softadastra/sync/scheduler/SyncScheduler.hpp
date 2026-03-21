/*
 * SyncScheduler.hpp
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
   * @brief Lightweight scheduler for driving the sync engine
   *
   * This first version is intentionally manual and deterministic.
   * It does not create threads or timers.
   *
   * Responsibilities:
   * - trigger retries for expired acknowledgements
   * - fetch the next batch ready for transport
   * - optionally prune completed outbox entries
   */
  class SyncScheduler
  {
  public:
    /**
     * @brief Result of one scheduler tick
     */
    struct TickResult
    {
      std::size_t retried_count{0};
      std::size_t pruned_count{0};
      std::vector<sync_core::SyncEnvelope> batch;
    };

    explicit SyncScheduler(sync_engine::SyncEngine &engine)
        : engine_(engine)
    {
    }

    /**
     * @brief Perform one scheduler cycle
     *
     * Order:
     * 1. retry expired operations
     * 2. collect next batch to send
     * 3. optionally prune completed operations
     */
    TickResult tick(bool prune_completed = false)
    {
      TickResult result;

      result.retried_count = engine_.retry_expired();
      result.batch = engine_.next_batch();

      if (prune_completed)
      {
        result.pruned_count = engine_.prune_completed();
      }

      return result;
    }

    /**
     * @brief Retry expired operations only
     */
    std::size_t retry_only()
    {
      return engine_.retry_expired();
    }

    /**
     * @brief Fetch the next batch only
     */
    std::vector<sync_core::SyncEnvelope> next_batch()
    {
      return engine_.next_batch();
    }

    /**
     * @brief Prune completed entries only
     */
    std::size_t prune_only()
    {
      return engine_.prune_completed();
    }

  private:
    sync_engine::SyncEngine &engine_;
  };

} // namespace softadastra::sync::scheduler

#endif
