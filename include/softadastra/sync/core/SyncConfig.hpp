/**
 *
 *  @file SyncConfig.hpp
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

#ifndef SOFTADASTRA_SYNC_CONFIG_HPP
#define SOFTADASTRA_SYNC_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include <softadastra/core/Core.hpp>
#include <softadastra/sync/types/ConflictPolicy.hpp>

namespace softadastra::sync::core
{
  namespace types = softadastra::sync::types;
  namespace core_time = softadastra::core::time;

  /**
   * @brief Configuration for the Softadastra sync engine.
   *
   * SyncConfig keeps the sync layer predictable and easy to initialize.
   *
   * It controls:
   * - local node identity
   * - batching
   * - retry behavior
   * - acknowledgement timeout
   * - conflict resolution policy
   * - automatic queuing
   * - acknowledgement tracking
   *
   * The sync module does not own transport by itself. Transport is expected
   * to be provided by higher-level modules or adapters.
   */
  struct SyncConfig
  {
    /**
     * @brief Local node identifier.
     *
     * This value must be unique per device or peer.
     */
    std::string node_id{"node-1"};

    /**
     * @brief Maximum number of operations returned in one batch.
     */
    std::size_t batch_size{64};

    /**
     * @brief Maximum number of send/apply retries before giving up.
     */
    std::uint32_t max_retries{5};

    /**
     * @brief Delay before retrying a failed or timed-out operation.
     */
    core_time::Duration retry_interval{
        core_time::Duration::from_seconds(5)};

    /**
     * @brief Maximum time to wait for an acknowledgement.
     */
    core_time::Duration ack_timeout{
        core_time::Duration::from_seconds(10)};

    /**
     * @brief Conflict resolution strategy.
     */
    types::ConflictPolicy conflict_policy{
        types::ConflictPolicy::LastWriteWins};

    /**
     * @brief Automatically queue local operations after submission.
     */
    bool auto_queue{true};

    /**
     * @brief Track acknowledgement state for sent operations.
     */
    bool require_ack{true};

    /**
     * @brief Creates a default sync configuration.
     */
    SyncConfig() = default;

    /**
     * @brief Creates a sync configuration for a node id.
     *
     * @param local_node_id Local node identifier.
     */
    explicit SyncConfig(std::string local_node_id)
        : node_id(std::move(local_node_id))
    {
    }

    /**
     * @brief Creates a production-oriented sync configuration.
     *
     * @param local_node_id Local node identifier.
     * @return Sync configuration with durable defaults.
     */
    [[nodiscard]] static SyncConfig durable(std::string local_node_id)
    {
      SyncConfig config(std::move(local_node_id));
      config.batch_size = 64;
      config.max_retries = 5;
      config.retry_interval = core_time::Duration::from_seconds(5);
      config.ack_timeout = core_time::Duration::from_seconds(10);
      config.conflict_policy = types::ConflictPolicy::LastWriteWins;
      config.auto_queue = true;
      config.require_ack = true;
      return config;
    }

    /**
     * @brief Creates a fast configuration for tests or local demos.
     *
     * @param local_node_id Local node identifier.
     * @return Sync configuration with shorter retry and ack windows.
     */
    [[nodiscard]] static SyncConfig fast(std::string local_node_id)
    {
      SyncConfig config(std::move(local_node_id));
      config.batch_size = 128;
      config.max_retries = 2;
      config.retry_interval = core_time::Duration::from_millis(250);
      config.ack_timeout = core_time::Duration::from_seconds(1);
      config.conflict_policy = types::ConflictPolicy::LastWriteWins;
      config.auto_queue = true;
      config.require_ack = false;
      return config;
    }

    /**
     * @brief Returns the retry interval in milliseconds.
     *
     * @return Retry interval in milliseconds.
     */
    [[nodiscard]] core_time::Duration::rep
    retry_interval_ms() const noexcept
    {
      return retry_interval.millis();
    }

    /**
     * @brief Returns the acknowledgement timeout in milliseconds.
     *
     * @return Ack timeout in milliseconds.
     */
    [[nodiscard]] core_time::Duration::rep
    ack_timeout_ms() const noexcept
    {
      return ack_timeout.millis();
    }

    /**
     * @brief Returns true if the configuration is usable.
     *
     * @return true when node id, batch size, durations, and policy are valid.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return !node_id.empty() &&
             batch_size > 0 &&
             retry_interval.millis() >= 0 &&
             ack_timeout.millis() >= 0 &&
             types::is_valid(conflict_policy);
    }
  };

} // namespace softadastra::sync::core

#endif // SOFTADASTRA_SYNC_CONFIG_HPP
