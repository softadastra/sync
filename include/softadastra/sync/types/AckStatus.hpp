/*
 * AckStatus.hpp
 */

#ifndef SOFTADASTRA_SYNC_ACK_STATUS_HPP
#define SOFTADASTRA_SYNC_ACK_STATUS_HPP

#include <cstdint>

namespace softadastra::sync::types
{
  /**
   * @brief Acknowledgement state of a sync operation
   *
   * Tracks whether an operation has been confirmed
   * by the remote peer.
   */
  enum class AckStatus : std::uint8_t
  {
    None = 0, // not sent yet
    Waiting,  // sent, waiting for ack
    Received, // ack received
    TimedOut  // no ack within timeout window
  };

} // namespace softadastra::sync::types

#endif
