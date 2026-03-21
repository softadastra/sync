/*
 * SyncDirection.hpp
 */

#ifndef SOFTADASTRA_SYNC_DIRECTION_HPP
#define SOFTADASTRA_SYNC_DIRECTION_HPP

#include <cstdint>

namespace softadastra::sync::types
{
  /**
   * @brief Direction of a sync operation
   *
   * Indicates whether the operation originated locally
   * and is being propagated outward, or came from a
   * remote peer and is being applied locally.
   */
  enum class SyncDirection : std::uint8_t
  {
    Unknown = 0,

    LocalToRemote,
    RemoteToLocal
  };

} // namespace softadastra::sync::types

#endif
