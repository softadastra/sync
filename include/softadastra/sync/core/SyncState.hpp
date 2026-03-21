/*
 * SyncState.hpp
 */

#ifndef SOFTADASTRA_SYNC_STATE_HPP
#define SOFTADASTRA_SYNC_STATE_HPP

#include <cstddef>
#include <cstdint>

namespace softadastra::sync::core
{
  /**
   * @brief Observable runtime state of the sync engine
   *
   * This is a lightweight snapshot of counters and progress
   * indicators useful for inspection, metrics, and tests.
   */
  struct SyncState
  {
    /**
     * Number of operations currently stored in outbox
     */
    std::size_t outbox_size{0};

    /**
     * Number of operations currently waiting in queue
     */
    std::size_t queued_count{0};

    /**
     * Number of operations waiting for acknowledgement
     */
    std::size_t in_flight_count{0};

    /**
     * Number of successfully acknowledged operations
     */
    std::size_t acknowledged_count{0};

    /**
     * Number of failed operations
     */
    std::size_t failed_count{0};

    /**
     * Last local version submitted to sync
     */
    std::uint64_t last_submitted_version{0};

    /**
     * Last remote version successfully applied
     */
    std::uint64_t last_applied_remote_version{0};

    /**
     * Total number of retry attempts performed
     */
    std::uint64_t total_retries{0};

    /**
     * @brief Reset all counters
     */
    void clear() noexcept
    {
      outbox_size = 0;
      queued_count = 0;
      in_flight_count = 0;
      acknowledged_count = 0;
      failed_count = 0;
      last_submitted_version = 0;
      last_applied_remote_version = 0;
      total_retries = 0;
    }
  };

} // namespace softadastra::sync::core

#endif
