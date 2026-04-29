/**
 *
 *  @file SyncState.hpp
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

#ifndef SOFTADASTRA_SYNC_STATE_HPP
#define SOFTADASTRA_SYNC_STATE_HPP

#include <cstddef>
#include <cstdint>

namespace softadastra::sync::core
{
  /**
   * @brief Observable runtime state of the sync engine.
   *
   * SyncState is a lightweight snapshot of counters and progress indicators.
   *
   * It is used by:
   * - SyncEngine
   * - tests
   * - diagnostics
   * - metrics
   * - monitoring
   *
   * It does not own operations and does not mutate the sync pipeline by itself.
   */
  struct SyncState
  {
    /**
     * @brief Number of operations currently stored in the outbox.
     */
    std::size_t outbox_size{0};

    /**
     * @brief Number of operations currently waiting in the queue.
     */
    std::size_t queued_count{0};

    /**
     * @brief Number of operations waiting for acknowledgement.
     */
    std::size_t in_flight_count{0};

    /**
     * @brief Number of successfully acknowledged operations.
     */
    std::size_t acknowledged_count{0};

    /**
     * @brief Number of failed operations.
     */
    std::size_t failed_count{0};

    /**
     * @brief Last local version submitted to sync.
     */
    std::uint64_t last_submitted_version{0};

    /**
     * @brief Last remote version successfully applied.
     */
    std::uint64_t last_applied_remote_version{0};

    /**
     * @brief Total number of retry attempts performed.
     */
    std::uint64_t total_retries{0};

    /**
     * @brief Creates an empty sync state.
     */
    SyncState() = default;

    /**
     * @brief Returns true if there are queued operations.
     *
     * @return true when queued_count is greater than zero.
     */
    [[nodiscard]] bool has_queued() const noexcept
    {
      return queued_count > 0;
    }

    /**
     * @brief Returns true if there are in-flight operations.
     *
     * @return true when in_flight_count is greater than zero.
     */
    [[nodiscard]] bool has_in_flight() const noexcept
    {
      return in_flight_count > 0;
    }

    /**
     * @brief Returns true if there are failed operations.
     *
     * @return true when failed_count is greater than zero.
     */
    [[nodiscard]] bool has_failed() const noexcept
    {
      return failed_count > 0;
    }

    /**
     * @brief Returns true if the sync pipeline has pending work.
     *
     * @return true when queued, in-flight, or failed operations exist.
     */
    [[nodiscard]] bool has_work() const noexcept
    {
      return queued_count > 0 ||
             in_flight_count > 0 ||
             failed_count > 0;
    }

    /**
     * @brief Returns true if no operation is tracked.
     *
     * @return true when all counters are zero.
     */
    [[nodiscard]] bool empty() const noexcept
    {
      return outbox_size == 0 &&
             queued_count == 0 &&
             in_flight_count == 0 &&
             acknowledged_count == 0 &&
             failed_count == 0 &&
             last_submitted_version == 0 &&
             last_applied_remote_version == 0 &&
             total_retries == 0;
    }

    /**
     * @brief Returns the total number of tracked operation states.
     *
     * @return Sum of queued, in-flight, acknowledged, and failed counts.
     */
    [[nodiscard]] std::size_t total_tracked() const noexcept
    {
      return queued_count +
             in_flight_count +
             acknowledged_count +
             failed_count;
    }

    /**
     * @brief Resets all counters and progress markers.
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

#endif // SOFTADASTRA_SYNC_STATE_HPP
