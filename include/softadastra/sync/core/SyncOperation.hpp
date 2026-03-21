/*
 * SyncOperation.hpp
 */

#ifndef SOFTADASTRA_SYNC_OPERATION_HPP
#define SOFTADASTRA_SYNC_OPERATION_HPP

#include <cstdint>
#include <string>

#include <softadastra/store/core/Operation.hpp>
#include <softadastra/sync/types/SyncDirection.hpp>

namespace softadastra::sync::core
{
  namespace store_core = softadastra::store::core;
  namespace types = softadastra::sync::types;

  /**
   * @brief Synchronizable store operation
   *
   * Wraps a logical store operation with the metadata required
   * by the sync layer to propagate it across nodes.
   *
   * Notes:
   * - version is typically the local WAL-backed version returned
   *   by StoreEngine after apply.
   * - timestamp is the sync-level timestamp for tracking/order.
   * - op.timestamp remains the original operation timestamp.
   */
  struct SyncOperation
  {
    /**
     * Unique identifier for this sync operation
     */
    std::string sync_id;

    /**
     * Node that originally created the operation
     */
    std::string origin_node_id;

    /**
     * Monotonic version associated with the operation
     *
     * In the local case, this is expected to map to the
     * store version returned by ApplyResult.version.
     */
    std::uint64_t version{0};

    /**
     * Logical store operation
     */
    store_core::Operation op;

    /**
     * Sync-layer timestamp
     */
    std::uint64_t timestamp{0};

    /**
     * Direction of propagation
     */
    types::SyncDirection direction{types::SyncDirection::Unknown};

    /**
     * @brief Check whether this operation is locally originated
     */
    bool is_local() const noexcept
    {
      return direction == types::SyncDirection::LocalToRemote;
    }

    /**
     * @brief Check whether this operation is remotely originated
     */
    bool is_remote() const noexcept
    {
      return direction == types::SyncDirection::RemoteToLocal;
    }

    /**
     * @brief Check whether this operation has a usable identity
     */
    bool valid() const noexcept
    {
      return !sync_id.empty() &&
             !origin_node_id.empty() &&
             direction != types::SyncDirection::Unknown;
    }
  };

} // namespace softadastra::sync::core

#endif
