/**
 *
 *  @file ConflictResolver.hpp
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

#ifndef SOFTADASTRA_SYNC_CONFLICT_RESOLVER_HPP
#define SOFTADASTRA_SYNC_CONFLICT_RESOLVER_HPP

#include <optional>
#include <string>

#include <softadastra/store/core/Entry.hpp>
#include <softadastra/store/core/Operation.hpp>
#include <softadastra/sync/types/ConflictPolicy.hpp>

namespace softadastra::sync::conflict
{
  namespace store_core = softadastra::store::core;
  namespace sync_types = softadastra::sync::types;

  /**
   * @brief Deterministic conflict resolver for sync operations.
   *
   * ConflictResolver decides whether an incoming remote operation should be
   * applied or whether the current local state should be kept.
   *
   * It is used by:
   * - RemoteApplier
   * - SyncEngine
   * - merge flows
   * - tests
   *
   * Supported policies:
   * - LastWriteWins
   * - KeepLocal
   * - KeepRemote
   * - Manual
   *
   * LastWriteWins rule:
   * - compare timestamps first
   * - if timestamps are equal, compare origin node ids
   * - the greater node id wins as deterministic tie-breaker
   */
  class ConflictResolver
  {
  public:
    /**
     * @brief Conflict resolution decision.
     *
     * Exactly one of apply_remote or keep_local should be true for a valid
     * resolution.
     */
    struct Resolution
    {
      /**
       * @brief True when the remote operation should be applied.
       */
      bool apply_remote{false};

      /**
       * @brief True when the local state should be kept.
       */
      bool keep_local{false};

      /**
       * @brief True when a local entry existed and conflict logic was needed.
       */
      bool conflict_detected{false};

      /**
       * @brief Creates a decision that applies the remote operation.
       *
       * @param conflict Whether a conflict was detected.
       * @return Resolution.
       */
      [[nodiscard]] static constexpr Resolution apply(
          bool conflict = false) noexcept
      {
        return Resolution{
            true,
            false,
            conflict};
      }

      /**
       * @brief Creates a decision that keeps the local state.
       *
       * @param conflict Whether a conflict was detected.
       * @return Resolution.
       */
      [[nodiscard]] static constexpr Resolution keep(
          bool conflict = true) noexcept
      {
        return Resolution{
            false,
            true,
            conflict};
      }

      /**
       * @brief Returns true if this decision is internally consistent.
       *
       * @return true when exactly one decision flag is set.
       */
      [[nodiscard]] constexpr bool is_valid() const noexcept
      {
        return apply_remote != keep_local;
      }
    };

    /**
     * @brief Resolves a remote operation against optional local state.
     *
     * If no local entry exists, the remote operation is accepted.
     *
     * @param local_entry Current local entry if it exists.
     * @param remote_operation Incoming remote operation.
     * @param policy Conflict policy to apply.
     * @param local_node_id Current local node id.
     * @param remote_node_id Origin node id of the remote operation.
     * @return Resolution decision.
     */
    [[nodiscard]] static Resolution resolve(
        const std::optional<store_core::Entry> &local_entry,
        const store_core::Operation &remote_operation,
        sync_types::ConflictPolicy policy,
        const std::string &local_node_id,
        const std::string &remote_node_id)
    {
      if (!remote_operation.is_valid())
      {
        return Resolution::keep(false);
      }

      if (!local_entry.has_value())
      {
        return Resolution::apply(false);
      }

      if (!sync_types::is_valid(policy))
      {
        return Resolution::keep(true);
      }

      switch (policy)
      {
      case sync_types::ConflictPolicy::KeepLocal:
        return Resolution::keep(true);

      case sync_types::ConflictPolicy::KeepRemote:
        return Resolution::apply(true);

      case sync_types::ConflictPolicy::Manual:
        return Resolution::keep(true);

      case sync_types::ConflictPolicy::LastWriteWins:
        return resolve_last_write_wins(
            *local_entry,
            remote_operation,
            local_node_id,
            remote_node_id);

      default:
        return Resolution::keep(true);
      }
    }

  private:
    /**
     * @brief Resolves a conflict using last-write-wins.
     *
     * Timestamp is compared first. If timestamps are equal, remote_node_id and
     * local_node_id are compared to ensure deterministic convergence.
     */
    [[nodiscard]] static Resolution resolve_last_write_wins(
        const store_core::Entry &local_entry,
        const store_core::Operation &remote_operation,
        const std::string &local_node_id,
        const std::string &remote_node_id)
    {
      if (remote_operation.timestamp > local_entry.timestamp)
      {
        return Resolution::apply(true);
      }

      if (remote_operation.timestamp < local_entry.timestamp)
      {
        return Resolution::keep(true);
      }

      if (remote_node_id > local_node_id)
      {
        return Resolution::apply(true);
      }

      return Resolution::keep(true);
    }
  };

} // namespace softadastra::sync::conflict

#endif // SOFTADASTRA_SYNC_CONFLICT_RESOLVER_HPP
