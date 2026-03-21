/*
 * ConflictResolver.hpp
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
   * @brief Resolves conflicts between local state and remote operations
   *
   * The resolver is deterministic:
   * - timestamp is compared first
   * - if equal, origin node id is used as a tie-breaker
   */
  class ConflictResolver
  {
  public:
    /**
     * @brief Resolution result
     */
    struct Resolution
    {
      bool apply_remote{false};
      bool keep_local{false};
      bool conflict_detected{false};
    };

    /**
     * @brief Resolve a remote operation against an optional local entry
     *
     * @param local_entry Current local entry if it exists
     * @param remote_op   Incoming remote store operation
     * @param policy      Conflict policy to apply
     * @param local_node_id Current local node id
     * @param remote_node_id Origin node id of the remote operation
     */
    static Resolution resolve(
        const std::optional<store_core::Entry> &local_entry,
        const store_core::Operation &remote_op,
        sync_types::ConflictPolicy policy,
        const std::string &local_node_id,
        const std::string &remote_node_id)
    {
      Resolution result{};

      if (!local_entry.has_value())
      {
        result.apply_remote = true;
        return result;
      }

      result.conflict_detected = true;

      switch (policy)
      {
      case sync_types::ConflictPolicy::KeepLocal:
        result.keep_local = true;
        return result;

      case sync_types::ConflictPolicy::KeepRemote:
        result.apply_remote = true;
        return result;

      case sync_types::ConflictPolicy::Manual:
        result.keep_local = true;
        return result;

      case sync_types::ConflictPolicy::LastWriteWins:
      default:
        return resolve_last_write_wins(
            *local_entry,
            remote_op,
            local_node_id,
            remote_node_id);
      }
    }

  private:
    static Resolution resolve_last_write_wins(
        const store_core::Entry &local_entry,
        const store_core::Operation &remote_op,
        const std::string &local_node_id,
        const std::string &remote_node_id)
    {
      Resolution result{};
      result.conflict_detected = true;

      if (remote_op.timestamp > local_entry.timestamp)
      {
        result.apply_remote = true;
        return result;
      }

      if (remote_op.timestamp < local_entry.timestamp)
      {
        result.keep_local = true;
        return result;
      }

      // deterministic tie-breaker
      if (remote_node_id > local_node_id)
      {
        result.apply_remote = true;
      }
      else
      {
        result.keep_local = true;
      }

      return result;
    }
  };

} // namespace softadastra::sync::conflict

#endif
