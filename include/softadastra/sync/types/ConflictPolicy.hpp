/**
 *
 *  @file ConflictPolicy.hpp
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

#ifndef SOFTADASTRA_SYNC_CONFLICT_POLICY_HPP
#define SOFTADASTRA_SYNC_CONFLICT_POLICY_HPP

#include <cstdint>
#include <string_view>

namespace softadastra::sync::types
{
  /**
   * @brief Conflict resolution strategy used by the sync layer.
   *
   * ConflictPolicy defines how the system should resolve conflicts between
   * local and remote operations that target the same logical state.
   *
   * It is used by:
   * - ConflictResolver
   * - SyncEngine
   * - RemoteApplier
   * - replay and merge flows
   *
   * Rules:
   * - Values must remain stable over time.
   * - Do not reorder existing values.
   * - Do not remove existing values once released.
   * - Add new values only at the end.
   */
  enum class ConflictPolicy : std::uint8_t
  {
    /**
     * @brief Unknown or invalid conflict policy.
     */
    Unknown = 0,

    /**
     * @brief Keep the operation with the newest timestamp or version.
     */
    LastWriteWins,

    /**
     * @brief Always keep the local state when a conflict happens.
     */
    KeepLocal,

    /**
     * @brief Always accept the remote state when a conflict happens.
     */
    KeepRemote,

    /**
     * @brief Defer resolution to a manual or higher-level resolver.
     */
    Manual
  };

  /**
   * @brief Returns a stable string representation of a conflict policy.
   *
   * This is intended for logs, diagnostics, and text serialization.
   *
   * @param policy Conflict policy.
   * @return Stable string representation.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(ConflictPolicy policy) noexcept
  {
    switch (policy)
    {
    case ConflictPolicy::Unknown:
      return "unknown";

    case ConflictPolicy::LastWriteWins:
      return "last_write_wins";

    case ConflictPolicy::KeepLocal:
      return "keep_local";

    case ConflictPolicy::KeepRemote:
      return "keep_remote";

    case ConflictPolicy::Manual:
      return "manual";

    default:
      return "invalid";
    }
  }

  /**
   * @brief Returns true if the conflict policy is known and usable.
   *
   * Unknown is intentionally treated as invalid.
   *
   * @param policy Conflict policy.
   * @return true for all usable policies.
   */
  [[nodiscard]] constexpr bool is_valid(ConflictPolicy policy) noexcept
  {
    return policy == ConflictPolicy::LastWriteWins ||
           policy == ConflictPolicy::KeepLocal ||
           policy == ConflictPolicy::KeepRemote ||
           policy == ConflictPolicy::Manual;
  }

  /**
   * @brief Returns true if the conflict policy can resolve automatically.
   *
   * Manual is not automatic because it requires higher-level intervention.
   *
   * @param policy Conflict policy.
   * @return true for automatic policies.
   */
  [[nodiscard]] constexpr bool is_automatic(ConflictPolicy policy) noexcept
  {
    return policy == ConflictPolicy::LastWriteWins ||
           policy == ConflictPolicy::KeepLocal ||
           policy == ConflictPolicy::KeepRemote;
  }

} // namespace softadastra::sync::types

#endif // SOFTADASTRA_SYNC_CONFLICT_POLICY_HPP
