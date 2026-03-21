/*
 * SyncStatus.hpp
 */

#ifndef SOFTADASTRA_SYNC_STATUS_HPP
#define SOFTADASTRA_SYNC_STATUS_HPP

#include <cstdint>

namespace softadastra::sync::types
{
  /**
   * @brief Lifecycle status of a sync operation
   *
   * Describes where an operation currently is in the
   * synchronization pipeline.
   */
  enum class SyncStatus : std::uint8_t
  {
    Unknown = 0,

    Pending,      // created but not yet queued
    Queued,       // ready to be sent
    InFlight,     // handed off for sending / awaiting completion
    Acknowledged, // remote side confirmed receipt
    Applied,      // applied successfully on target side
    Failed        // failed and may require retry or inspection
  };

} // namespace softadastra::sync::types

#endif
