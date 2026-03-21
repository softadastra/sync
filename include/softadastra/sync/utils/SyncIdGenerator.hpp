/*
 * SyncIdGenerator.hpp
 */

#ifndef SOFTADASTRA_SYNC_ID_GENERATOR_HPP
#define SOFTADASTRA_SYNC_ID_GENERATOR_HPP

#include <atomic>
#include <cstdint>
#include <string>

namespace softadastra::sync::utils
{
  /**
   * @brief Generates unique sync operation identifiers
   *
   * The generated id is deterministic in structure:
   *
   *   <node_id>-<counter>
   *
   * This is sufficient for the current local-first / offline-first
   * architecture as long as each node_id is unique.
   */
  class SyncIdGenerator
  {
  public:
    /**
     * @brief Construct a generator bound to a node id
     */
    explicit SyncIdGenerator(std::string node_id)
        : node_id_(std::move(node_id))
    {
    }

    /**
     * @brief Generate the next sync id
     */
    std::string next()
    {
      const std::uint64_t value = ++counter_;
      return node_id_ + "-" + std::to_string(value);
    }

    /**
     * @brief Return the associated node id
     */
    const std::string &node_id() const noexcept
    {
      return node_id_;
    }

    /**
     * @brief Return the current counter value
     */
    std::uint64_t current() const noexcept
    {
      return counter_.load();
    }

    /**
     * @brief Reset the counter
     *
     * Mainly useful for tests.
     */
    void reset(std::uint64_t value = 0) noexcept
    {
      counter_.store(value);
    }

    /**
     * @brief Check whether the generator is usable
     */
    bool valid() const noexcept
    {
      return !node_id_.empty();
    }

  private:
    std::string node_id_;
    std::atomic<std::uint64_t> counter_{0};
  };

} // namespace softadastra::sync::utils

#endif
