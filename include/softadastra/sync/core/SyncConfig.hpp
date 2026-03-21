/*
 * SyncConfig.hpp
 */

#ifndef SOFTADASTRA_SYNC_CONFIG_HPP
#define SOFTADASTRA_SYNC_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include <softadastra/sync/types/ConflictPolicy.hpp>

namespace softadastra::sync::core
{
  namespace types = softadastra::sync::types;

  /**
   * @brief Sync engine configuration
   */
  struct SyncConfig
  {
    /**
     * Local node identifier
     */
    std::string node_id{"node-1"};

    /**
     * Maximum number of operations returned in one batch
     */
    std::size_t batch_size{64};

    /**
     * Maximum number of retries before giving up
     */
    std::uint32_t max_retries{5};

    /**
     * Delay before retrying a failed or timed out operation
     * in milliseconds
     */
    std::uint64_t retry_interval_ms{5000};

    /**
     * Timeout for waiting an acknowledgement
     * in milliseconds
     */
    std::uint64_t ack_timeout_ms{10000};

    /**
     * Conflict resolution strategy
     */
    types::ConflictPolicy conflict_policy{
        types::ConflictPolicy::LastWriteWins};

    /**
     * Automatically queue local operations after submission
     */
    bool auto_queue{true};

    /**
     * Automatically track acknowledgement state
     */
    bool require_ack{true};
  };

} // namespace softadastra::sync::core

#endif
