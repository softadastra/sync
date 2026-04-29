/**
 *
 *  @file AckStatus.hpp
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

#ifndef SOFTADASTRA_SYNC_ACK_STATUS_HPP
#define SOFTADASTRA_SYNC_ACK_STATUS_HPP

#include <cstdint>
#include <string_view>

namespace softadastra::sync::types
{
  /**
   * @brief Acknowledgement state of a sync operation.
   *
   * AckStatus tracks whether a sync operation has been acknowledged by a
   * remote peer.
   *
   * It is used by:
   * - AckTracker
   * - OutboxEntry
   * - SyncEngine
   * - retry scheduling
   * - diagnostics
   *
   * Rules:
   * - Values must remain stable over time.
   * - Do not reorder existing values.
   * - Do not remove existing values once released.
   * - Add new values only at the end.
   */
  enum class AckStatus : std::uint8_t
  {
    /**
     * @brief Operation has not been sent yet.
     */
    None = 0,

    /**
     * @brief Operation was sent and is waiting for acknowledgement.
     */
    Waiting,

    /**
     * @brief Remote acknowledgement was received.
     */
    Received,

    /**
     * @brief Operation was sent but no acknowledgement arrived in time.
     */
    TimedOut
  };

  /**
   * @brief Returns a stable string representation of an acknowledgement status.
   *
   * This is intended for logs, diagnostics, and text serialization.
   *
   * @param status Acknowledgement status.
   * @return Stable string representation.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(AckStatus status) noexcept
  {
    switch (status)
    {
    case AckStatus::None:
      return "none";

    case AckStatus::Waiting:
      return "waiting";

    case AckStatus::Received:
      return "received";

    case AckStatus::TimedOut:
      return "timed_out";

    default:
      return "invalid";
    }
  }

  /**
   * @brief Returns true if the acknowledgement status is known.
   *
   * @param status Acknowledgement status.
   * @return true when the status is valid.
   */
  [[nodiscard]] constexpr bool is_valid(AckStatus status) noexcept
  {
    return status == AckStatus::None ||
           status == AckStatus::Waiting ||
           status == AckStatus::Received ||
           status == AckStatus::TimedOut;
  }

  /**
   * @brief Returns true if the acknowledgement reached a terminal state.
   *
   * @param status Acknowledgement status.
   * @return true for Received and TimedOut.
   */
  [[nodiscard]] constexpr bool is_terminal(AckStatus status) noexcept
  {
    return status == AckStatus::Received ||
           status == AckStatus::TimedOut;
  }

  /**
   * @brief Returns true if the operation is currently waiting for ack.
   *
   * @param status Acknowledgement status.
   * @return true for Waiting.
   */
  [[nodiscard]] constexpr bool is_waiting(AckStatus status) noexcept
  {
    return status == AckStatus::Waiting;
  }

} // namespace softadastra::sync::types

#endif // SOFTADASTRA_SYNC_ACK_STATUS_HPP
