/*
 * RemoteApplier.hpp
 */

#ifndef SOFTADASTRA_SYNC_REMOTE_APPLIER_HPP
#define SOFTADASTRA_SYNC_REMOTE_APPLIER_HPP

#include <stdexcept>
#include <string>

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

  /**
   * @brief Applies remote sync operations to the local store
   *
   * The store is responsible for:
   * - preserving the provided operation timestamp
   * - appending to WAL if enabled
   * - updating the in-memory state
   * - returning the assigned version
   */
  class RemoteApplier
  {
  public:
    /**
     * @brief Application result
     */
    struct Result
    {
      bool success{false};
      bool applied{false};
      bool ignored{false};
      bool conflict_detected{false};

      store_engine::ApplyResult store_result;
    };

    explicit RemoteApplier(const sync_core::SyncContext &context)
        : context_(context)
    {
    }

    /**
     * @brief Apply a remote sync operation to the local store
     */
    Result apply_remote(const sync_core::SyncOperation &sync_op) const
    {
      if (!context_.valid())
      {
        throw std::runtime_error("RemoteApplier: invalid SyncContext");
      }

      Result result{};

      auto &store = context_.store_engine_ref();

      const auto local_entry = store.get(sync_op.op.key);

      const auto resolution = sync_conflict::ConflictResolver::resolve(
          local_entry,
          sync_op.op,
          context_.config_ref().conflict_policy,
          context_.node_id(),
          sync_op.origin_node_id);

      result.conflict_detected = resolution.conflict_detected;

      if (!resolution.apply_remote)
      {
        result.success = true;
        result.ignored = true;
        return result;
      }

      result.store_result = store.apply_operation(sync_op.op);
      result.success = result.store_result.success;
      result.applied = result.store_result.success;

      return result;
    }

  private:
    const sync_core::SyncContext &context_;
  };

} // namespace softadastra::sync::applier

#endif
