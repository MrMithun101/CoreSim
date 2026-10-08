#pragma once

#include <cstdint>
#include <limits>

namespace coresim {
// IDs are scoped to their originating World, not interchangeable between worlds.
struct Entity {
    static constexpr std::uint32_t invalid_index = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t index{invalid_index};
    std::uint64_t generation{};
    friend bool operator==(Entity, Entity) = default;
};
} // namespace coresim
