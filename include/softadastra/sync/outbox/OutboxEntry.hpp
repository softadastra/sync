/*
 * OutboxEntry.hpp
 */

#ifndef SOFTADASTRA_SYNC_OUTBOX_ENTRY_HPP
#define SOFTADASTRA_SYNC_OUTBOX_ENTRY_HPP

#include <cstdint>

#include <softadastra/sync/core/SyncEnvelope.hpp>

namespace softadastra::sync::outbox
{
  namespace core = softadastra::sync::core;
  namespace types = softadastra::sync::types;

  /**
   * @brief Entry stored in the sync outbox
   *
   * Wraps a sync envelope and provides convenience helpers
   * for scheduling, acknowledgement, and retry handling.
   */
  struct OutboxEntry
  {
    /**
     * Envelope carried by the outbox
     */
    core::SyncEnvelope envelope;

    /**
     * @brief Return the sync id of the underlying operation
     */
    const std::string &sync_id() const noexcept
    {
      return envelope.operation.sync_id;
    }

    /**
     * @brief Return true if this entry is ready to be scheduled
     */
    bool ready(std::uint64_t now_ms) const noexcept
    {
      return envelope.valid() &&
             envelope.status != types::SyncStatus::Acknowledged &&
             envelope.status != types::SyncStatus::Applied &&
             now_ms >= envelope.next_retry_at;
    }

    /**
     * @brief Return true if this entry is waiting for ack
     */
    bool awaiting_ack() const noexcept
    {
      return envelope.awaiting_ack();
    }

    /**
     * @brief Return true if this entry has been acknowledged
     */
    bool acknowledged() const noexcept
    {
      return envelope.acknowledged();
    }

    /**
     * @brief Return true if the entry may be retried
     */
    bool retryable() const noexcept
    {
      return envelope.retryable();
    }

    /**
     * @brief Basic validity check
     */
    bool valid() const noexcept
    {
      return envelope.valid();
    }
  };

} // namespace softadastra::sync::outbox

#endif
