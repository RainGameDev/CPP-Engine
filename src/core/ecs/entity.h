#pragma once

#include <cstdint>

using EntityId = uint32_t;

/// Sentinel for "no entity" (reused by component sparse-sets and the editor).
inline constexpr EntityId NONE = static_cast<EntityId>(-1);
