/**
 *
 *  @file SyncContext.hpp
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

#ifndef SOFTADASTRA_SYNC_CONTEXT_HPP
#define SOFTADASTRA_SYNC_CONTEXT_HPP

#include <string>

#include <softadastra/core/Core.hpp>
#include <softadastra/store/engine/StoreEngine.hpp>
#include <softadastra/sync/core/SyncConfig.hpp>

namespace softadastra::sync::core
{
  namespace store_engine = softadastra::store::engine;
  namespace core_errors = softadastra::core::errors;
  namespace core_types = softadastra::core::types;

  /**
   * @brief Runtime dependency context for sync components.
   *
   * SyncContext groups the shared dependencies required by the sync module.
   *
   * It provides access to:
   * - the local StoreEngine
   * - the active SyncConfig
   *
   * It is used by:
   * - SyncEngine
   * - RemoteApplier
   * - ConflictResolver
   * - scheduler and retry flows
   *
   * The context does not own the store or config.
   * The caller must ensure both objects outlive the sync components using them.
   */
  struct SyncContext
  {
    /**
     * @brief Store engine used to apply logical operations.
     *
     * The store is responsible for WAL persistence, replay, and version
     * assignment.
     */
    store_engine::StoreEngine *store{nullptr};

    /**
     * @brief Sync configuration.
     */
    const SyncConfig *config{nullptr};

    /**
     * @brief Result type returned by checked accessors.
     */
    template <typename T>
    using Result = core_types::Result<T, core_errors::Error>;

    /**
     * @brief Creates an empty invalid context.
     */
    SyncContext() = default;

    /**
     * @brief Creates a sync context from dependencies.
     *
     * @param store_engine Store engine reference.
     * @param sync_config Sync configuration reference.
     */
    SyncContext(
        store_engine::StoreEngine &store_engine,
        const SyncConfig &sync_config) noexcept
        : store(&store_engine),
          config(&sync_config)
    {
    }

    /**
     * @brief Returns true if the context is fully usable.
     *
     * @return true when store and config are present and config is valid.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return store != nullptr &&
             config != nullptr &&
             config->is_valid();
    }

    /**
     * @brief Returns the configured local node id.
     *
     * If the context is invalid, an empty string is returned.
     *
     * @return Local node id.
     */
    [[nodiscard]] const std::string &node_id() const noexcept
    {
      static const std::string empty;

      if (config == nullptr)
      {
        return empty;
      }

      return config->node_id;
    }

    /**
     * @brief Returns the configured store engine pointer.
     *
     * @return Store engine pointer, or nullptr.
     */
    [[nodiscard]] store_engine::StoreEngine *
    store_engine_ptr() const noexcept
    {
      return store;
    }

    /**
     * @brief Returns the sync configuration pointer.
     *
     * @return Sync config pointer, or nullptr.
     */
    [[nodiscard]] const SyncConfig *
    config_ptr() const noexcept
    {
      return config;
    }

    /**
     * @brief Returns the configured store engine as a Result.
     *
     * @return StoreEngine reference on success, Error on failure.
     */
    [[nodiscard]] Result<store_engine::StoreEngine *>
    store_engine_checked() const
    {
      if (store == nullptr)
      {
        return Result<store_engine::StoreEngine *>::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidState,
                "sync context store is null"));
      }

      return Result<store_engine::StoreEngine *>::ok(store);
    }

    /**
     * @brief Returns the sync configuration as a Result.
     *
     * @return SyncConfig pointer on success, Error on failure.
     */
    [[nodiscard]] Result<const SyncConfig *>
    config_checked() const
    {
      if (config == nullptr)
      {
        return Result<const SyncConfig *>::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidState,
                "sync context config is null"));
      }

      if (!config->is_valid())
      {
        return Result<const SyncConfig *>::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidArgument,
                "sync config is invalid"));
      }

      return Result<const SyncConfig *>::ok(config);
    }

    /**
     * @brief Returns true if a store engine is present.
     *
     * @return true when store is not null.
     */
    [[nodiscard]] bool has_store() const noexcept
    {
      return store != nullptr;
    }

    /**
     * @brief Returns true if a sync configuration is present.
     *
     * @return true when config is not null.
     */
    [[nodiscard]] bool has_config() const noexcept
    {
      return config != nullptr;
    }
  };

} // namespace softadastra::sync::core

#endif // SOFTADASTRA_SYNC_CONTEXT_HPP
