/**
 *
 *  @file OutboxEntry.hpp
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

#ifndef SOFTADASTRA_SYNC_OUTBOX_ENTRY_HPP
#define SOFTADASTRA_SYNC_OUTBOX_ENTRY_HPP

#include <string>

#include <softadastra/core/Core.hpp>
#include <softadastra/sync/core/SyncEnvelope.hpp>
#include <softadastra/sync/types/AckStatus.hpp>
#include <softadastra/sync/types/SyncStatus.hpp>

namespace softadastra::sync::outbox
{
  namespace core = softadastra::sync::core;
  namespace types = softadastra::sync::types;
  namespace core_time = softadastra::core::time;

  /**
   * @brief Entry stored in the sync outbox.
   *
   * OutboxEntry wraps a SyncEnvelope and provides small convenience helpers
   * for scheduling, acknowledgement, retry handling, and lifecycle checks.
   *
   * It is used by:
   * - Outbox
   * - SyncEngine
   * - SyncScheduler
   * - retry logic
   *
   * The outbox is the local source of pending outbound sync work.
   * The queue only decides send order.
   */
  struct OutboxEntry
  {
    /**
     * @brief Envelope carried by the outbox.
     */
    core::SyncEnvelope envelope{};

    /**
     * @brief Creates an empty invalid outbox entry.
     */
    OutboxEntry() = default;

    /**
     * @brief Creates an outbox entry from an envelope.
     *
     * @param sync_envelope Sync envelope.
     */
    explicit OutboxEntry(core::SyncEnvelope sync_envelope)
        : envelope(std::move(sync_envelope))
    {
    }

    /**
     * @brief Returns the sync id of the underlying operation.
     *
     * @return Sync operation id.
     */
    [[nodiscard]] const std::string &sync_id() const noexcept
    {
      return envelope.operation.sync_id;
    }

    /**
     * @brief Returns true if this entry is ready to be scheduled.
     *
     * Ready means:
     * - the envelope is valid
     * - the status is not terminal
     * - the ack status is not already received
     * - the retry time has been reached
     *
     * @param now Current timestamp.
     * @return true if ready for scheduling.
     */
    [[nodiscard]] bool ready(core_time::Timestamp now) const noexcept
    {
      if (!now.is_valid())
      {
        return false;
      }

      if (!is_valid())
      {
        return false;
      }

      if (types::is_terminal(envelope.status))
      {
        return false;
      }

      if (envelope.ack_status == types::AckStatus::Received)
      {
        return false;
      }

      if (!envelope.next_retry_at.is_valid())
      {
        return true;
      }

      return !(now < envelope.next_retry_at);
    }

    /**
     * @brief Returns true if this entry is ready using the current time.
     *
     * @return true if ready for scheduling.
     */
    [[nodiscard]] bool ready() const noexcept
    {
      return ready(core_time::Timestamp::now());
    }

    /**
     * @brief Returns true if this entry is waiting for acknowledgement.
     *
     * @return true when waiting for ack.
     */
    [[nodiscard]] bool awaiting_ack() const noexcept
    {
      return envelope.awaiting_ack();
    }

    /**
     * @brief Returns true if this entry has been acknowledged.
     *
     * @return true when ack was received.
     */
    [[nodiscard]] bool acknowledged() const noexcept
    {
      return envelope.acknowledged();
    }

    /**
     * @brief Returns true if this entry reached a terminal sync status.
     *
     * @return true when applied or failed.
     */
    [[nodiscard]] bool terminal() const noexcept
    {
      return envelope.terminal();
    }

    /**
     * @brief Returns true if the entry may be retried.
     *
     * @return true when failed or timed out.
     */
    [[nodiscard]] bool retryable() const noexcept
    {
      return envelope.retryable();
    }

    /**
     * @brief Returns true if this entry can be sent now.
     *
     * This delegates basic send readiness to the envelope.
     *
     * @return true if ready to send.
     */
    [[nodiscard]] bool ready_to_send() const noexcept
    {
      return envelope.ready_to_send();
    }

    /**
     * @brief Marks this entry as queued.
     */
    void mark_queued() noexcept
    {
      envelope.mark_queued();
    }

    /**
     * @brief Marks this entry as in-flight.
     *
     * @param require_ack Whether acknowledgement should be tracked.
     */
    void mark_in_flight(bool require_ack = true) noexcept
    {
      envelope.mark_in_flight(require_ack);
    }

    /**
     * @brief Marks this entry as acknowledged.
     */
    void mark_acknowledged() noexcept
    {
      envelope.mark_acknowledged();
    }

    /**
     * @brief Marks this entry as applied.
     */
    void mark_applied() noexcept
    {
      envelope.mark_applied();
    }

    /**
     * @brief Marks this entry as failed.
     */
    void mark_failed() noexcept
    {
      envelope.mark_failed();
    }

    /**
     * @brief Marks this entry as timed out.
     */
    void mark_timed_out() noexcept
    {
      envelope.mark_timed_out();
    }

    /**
     * @brief Schedules the next retry time.
     *
     * @param timestamp Next retry timestamp.
     */
    void schedule_retry_at(core_time::Timestamp timestamp) noexcept
    {
      envelope.schedule_retry_at(timestamp);
    }

    /**
     * @brief Returns true if the entry is structurally valid.
     *
     * @return true when the underlying envelope is valid.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return envelope.is_valid();
    }

    /**
     * @brief Clears the entry.
     */
    void clear() noexcept
    {
      envelope.clear();
    }
  };

} // namespace softadastra::sync::outbox

#endif // SOFTADASTRA_SYNC_OUTBOX_ENTRY_HPP
