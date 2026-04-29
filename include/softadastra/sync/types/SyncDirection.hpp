/**
 *
 *  @file SyncDirection.hpp
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

#ifndef SOFTADASTRA_SYNC_DIRECTION_HPP
#define SOFTADASTRA_SYNC_DIRECTION_HPP

#include <cstdint>
#include <string_view>

namespace softadastra::sync::types
{
  /**
   * @brief Direction of a sync operation.
   *
   * SyncDirection identifies where an operation comes from and where it is
   * expected to go.
   *
   * It is used by:
   * - SyncOperation
   * - SyncEnvelope
   * - SyncEngine
   * - Outbox
   * - RemoteApplier
   *
   * Rules:
   * - Values must remain stable over time.
   * - Do not reorder existing values.
   * - Do not remove existing values once released.
   * - Add new values only at the end.
   */
  enum class SyncDirection : std::uint8_t
  {
    /**
     * @brief Unknown or invalid direction.
     */
    Unknown = 0,

    /**
     * @brief Operation originated locally and should be sent to a remote peer.
     */
    LocalToRemote,

    /**
     * @brief Operation came from a remote peer and should be applied locally.
     */
    RemoteToLocal
  };

  /**
   * @brief Returns a stable string representation of a sync direction.
   *
   * This is intended for logs, diagnostics, and text serialization.
   *
   * @param direction Sync direction.
   * @return Stable string representation.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(SyncDirection direction) noexcept
  {
    switch (direction)
    {
    case SyncDirection::Unknown:
      return "unknown";

    case SyncDirection::LocalToRemote:
      return "local_to_remote";

    case SyncDirection::RemoteToLocal:
      return "remote_to_local";

    default:
      return "invalid";
    }
  }

  /**
   * @brief Returns true if the sync direction is known and usable.
   *
   * Unknown is intentionally treated as invalid.
   *
   * @param direction Sync direction.
   * @return true for LocalToRemote and RemoteToLocal.
   */
  [[nodiscard]] constexpr bool is_valid(SyncDirection direction) noexcept
  {
    return direction == SyncDirection::LocalToRemote ||
           direction == SyncDirection::RemoteToLocal;
  }

  /**
   * @brief Returns true if the operation should be sent outward.
   *
   * @param direction Sync direction.
   * @return true for LocalToRemote.
   */
  [[nodiscard]] constexpr bool is_outbound(SyncDirection direction) noexcept
  {
    return direction == SyncDirection::LocalToRemote;
  }

  /**
   * @brief Returns true if the operation should be applied locally.
   *
   * @param direction Sync direction.
   * @return true for RemoteToLocal.
   */
  [[nodiscard]] constexpr bool is_inbound(SyncDirection direction) noexcept
  {
    return direction == SyncDirection::RemoteToLocal;
  }

} // namespace softadastra::sync::types

#endif // SOFTADASTRA_SYNC_DIRECTION_HPP
