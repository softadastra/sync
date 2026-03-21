/*
 * ConflictPolicy.hpp
 */

#ifndef SOFTADASTRA_SYNC_CONFLICT_POLICY_HPP
#define SOFTADASTRA_SYNC_CONFLICT_POLICY_HPP

#include <cstdint>

namespace softadastra::sync::types
{
  /**
   * @brief Conflict resolution strategy
   *
   * Defines how conflicting operations between local
   * and remote states should be resolved.
   */
  enum class ConflictPolicy : std::uint8_t
  {
    Unknown = 0,

    LastWriteWins, // compare timestamp / version
    KeepLocal,     // always keep local state
    KeepRemote,    // always accept remote state
    Manual         // defer resolution (future)
  };

} // namespace softadastra::sync::types

#endif
