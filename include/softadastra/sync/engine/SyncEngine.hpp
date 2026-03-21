/*
 * SyncEngine.hpp
 */

#ifndef SOFTADASTRA_SYNC_ENGINE_HPP
#define SOFTADASTRA_SYNC_ENGINE_HPP

#include <cstdint>
#include <ctime>
#include <stdexcept>
#include <vector>

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

  /**
   * @brief Central orchestration engine for synchronization
   *
   * Responsibilities:
   * - submit local operations
   * - prepare operations for transport
   * - track acknowledgements
   * - apply remote operations
   * - retry timed out operations
   */
  class SyncEngine
  {
  public:
    explicit SyncEngine(const sync_core::SyncContext &context)
        : context_(context),
          id_generator_(context.node_id()),
          remote_applier_(context)
    {
      if (!context_.valid())
      {
        throw std::runtime_error("SyncEngine: invalid SyncContext");
      }
    }

    /**
     * @brief Return local node identifier
     */
    const std::string &node_id() const
    {
      return context_.node_id();
    }

    /**
     * @brief Submit a local store operation into the sync pipeline
     *
     * The operation is first applied to the local store so it gets:
     * - WAL durability
     * - a version
     * - in-memory application
     *
     * Then it is wrapped as a SyncOperation and queued for propagation.
     */
    sync_core::SyncOperation submit_local_operation(const store_core::Operation &op)
    {
      auto &store = context_.store_engine_ref();

      const store_engine::ApplyResult apply_result = store.apply_operation(op);

      sync_core::SyncOperation sync_op;
      sync_op.sync_id = id_generator_.next();
      sync_op.origin_node_id = context_.node_id();
      sync_op.version = apply_result.version;
      sync_op.op = op;
      sync_op.timestamp = now_ms();
      sync_op.direction = sync_types::SyncDirection::LocalToRemote;

      sync_core::SyncEnvelope envelope;
      envelope.operation = sync_op;
      envelope.status = context_.config_ref().auto_queue
                            ? sync_types::SyncStatus::Queued
                            : sync_types::SyncStatus::Pending;
      envelope.ack_status = sync_types::AckStatus::None;
      envelope.retry_count = 0;
      envelope.last_attempt_at = 0;
      envelope.next_retry_at = 0;

      sync_outbox::OutboxEntry entry;
      entry.envelope = envelope;

      outbox_.push(std::move(entry));

      if (context_.config_ref().auto_queue)
      {
        queue_.push(envelope);
      }

      state_.last_submitted_version = sync_op.version;
      refresh_state();

      return sync_op;
    }

    /**
     * @brief Queue a previously submitted outbox entry
     */
    bool queue_operation(const std::string &sync_id)
    {
      const auto entry = outbox_.find(sync_id);
      if (!entry.has_value())
      {
        return false;
      }

      if (!outbox_.mark_queued(sync_id))
      {
        return false;
      }

      auto envelope = entry->envelope;
      envelope.status = sync_types::SyncStatus::Queued;
      queue_.push(std::move(envelope));

      refresh_state();
      return true;
    }

    /**
     * @brief Return the next batch of operations to send
     *
     * Returned operations are marked in-flight and tracked for ack.
     */
    std::vector<sync_core::SyncEnvelope> next_batch()
    {
      const std::size_t max_count = context_.config_ref().batch_size;
      std::vector<sync_core::SyncEnvelope> batch = queue_.pop_batch(max_count);

      const std::uint64_t now = now_ms();

      for (auto &envelope : batch)
      {
        outbox_.mark_in_flight(
            envelope.operation.sync_id,
            now,
            context_.config_ref().ack_timeout_ms);

        envelope.status = sync_types::SyncStatus::InFlight;
        envelope.ack_status = context_.config_ref().require_ack
                                  ? sync_types::AckStatus::Waiting
                                  : sync_types::AckStatus::Received;
        envelope.last_attempt_at = now;
        envelope.next_retry_at = now + context_.config_ref().ack_timeout_ms;
        ++envelope.retry_count;

        if (context_.config_ref().require_ack)
        {
          ack_tracker_.track(
              envelope.operation.sync_id,
              now,
              context_.config_ref().ack_timeout_ms);
        }
      }

      refresh_state();
      return batch;
    }

    /**
     * @brief Receive acknowledgement for one operation
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
     * @brief Apply a remote sync operation locally
     */
    sync_applier::RemoteApplier::Result receive_remote_operation(
        const sync_core::SyncOperation &sync_op)
    {
      auto result = remote_applier_.apply_remote(sync_op);

      if (result.applied && result.store_result.success)
      {
        state_.last_applied_remote_version = result.store_result.version;
      }

      refresh_state();
      return result;
    }

    /**
     * @brief Retry expired in-flight operations
     *
     * Returns the number of operations requeued.
     */
    std::size_t retry_expired()
    {
      const std::uint64_t now = now_ms();

      const auto expired = ack_tracker_.collect_expired(now);

      std::size_t requeued = 0;

      for (const auto &entry : expired)
      {
        ack_tracker_.erase(entry.sync_id);

        if (outbox_.mark_failed(
                entry.sync_id,
                now,
                context_.config_ref().retry_interval_ms))
        {
          const auto found = outbox_.find(entry.sync_id);
          if (found.has_value())
          {
            auto envelope = found->envelope;

            if (envelope.retry_count < context_.config_ref().max_retries)
            {
              envelope.status = sync_types::SyncStatus::Queued;
              envelope.ack_status = sync_types::AckStatus::TimedOut;
              envelope.next_retry_at = now + context_.config_ref().retry_interval_ms;

              outbox_.mark_queued(entry.sync_id);
              queue_.push(std::move(envelope));

              ++requeued;
              ++state_.total_retries;
            }
          }
        }
      }

      refresh_state();
      return requeued;
    }

    /**
     * @brief Remove completed operations from the outbox
     */
    std::size_t prune_completed()
    {
      const std::size_t removed = outbox_.prune_completed();
      refresh_state();
      return removed;
    }

    /**
     * @brief Current observable sync state
     */
    const sync_core::SyncState &state() const noexcept
    {
      return state_;
    }

    /**
     * @brief Read-only access to outbox
     */
    const sync_outbox::Outbox &outbox() const noexcept
    {
      return outbox_;
    }

    /**
     * @brief Read-only access to queue
     */
    const sync_queue::SyncQueue &queue() const noexcept
    {
      return queue_;
    }

    /**
     * @brief Read-only access to ack tracker
     */
    const sync_ack::AckTracker &ack_tracker() const noexcept
    {
      return ack_tracker_;
    }

  private:
    void refresh_state()
    {
      state_.outbox_size = outbox_.size();
      state_.queued_count = queue_.size();
      state_.in_flight_count = ack_tracker_.size();
      state_.acknowledged_count = count_acknowledged();
      state_.failed_count = count_failed();
    }

    std::size_t count_acknowledged() const
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

    std::size_t count_failed() const
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

    static std::uint64_t now_ms()
    {
      return static_cast<std::uint64_t>(std::time(nullptr)) * 1000ULL;
    }

  private:
    const sync_core::SyncContext &context_;

    sync_utils::SyncIdGenerator id_generator_;
    sync_outbox::Outbox outbox_;
    sync_queue::SyncQueue queue_;
    sync_ack::AckTracker ack_tracker_;
    sync_applier::RemoteApplier remote_applier_;
    sync_core::SyncState state_;
  };

} // namespace softadastra::sync::engine

#endif
