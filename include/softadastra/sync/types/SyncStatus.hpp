/**
 *
 *  @file SyncStatus.hpp
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

#ifndef SOFTADASTRA_SYNC_STATUS_HPP
#define SOFTADASTRA_SYNC_STATUS_HPP

#include <cstdint>
#include <string_view>

namespace softadastra::sync::types
{
  /**
   * @brief Lifecycle status of a sync operation.
   *
   * SyncStatus describes where an operation currently is in the
   * synchronization pipeline.
   *
   * It is used by:
   * - SyncOperation
   * - OutboxEntry
   * - SyncQueue
   * - SyncEngine
   * - SyncScheduler
   * - diagnostics
   *
   * Rules:
   * - Values must remain stable over time.
   * - Do not reorder existing values.
   * - Do not remove existing values once released.
   * - Add new values only at the end.
   */
  enum class SyncStatus : std::uint8_t
  {
    /**
     * @brief Unknown or invalid status.
     */
    Unknown = 0,

    /**
     * @brief Operation was created but is not queued yet.
     */
    Pending,

    /**
     * @brief Operation is ready to be sent.
     */
    Queued,

    /**
     * @brief Operation was handed off for sending and is awaiting completion.
     */
    InFlight,

    /**
     * @brief Remote side confirmed receipt.
     */
    Acknowledged,

    /**
     * @brief Operation was applied successfully on the target side.
     */
    Applied,

    /**
     * @brief Operation failed and may require retry or inspection.
     */
    Failed
  };

  /**
   * @brief Returns a stable string representation of a sync status.
   *
   * This is intended for logs, diagnostics, and text serialization.
   *
   * @param status Sync status.
   * @return Stable string representation.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(SyncStatus status) noexcept
  {
    switch (status)
    {
    case SyncStatus::Unknown:
      return "unknown";

    case SyncStatus::Pending:
      return "pending";

    case SyncStatus::Queued:
      return "queued";

    case SyncStatus::InFlight:
      return "in_flight";

    case SyncStatus::Acknowledged:
      return "acknowledged";

    case SyncStatus::Applied:
      return "applied";

    case SyncStatus::Failed:
      return "failed";

    default:
      return "invalid";
    }
  }

  /**
   * @brief Returns true if the sync status is known and usable.
   *
   * Unknown is intentionally treated as invalid.
   *
   * @param status Sync status.
   * @return true for all usable statuses.
   */
  [[nodiscard]] constexpr bool is_valid(SyncStatus status) noexcept
  {
    return status == SyncStatus::Pending ||
           status == SyncStatus::Queued ||
           status == SyncStatus::InFlight ||
           status == SyncStatus::Acknowledged ||
           status == SyncStatus::Applied ||
           status == SyncStatus::Failed;
  }

  /**
   * @brief Returns true if the status means the operation is still active.
   *
   * @param status Sync status.
   * @return true for Pending, Queued, and InFlight.
   */
  [[nodiscard]] constexpr bool is_active(SyncStatus status) noexcept
  {
    return status == SyncStatus::Pending ||
           status == SyncStatus::Queued ||
           status == SyncStatus::InFlight;
  }

  /**
   * @brief Returns true if the operation reached a terminal status.
   *
   * @param status Sync status.
   * @return true for Applied and Failed.
   */
  [[nodiscard]] constexpr bool is_terminal(SyncStatus status) noexcept
  {
    return status == SyncStatus::Applied ||
           status == SyncStatus::Failed;
  }

  /**
   * @brief Returns true if the operation can be retried.
   *
   * @param status Sync status.
   * @return true for Failed.
   */
  [[nodiscard]] constexpr bool can_retry(SyncStatus status) noexcept
  {
    return status == SyncStatus::Failed;
  }

} // namespace softadastra::sync::types

#endif // SOFTADASTRA_SYNC_STATUS_HPP
