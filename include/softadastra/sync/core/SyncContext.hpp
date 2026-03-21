/*
 * SyncContext.hpp
 */

#ifndef SOFTADASTRA_SYNC_CONTEXT_HPP
#define SOFTADASTRA_SYNC_CONTEXT_HPP

#include <stdexcept>
#include <string>

#include <softadastra/store/engine/StoreEngine.hpp>
#include <softadastra/sync/core/SyncConfig.hpp>

namespace softadastra::sync::core
{
  namespace store_engine = softadastra::store::engine;

  /**
   * @brief Runtime dependency context for the sync module
   *
   * Groups the shared dependencies required by sync components,
   * especially the store engine and the sync configuration.
   *
   * This keeps components such as SyncEngine, RemoteApplier,
   * and ConflictResolver decoupled from construction details.
   */
  struct SyncContext
  {
    /**
     * Store engine used to apply logical operations.
     *
     * The store itself is responsible for WAL persistence,
     * replay, and version assignment.
     */
    store_engine::StoreEngine *store{nullptr};

    /**
     * Sync configuration
     */
    const SyncConfig *config{nullptr};

    /**
     * @brief Check whether the context is fully usable
     */
    bool valid() const noexcept
    {
      return store != nullptr &&
             config != nullptr &&
             !config->node_id.empty();
    }

    /**
     * @brief Return the configured local node id
     */
    const std::string &node_id() const
    {
      if (config == nullptr)
      {
        throw std::runtime_error(
            "SyncContext: config is null");
      }

      return config->node_id;
    }

    /**
     * @brief Return the configured store engine
     */
    store_engine::StoreEngine &store_engine_ref() const
    {
      if (store == nullptr)
      {
        throw std::runtime_error(
            "SyncContext: store is null");
      }

      return *store;
    }

    /**
     * @brief Return the sync configuration
     */
    const SyncConfig &config_ref() const
    {
      if (config == nullptr)
      {
        throw std::runtime_error(
            "SyncContext: config is null");
      }

      return *config;
    }
  };

} // namespace softadastra::sync::core

#endif
