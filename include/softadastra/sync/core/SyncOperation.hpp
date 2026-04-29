/**
 *
 *  @file SyncOperation.hpp
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

#ifndef SOFTADASTRA_SYNC_OPERATION_HPP
#define SOFTADASTRA_SYNC_OPERATION_HPP

#include <cstdint>
#include <string>
#include <utility>

#include <softadastra/core/Core.hpp>
#include <softadastra/store/core/Operation.hpp>
#include <softadastra/sync/types/SyncDirection.hpp>

namespace softadastra::sync::core
{
  namespace store_core = softadastra::store::core;
  namespace types = softadastra::sync::types;
  namespace core_time = softadastra::core::time;

  /**
   * @brief Synchronizable store operation.
   *
   * SyncOperation wraps a logical store Operation with metadata required by
   * the sync layer to propagate it across nodes.
   *
   * It contains:
   * - a unique sync id
   * - the node that created the operation
   * - a monotonic version
   * - the store operation itself
   * - a sync-level timestamp
   * - a propagation direction
   *
   * Notes:
   * - version is usually the store version returned by ApplyResult::version.
   * - operation.timestamp remains the original store operation timestamp.
   * - timestamp tracks the sync-level creation or propagation time.
   */
  struct SyncOperation
  {
    /**
     * @brief Unique identifier for this sync operation.
     */
    std::string sync_id{};

    /**
     * @brief Node that originally created the operation.
     */
    std::string origin_node_id{};

    /**
     * @brief Monotonic version associated with the operation.
     *
     * In the local case, this usually maps to the store version returned by
     * ApplyResult::version.
     */
    std::uint64_t version{0};

    /**
     * @brief Logical store operation.
     */
    store_core::Operation operation{};

    /**
     * @brief Sync-layer timestamp.
     */
    core_time::Timestamp timestamp{};

    /**
     * @brief Direction of propagation.
     */
    types::SyncDirection direction{types::SyncDirection::Unknown};

    /**
     * @brief Creates an empty invalid sync operation.
     */
    SyncOperation() = default;

    /**
     * @brief Creates a sync operation with explicit fields.
     *
     * @param id Unique sync id.
     * @param origin Node that created the operation.
     * @param operation_version Monotonic operation version.
     * @param store_operation Store operation.
     * @param sync_timestamp Sync-layer timestamp.
     * @param sync_direction Propagation direction.
     */
    SyncOperation(
        std::string id,
        std::string origin,
        std::uint64_t operation_version,
        store_core::Operation store_operation,
        core_time::Timestamp sync_timestamp,
        types::SyncDirection sync_direction)
        : sync_id(std::move(id)),
          origin_node_id(std::move(origin)),
          version(operation_version),
          operation(std::move(store_operation)),
          timestamp(sync_timestamp),
          direction(sync_direction)
    {
    }

    /**
     * @brief Creates a local outbound sync operation.
     *
     * @param id Unique sync id.
     * @param origin Local node id.
     * @param operation_version Store operation version.
     * @param store_operation Store operation.
     * @return Local outbound sync operation.
     */
    [[nodiscard]] static SyncOperation local(
        std::string id,
        std::string origin,
        std::uint64_t operation_version,
        store_core::Operation store_operation)
    {
      return SyncOperation(
          std::move(id),
          std::move(origin),
          operation_version,
          std::move(store_operation),
          core_time::Timestamp::now(),
          types::SyncDirection::LocalToRemote);
    }

    /**
     * @brief Creates a remote inbound sync operation.
     *
     * @param id Unique sync id.
     * @param origin Remote origin node id.
     * @param operation_version Remote operation version.
     * @param store_operation Store operation.
     * @return Remote inbound sync operation.
     */
    [[nodiscard]] static SyncOperation remote(
        std::string id,
        std::string origin,
        std::uint64_t operation_version,
        store_core::Operation store_operation)
    {
      return SyncOperation(
          std::move(id),
          std::move(origin),
          operation_version,
          std::move(store_operation),
          core_time::Timestamp::now(),
          types::SyncDirection::RemoteToLocal);
    }

    /**
     * @brief Returns true if this operation is locally originated.
     *
     * @return true for LocalToRemote.
     */
    [[nodiscard]] bool is_local() const noexcept
    {
      return types::is_outbound(direction);
    }

    /**
     * @brief Returns true if this operation is remotely originated.
     *
     * @return true for RemoteToLocal.
     */
    [[nodiscard]] bool is_remote() const noexcept
    {
      return types::is_inbound(direction);
    }

    /**
     * @brief Returns true if the operation has a usable sync id.
     *
     * @return true when sync_id is not empty.
     */
    [[nodiscard]] bool has_sync_id() const noexcept
    {
      return !sync_id.empty();
    }

    /**
     * @brief Returns true if the operation has an origin node id.
     *
     * @return true when origin_node_id is not empty.
     */
    [[nodiscard]] bool has_origin() const noexcept
    {
      return !origin_node_id.empty();
    }

    /**
     * @brief Returns true if the operation has a non-zero version.
     *
     * @return true when version is greater than zero.
     */
    [[nodiscard]] bool has_version() const noexcept
    {
      return version > 0;
    }

    /**
     * @brief Returns true if this sync operation is structurally valid.
     *
     * @return true when identity, origin, direction, timestamp, and store
     * operation are valid.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return !sync_id.empty() &&
             !origin_node_id.empty() &&
             version > 0 &&
             operation.is_valid() &&
             timestamp.is_valid() &&
             types::is_valid(direction);
    }

    /**
     * @brief Clears the operation and resets it to the default state.
     */
    void clear() noexcept
    {
      sync_id.clear();
      origin_node_id.clear();
      version = 0;
      operation.clear();
      timestamp = core_time::Timestamp{};
      direction = types::SyncDirection::Unknown;
    }
  };

} // namespace softadastra::sync::core

#endif // SOFTADASTRA_SYNC_OPERATION_HPP
