/**
 *
 *  @file SyncEnvelope.hpp
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

#ifndef SOFTADASTRA_SYNC_ENVELOPE_HPP
#define SOFTADASTRA_SYNC_ENVELOPE_HPP

#include <cstdint>
#include <utility>

#include <softadastra/core/Core.hpp>
#include <softadastra/sync/core/SyncOperation.hpp>
#include <softadastra/sync/types/AckStatus.hpp>
#include <softadastra/sync/types/SyncStatus.hpp>

namespace softadastra::sync::core
{
  namespace types = softadastra::sync::types;
  namespace core_time = softadastra::core::time;

  /**
   * @brief Runtime wrapper around a sync operation.
   *
   * SyncEnvelope adds pipeline state around a SyncOperation.
   *
   * It tracks:
   * - current sync status
   * - acknowledgement status
   * - retry count
   * - last attempt time
   * - next retry time
   *
   * It is used by:
   * - Outbox
   * - SyncQueue
   * - AckTracker
   * - SyncScheduler
   * - SyncEngine
   *
   * The envelope does not own transport logic.
   * It only describes where an operation is in the local sync pipeline.
   */
  struct SyncEnvelope
  {
    /**
     * @brief Synchronizable operation.
     */
    SyncOperation operation{};

    /**
     * @brief Current sync pipeline status.
     */
    types::SyncStatus status{types::SyncStatus::Pending};

    /**
     * @brief Current acknowledgement state.
     */
    types::AckStatus ack_status{types::AckStatus::None};

    /**
     * @brief Number of send or apply retry attempts.
     */
    std::uint32_t retry_count{0};

    /**
     * @brief Timestamp of the last send or apply attempt.
     */
    core_time::Timestamp last_attempt_at{};

    /**
     * @brief Next time this operation becomes eligible for retry.
     */
    core_time::Timestamp next_retry_at{};

    /**
     * @brief Creates an empty invalid envelope.
     */
    SyncEnvelope() = default;

    /**
     * @brief Creates an envelope from a sync operation.
     *
     * @param sync_operation Synchronizable operation.
     */
    explicit SyncEnvelope(SyncOperation sync_operation)
        : operation(std::move(sync_operation))
    {
    }

    /**
     * @brief Creates an envelope with explicit status fields.
     *
     * @param sync_operation Synchronizable operation.
     * @param sync_status Pipeline status.
     * @param acknowledgement_status Ack status.
     */
    SyncEnvelope(
        SyncOperation sync_operation,
        types::SyncStatus sync_status,
        types::AckStatus acknowledgement_status)
        : operation(std::move(sync_operation)),
          status(sync_status),
          ack_status(acknowledgement_status)
    {
    }

    /**
     * @brief Returns true if the operation is waiting for acknowledgement.
     *
     * @return true when ack status is Waiting.
     */
    [[nodiscard]] bool awaiting_ack() const noexcept
    {
      return types::is_waiting(ack_status);
    }

    /**
     * @brief Returns true if the operation was acknowledged.
     *
     * @return true when ack status is Received.
     */
    [[nodiscard]] bool acknowledged() const noexcept
    {
      return ack_status == types::AckStatus::Received;
    }

    /**
     * @brief Returns true if the envelope reached a terminal sync status.
     *
     * @return true when status is Applied or Failed.
     */
    [[nodiscard]] bool terminal() const noexcept
    {
      return types::is_terminal(status);
    }

    /**
     * @brief Returns true if the envelope can be retried.
     *
     * @return true for failed or timed-out operations.
     */
    [[nodiscard]] bool retryable() const noexcept
    {
      return types::can_retry(status) ||
             ack_status == types::AckStatus::TimedOut;
    }

    /**
     * @brief Returns true if the operation can be sent now.
     *
     * This only checks status and ack state, not wall-clock retry eligibility.
     *
     * @return true when status is Queued and not waiting for ack.
     */
    [[nodiscard]] bool ready_to_send() const noexcept
    {
      return status == types::SyncStatus::Queued &&
             ack_status != types::AckStatus::Waiting;
    }

    /**
     * @brief Marks the envelope as queued.
     */
    void mark_queued() noexcept
    {
      status = types::SyncStatus::Queued;
      ack_status = types::AckStatus::None;
    }

    /**
     * @brief Marks the envelope as in-flight.
     *
     * This updates retry count and last attempt timestamp.
     *
     * @param require_ack Whether this operation should wait for ack.
     */
    void mark_in_flight(bool require_ack = true) noexcept
    {
      status = types::SyncStatus::InFlight;
      ack_status = require_ack
                       ? types::AckStatus::Waiting
                       : types::AckStatus::None;

      ++retry_count;
      last_attempt_at = core_time::Timestamp::now();
    }

    /**
     * @brief Marks the envelope as acknowledged.
     */
    void mark_acknowledged() noexcept
    {
      status = types::SyncStatus::Acknowledged;
      ack_status = types::AckStatus::Received;
    }

    /**
     * @brief Marks the envelope as applied.
     */
    void mark_applied() noexcept
    {
      status = types::SyncStatus::Applied;
    }

    /**
     * @brief Marks the envelope as failed.
     */
    void mark_failed() noexcept
    {
      status = types::SyncStatus::Failed;
    }

    /**
     * @brief Marks the acknowledgement as timed out.
     */
    void mark_timed_out() noexcept
    {
      ack_status = types::AckStatus::TimedOut;
      status = types::SyncStatus::Failed;
    }

    /**
     * @brief Schedules the next retry timestamp.
     *
     * @param timestamp Next retry timestamp.
     */
    void schedule_retry_at(core_time::Timestamp timestamp) noexcept
    {
      next_retry_at = timestamp;
    }

    /**
     * @brief Returns true if this envelope is structurally valid.
     *
     * @return true when operation, status, and ack status are valid.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return operation.is_valid() &&
             types::is_valid(status) &&
             types::is_valid(ack_status);
    }

    /**
     * @brief Clears the envelope.
     */
    void clear() noexcept
    {
      operation.clear();
      status = types::SyncStatus::Pending;
      ack_status = types::AckStatus::None;
      retry_count = 0;
      last_attempt_at = core_time::Timestamp{};
      next_retry_at = core_time::Timestamp{};
    }
  };

} // namespace softadastra::sync::core

#endif // SOFTADASTRA_SYNC_ENVELOPE_HPP
