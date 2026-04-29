/**
 *
 *  @file SyncIdGenerator.hpp
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

#ifndef SOFTADASTRA_SYNC_ID_GENERATOR_HPP
#define SOFTADASTRA_SYNC_ID_GENERATOR_HPP

#include <atomic>
#include <cstdint>
#include <string>
#include <utility>

namespace softadastra::sync::utils
{
  /**
   * @brief Thread-safe sync operation identifier generator.
   *
   * SyncIdGenerator creates stable sync identifiers using the format:
   *
   * @code
   * <node_id>-<counter>
   * @endcode
   *
   * This is suitable for local-first and offline-first systems as long as
   * each node id is unique.
   *
   * It is used by:
   * - SyncOperation
   * - SyncEnvelope
   * - Outbox
   * - SyncEngine
   * - diagnostics
   *
   * Rules:
   * - node_id must not be empty.
   * - counter increases monotonically.
   * - generated ids are stable strings.
   * - the generator does not persist the counter by itself.
   */
  class SyncIdGenerator
  {
  public:
    /**
     * @brief Underlying counter type.
     */
    using value_type = std::uint64_t;

    /**
     * @brief Creates an empty generator.
     *
     * The generator is not valid until a node id is assigned.
     */
    SyncIdGenerator() = default;

    /**
     * @brief Creates a generator bound to a node id.
     *
     * @param node_id Unique local node identifier.
     */
    explicit SyncIdGenerator(std::string node_id)
        : node_id_(std::move(node_id))
    {
    }

    SyncIdGenerator(const SyncIdGenerator &) = delete;
    SyncIdGenerator &operator=(const SyncIdGenerator &) = delete;

    /**
     * @brief Move-constructs a generator.
     *
     * The counter value is copied atomically from the source.
     */
    SyncIdGenerator(SyncIdGenerator &&other) noexcept
        : node_id_(std::move(other.node_id_)),
          counter_(other.current())
    {
    }

    /**
     * @brief Move-assigns a generator.
     *
     * The counter value is copied atomically from the source.
     */
    SyncIdGenerator &operator=(SyncIdGenerator &&other) noexcept
    {
      if (this != &other)
      {
        node_id_ = std::move(other.node_id_);
        reset(other.current());
      }

      return *this;
    }

    /**
     * @brief Generates the next sync identifier.
     *
     * If the generator is invalid, an empty string is returned.
     *
     * @return Generated sync id.
     */
    [[nodiscard]] std::string next()
    {
      if (!is_valid())
      {
        return {};
      }

      const value_type value =
          counter_.fetch_add(1, std::memory_order_relaxed) + 1;

      return node_id_ + "-" + std::to_string(value);
    }

    /**
     * @brief Returns the associated node id.
     *
     * @return Node id.
     */
    [[nodiscard]] const std::string &node_id() const noexcept
    {
      return node_id_;
    }

    /**
     * @brief Changes the associated node id.
     *
     * The counter is not reset automatically.
     *
     * @param node_id New node id.
     */
    void set_node_id(std::string node_id)
    {
      node_id_ = std::move(node_id);
    }

    /**
     * @brief Returns the current counter value.
     *
     * @return Current counter.
     */
    [[nodiscard]] value_type current() const noexcept
    {
      return counter_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Sets the current counter value.
     *
     * Mainly useful for recovery and tests.
     *
     * @param value New counter value.
     */
    void reset(value_type value = 0) noexcept
    {
      counter_.store(value, std::memory_order_relaxed);
    }

    /**
     * @brief Returns true if the generator is usable.
     *
     * @return true when node_id is not empty.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return !node_id_.empty();
    }

    /**
     * @brief Returns true if no id has been generated yet.
     *
     * @return true when current counter is zero.
     */
    [[nodiscard]] bool empty() const noexcept
    {
      return current() == 0;
    }

  private:
    std::string node_id_{};
    std::atomic<value_type> counter_{0};
  };

} // namespace softadastra::sync::utils

#endif // SOFTADASTRA_SYNC_ID_GENERATOR_HPP
