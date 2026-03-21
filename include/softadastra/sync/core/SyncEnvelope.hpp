/*
 * SyncEnvelope.hpp
 */

#ifndef SOFTADASTRA_SYNC_ENVELOPE_HPP
#define SOFTADASTRA_SYNC_ENVELOPE_HPP

#include <cstdint>

#include <softadastra/sync/core/SyncOperation.hpp>
#include <softadastra/sync/types/SyncStatus.hpp>
#include <softadastra/sync/types/AckStatus.hpp>

namespace softadastra::sync::core
{
  namespace types = softadastra::sync::types;

  /**
   * @brief Runtime wrapper around a sync operation
   *
   * Adds pipeline state, acknowledgement tracking,
   * retry counters, and scheduling metadata.
   */
  struct SyncEnvelope
  {
    /**
     * Synchronizable operation
     */
    SyncOperation operation;

    /**
     * Current sync pipeline status
     */
    types::SyncStatus status{types::SyncStatus::Pending};

    /**
     * Acknowledgement state
     */
    types::AckStatus ack_status{types::AckStatus::None};

    /**
     * Number of send attempts
     */
    std::uint32_t retry_count{0};

    /**
     * Timestamp of the last send attempt
     */
    std::uint64_t last_attempt_at{0};

    /**
     * Next time this operation becomes eligible
     * for retry or scheduling
     */
    std::uint64_t next_retry_at{0};

    /**
     * @brief True if the operation is waiting for ack
     */
    bool awaiting_ack() const noexcept
    {
      return ack_status == types::AckStatus::Waiting;
    }

    /**
     * @brief True if the operation was acknowledged
     */
    bool acknowledged() const noexcept
    {
      return ack_status == types::AckStatus::Received;
    }

    /**
     * @brief True if this envelope can be retried
     */
    bool retryable() const noexcept
    {
      return status == types::SyncStatus::Failed ||
             ack_status == types::AckStatus::TimedOut;
    }

    /**
     * @brief Basic validity check
     */
    bool valid() const noexcept
    {
      return operation.valid();
    }
  };

} // namespace softadastra::sync::core

#endif
