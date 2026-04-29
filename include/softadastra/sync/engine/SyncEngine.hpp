/**
 *
 *  @file SyncEngine.hpp
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

#ifndef SOFTADASTRA_SYNC_ENGINE_HPP
#define SOFTADASTRA_SYNC_ENGINE_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <softadastra/core/Core.hpp>
#include <softadastra/store/core/Operation.hpp>
#include <softadastra/store/engine/ApplyResult.hpp>
#include <softadastra/sync/ack/AckTracker.hpp>
#include <softadastra/sync/applier/RemoteApplier.hpp>
#include <softadastra/sync/core/SyncConfig.hpp>
#include <softadastra/sync/core/SyncContext.hpp>
#include <softadastra/sync/core/SyncEnvelope.hpp>
#include <softadastra/sync/core/SyncOperation.hpp>
#include <softadastra/sync/core/SyncState.hpp>
#include <softadastra/sync/outbox/Outbox.hpp>
#include <softadastra/sync/outbox/OutboxEntry.hpp>
#include <softadastra/sync/queue/SyncQueue.hpp>
#include <softadastra/sync/types/AckStatus.hpp>
#include <softadastra/sync/types/SyncDirection.hpp>
#include <softadastra/sync/types/SyncStatus.hpp>
#include <softadastra/sync/utils/SyncIdGenerator.hpp>

namespace softadastra::sync::engine
{
  namespace store_core = softadastra::store::core;
  namespace store_engine = softadastra::store::engine;

  namespace sync_ack = softadastra::sync::ack;
  namespace sync_applier = softadastra::sync::applier;
  namespace sync_core = softadastra::sync::core;
  namespace sync_outbox = softadastra::sync::outbox;
  namespace sync_queue = softadastra::sync::queue;
  namespace sync_types = softadastra::sync::types;
  namespace sync_utils = softadastra::sync::utils;

  namespace core_types = softadastra::core::types;
  namespace core_errors = softadastra::core::errors;
  namespace core_time = softadastra::core::time;

  /**
   * @brief Central orchestration engine for local-first synchronization.
   *
   * SyncEngine coordinates the local sync pipeline.
   *
   * Responsibilities:
   * - submit local store operations
   * - wrap local operations into sync operations
   * - queue operations for transport
   * - return batches ready to send
   * - track acknowledgements
   * - apply remote operations
   * - retry timed-out operations
   * - expose observable sync state
   *
   * SyncEngine does not implement transport.
   * Callers are responsible for sending envelopes returned by next_batch().
   */
  class SyncEngine : public core_types::NonCopyable
  {
  public:
    /**
     * @brief Result returned when submitting local operations.
     */
    using SubmitResult =
        core_types::Result<sync_core::SyncOperation, core_errors::Error>;

    /**
     * @brief Result returned when applying remote operations.
     */
    using RemoteResult = sync_applier::RemoteApplier::Result;

    /**
     * @brief Creates a sync engine from a runtime context.
     *
     * The context is not owned by the engine.
     *
     * @param context Sync runtime context.
     */
    explicit SyncEngine(const sync_core::SyncContext &context)
        : context_(context),
          id_generator_(context.node_id()),
          remote_applier_(context)
    {
      refresh_state();
    }

    /**
     * @brief Moves a sync engine.
     */
    SyncEngine(SyncEngine &&) noexcept = default;

    /**
     * @brief Move-assigns a sync engine.
     */
    SyncEngine &operator=(SyncEngine &&) noexcept = default;

    /**
     * @brief Returns the local node identifier.
     *
     * @return Local node id.
     */
    [[nodiscard]] const std::string &node_id() const noexcept
    {
      return context_.node_id();
    }

    /**
     * @brief Returns true if the engine context is usable.
     *
     * @return true when context is valid.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return context_.is_valid() &&
             id_generator_.is_valid();
    }

    /**
     * @brief Submits a local store operation into the sync pipeline.
     *
     * Flow:
     * - apply the operation to the local store
     * - receive the store/WAL-backed version
     * - create a SyncOperation
     * - store it in the outbox
     * - queue it automatically if configured
     *
     * @param operation Local store operation.
     * @return SyncOperation on success, Error on failure.
     */
    [[nodiscard]] SubmitResult submit_local_operation(
        const store_core::Operation &operation)
    {
      if (!context_.is_valid())
      {
        return SubmitResult::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidState,
                "invalid sync context"));
      }

      if (!operation.is_valid())
      {
        return SubmitResult::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidArgument,
                "invalid local store operation"));
      }

      auto store_result = context_.store_engine_checked();

      if (store_result.is_err())
      {
        return SubmitResult::err(store_result.error());
      }

      auto config_result = context_.config_checked();

      if (config_result.is_err())
      {
        return SubmitResult::err(config_result.error());
      }

      auto *store = store_result.value();
      const auto *config = config_result.value();

      auto applied = store->apply_operation(operation);

      if (applied.is_err())
      {
        return SubmitResult::err(applied.error());
      }

      const auto apply_result = applied.value();

      auto sync_id = id_generator_.next();

      if (sync_id.empty())
      {
        return SubmitResult::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidState,
                "failed to generate sync id"));
      }

      auto sync_operation =
          sync_core::SyncOperation::local(
              std::move(sync_id),
              config->node_id,
              apply_result.version,
              operation);

      if (!sync_operation.is_valid())
      {
        return SubmitResult::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidState,
                "failed to build sync operation"));
      }

      sync_core::SyncEnvelope envelope{sync_operation};

      if (config->auto_queue)
      {
        envelope.mark_queued();
      }

      sync_outbox::OutboxEntry entry{envelope};
      outbox_.upsert(std::move(entry));

      if (config->auto_queue)
      {
        queue_.upsert(std::move(envelope));
      }

      state_.last_submitted_version = sync_operation.version;

      refresh_state();

      return SubmitResult::ok(std::move(sync_operation));
    }

    /**
     * @brief Queues a previously submitted outbox entry.
     *
     * @param sync_id Sync operation id.
     * @return true if the entry was found and queued.
     */
    bool queue_operation(const std::string &sync_id)
    {
      auto found = outbox_.find(sync_id);

      if (!found.has_value())
      {
        return false;
      }

      if (!outbox_.mark_queued(sync_id))
      {
        return false;
      }

      auto envelope = found->envelope;
      envelope.mark_queued();

      queue_.upsert(std::move(envelope));

      refresh_state();

      return true;
    }

    /**
     * @brief Returns the next batch of envelopes ready for transport.
     *
     * Returned envelopes are marked in-flight in the outbox and optionally
     * tracked for acknowledgement.
     *
     * @return Batch of sync envelopes.
     */
    [[nodiscard]] std::vector<sync_core::SyncEnvelope> next_batch()
    {
      auto config_result = context_.config_checked();

      if (config_result.is_err())
      {
        refresh_state();
        return {};
      }

      const auto *config = config_result.value();

      auto batch = queue_.pop_batch(config->batch_size);

      for (auto &envelope : batch)
      {
        const auto &sync_id = envelope.operation.sync_id;

        outbox_.mark_in_flight(sync_id, config->require_ack);

        envelope.mark_in_flight(config->require_ack);

        if (config->require_ack)
        {
          ack_tracker_.track(
              sync_id,
              config->ack_timeout);
        }
        else
        {
          outbox_.mark_acked(sync_id);
          outbox_.mark_applied(sync_id);
          envelope.mark_acknowledged();
          envelope.mark_applied();
        }
      }

      refresh_state();

      return batch;
    }

    /**
     * @brief Receives acknowledgement for one operation.
     *
     * @param sync_id Sync operation id.
     * @return true if the acknowledgement matched a known operation.
     */
    bool receive_ack(const std::string &sync_id)
    {
      const bool acked_in_tracker = ack_tracker_.ack(sync_id);
      const bool acked_in_outbox = outbox_.mark_acked(sync_id);

      ack_tracker_.erase(sync_id);

      if (acked_in_outbox)
      {
        outbox_.mark_applied(sync_id);
      }

      refresh_state();

      return acked_in_tracker || acked_in_outbox;
    }

    /**
     * @brief Applies a remote sync operation locally.
     *
     * @param sync_operation Remote sync operation.
     * @return Apply result on success, Error on failure.
     */
    [[nodiscard]] RemoteResult receive_remote_operation(
        const sync_core::SyncOperation &sync_operation)
    {
      auto result = remote_applier_.apply_remote(sync_operation);

      if (result.is_ok())
      {
        const auto &value = result.value();

        if (value.applied && value.store_result.success)
        {
          state_.last_applied_remote_version =
              value.store_result.version;
        }
      }

      refresh_state();

      return result;
    }

    /**
     * @brief Retries expired in-flight operations.
     *
     * Operations waiting for ack past their timeout are requeued when their
     * retry count is still below max_retries.
     *
     * @return Number of operations requeued.
     */
    std::size_t retry_expired()
    {
      auto config_result = context_.config_checked();

      if (config_result.is_err())
      {
        refresh_state();
        return 0;
      }

      const auto *config = config_result.value();

      const auto now = core_time::Timestamp::now();
      const auto expired = ack_tracker_.collect_expired(now);

      std::size_t requeued = 0;

      for (const auto &ack_entry : expired)
      {
        ack_tracker_.erase(ack_entry.sync_id);

        auto found = outbox_.find(ack_entry.sync_id);

        if (!found.has_value())
        {
          continue;
        }

        auto envelope = found->envelope;

        if (envelope.retry_count >= config->max_retries)
        {
          outbox_.mark_failed(
              ack_entry.sync_id,
              now,
              config->retry_interval);

          continue;
        }

        envelope.mark_timed_out();
        envelope.mark_queued();

        envelope.schedule_retry_at(
            core_time::Timestamp::from_millis(
                now.millis() + config->retry_interval.millis()));

        outbox_.upsert(sync_outbox::OutboxEntry{envelope});
        queue_.upsert(std::move(envelope));

        ++requeued;
        ++state_.total_retries;
      }

      refresh_state();

      return requeued;
    }

    /**
     * @brief Removes completed operations from the outbox.
     *
     * @return Number of removed entries.
     */
    std::size_t prune_completed()
    {
      const std::size_t removed = outbox_.prune_completed();
      refresh_state();
      return removed;
    }

    /**
     * @brief Removes failed operations from the outbox.
     *
     * @return Number of removed entries.
     */
    std::size_t prune_failed()
    {
      const std::size_t removed = outbox_.prune_failed();
      refresh_state();
      return removed;
    }

    /**
     * @brief Rebuilds the observable sync state.
     */
    void refresh_state()
    {
      state_.outbox_size = outbox_.size();
      state_.queued_count = queue_.size();
      state_.in_flight_count = count_in_flight();
      state_.acknowledged_count = count_acknowledged();
      state_.failed_count = count_failed();
    }

    /**
     * @brief Returns the current observable sync state.
     *
     * @return Sync state.
     */
    [[nodiscard]] const sync_core::SyncState &state() const noexcept
    {
      return state_;
    }

    /**
     * @brief Returns read-only access to the outbox.
     *
     * @return Outbox.
     */
    [[nodiscard]] const sync_outbox::Outbox &outbox() const noexcept
    {
      return outbox_;
    }

    /**
     * @brief Returns read-only access to the queue.
     *
     * @return Sync queue.
     */
    [[nodiscard]] const sync_queue::SyncQueue &queue() const noexcept
    {
      return queue_;
    }

    /**
     * @brief Returns read-only access to the acknowledgement tracker.
     *
     * @return Ack tracker.
     */
    [[nodiscard]] const sync_ack::AckTracker &ack_tracker() const noexcept
    {
      return ack_tracker_;
    }

  private:
    /**
     * @brief Counts in-flight outbox entries.
     */
    [[nodiscard]] std::size_t count_in_flight() const
    {
      std::size_t count = 0;

      for (const auto &entry : outbox_.entries())
      {
        if (entry.envelope.status == sync_types::SyncStatus::InFlight)
        {
          ++count;
        }
      }

      return count;
    }

    /**
     * @brief Counts acknowledged or applied outbox entries.
     */
    [[nodiscard]] std::size_t count_acknowledged() const
    {
      std::size_t count = 0;

      for (const auto &entry : outbox_.entries())
      {
        if (entry.envelope.status == sync_types::SyncStatus::Acknowledged ||
            entry.envelope.status == sync_types::SyncStatus::Applied)
        {
          ++count;
        }
      }

      return count;
    }

    /**
     * @brief Counts failed outbox entries.
     */
    [[nodiscard]] std::size_t count_failed() const
    {
      std::size_t count = 0;

      for (const auto &entry : outbox_.entries())
      {
        if (entry.envelope.status == sync_types::SyncStatus::Failed)
        {
          ++count;
        }
      }

      return count;
    }

  private:
    const sync_core::SyncContext &context_;

    sync_utils::SyncIdGenerator id_generator_{};
    sync_outbox::Outbox outbox_{};
    sync_queue::SyncQueue queue_{};
    sync_ack::AckTracker ack_tracker_{};
    sync_applier::RemoteApplier remote_applier_;
    sync_core::SyncState state_{};
  };

} // namespace softadastra::sync::engine

#endif // SOFTADASTRA_SYNC_ENGINE_HPP
