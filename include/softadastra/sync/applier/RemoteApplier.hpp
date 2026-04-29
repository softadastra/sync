/**
 *
 *  @file RemoteApplier.hpp
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

#ifndef SOFTADASTRA_SYNC_REMOTE_APPLIER_HPP
#define SOFTADASTRA_SYNC_REMOTE_APPLIER_HPP

#include <utility>

#include <softadastra/core/Core.hpp>
#include <softadastra/store/engine/ApplyResult.hpp>
#include <softadastra/store/engine/StoreEngine.hpp>
#include <softadastra/sync/conflict/ConflictResolver.hpp>
#include <softadastra/sync/core/SyncContext.hpp>
#include <softadastra/sync/core/SyncOperation.hpp>

namespace softadastra::sync::applier
{
  namespace store_engine = softadastra::store::engine;
  namespace sync_core = softadastra::sync::core;
  namespace sync_conflict = softadastra::sync::conflict;

  namespace core_types = softadastra::core::types;
  namespace core_errors = softadastra::core::errors;

  /**
   * @brief Applies remote sync operations to the local store.
   *
   * RemoteApplier is responsible for deciding whether an incoming remote
   * operation should be applied to the local StoreEngine.
   *
   * It performs:
   * - context validation
   * - local entry lookup
   * - conflict resolution
   * - remote operation application
   *
   * The store remains responsible for:
   * - preserving the operation timestamp
   * - appending to WAL when enabled
   * - updating in-memory state
   * - assigning the final local version
   *
   * RemoteApplier does not perform networking or acknowledgement handling.
   */
  class RemoteApplier
  {
  public:
    /**
     * @brief Application result for a remote sync operation.
     */
    struct ApplyRemoteResult
    {
      /**
       * @brief True when the remote apply flow completed successfully.
       */
      bool success{false};

      /**
       * @brief True when the remote operation was applied to the store.
       */
      bool applied{false};

      /**
       * @brief True when the remote operation was ignored.
       */
      bool ignored{false};

      /**
       * @brief True when conflict logic was needed.
       */
      bool conflict_detected{false};

      /**
       * @brief Result returned by the underlying StoreEngine.
       */
      store_engine::ApplyResult store_result{};

      /**
       * @brief Creates a successful ignored result.
       *
       * @param conflict Whether a conflict was detected.
       * @return ApplyRemoteResult.
       */
      [[nodiscard]] static constexpr ApplyRemoteResult ignored_result(
          bool conflict) noexcept
      {
        ApplyRemoteResult result{};
        result.success = true;
        result.ignored = true;
        result.conflict_detected = conflict;
        return result;
      }

      /**
       * @brief Creates a successful applied result.
       *
       * @param conflict Whether a conflict was detected.
       * @param apply_result Store apply result.
       * @return ApplyRemoteResult.
       */
      [[nodiscard]] static constexpr ApplyRemoteResult applied_result(
          bool conflict,
          store_engine::ApplyResult apply_result) noexcept
      {
        ApplyRemoteResult result{};
        result.success = true;
        result.applied = true;
        result.conflict_detected = conflict;
        result.store_result = apply_result;
        return result;
      }
    };

    /**
     * @brief Result type returned by apply_remote().
     */
    using Result =
        core_types::Result<ApplyRemoteResult, core_errors::Error>;

    /**
     * @brief Creates a remote applier from a sync context.
     *
     * The context is not owned by the applier.
     *
     * @param context Sync context.
     */
    explicit RemoteApplier(const sync_core::SyncContext &context)
        : context_(context)
    {
    }

    /**
     * @brief Applies a remote sync operation to the local store.
     *
     * If the conflict resolver decides to keep local state, the operation is
     * ignored successfully.
     *
     * @param sync_operation Remote sync operation.
     * @return ApplyRemoteResult on success, Error on failure.
     */
    [[nodiscard]] Result apply_remote(
        const sync_core::SyncOperation &sync_operation) const
    {
      if (!context_.is_valid())
      {
        return Result::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidState,
                "invalid sync context"));
      }

      if (!sync_operation.is_valid())
      {
        return Result::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidArgument,
                "invalid remote sync operation"));
      }

      if (!sync_operation.is_remote())
      {
        return Result::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidArgument,
                "sync operation is not remote-to-local"));
      }

      auto store_result = context_.store_engine_checked();

      if (store_result.is_err())
      {
        return Result::err(store_result.error());
      }

      auto config_result = context_.config_checked();

      if (config_result.is_err())
      {
        return Result::err(config_result.error());
      }

      auto *store = store_result.value();
      const auto *config = config_result.value();

      const auto local_entry =
          store->get(sync_operation.operation.key);

      const auto resolution =
          sync_conflict::ConflictResolver::resolve(
              local_entry,
              sync_operation.operation,
              config->conflict_policy,
              context_.node_id(),
              sync_operation.origin_node_id);

      if (!resolution.is_valid())
      {
        return Result::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidState,
                "invalid conflict resolution result"));
      }

      if (!resolution.apply_remote)
      {
        return Result::ok(
            ApplyRemoteResult::ignored_result(
                resolution.conflict_detected));
      }

      auto applied =
          store->apply_operation(sync_operation.operation);

      if (applied.is_err())
      {
        return Result::err(applied.error());
      }

      return Result::ok(
          ApplyRemoteResult::applied_result(
              resolution.conflict_detected,
              applied.value()));
    }

  private:
    const sync_core::SyncContext &context_;
  };

} // namespace softadastra::sync::applier

#endif // SOFTADASTRA_SYNC_REMOTE_APPLIER_HPP
