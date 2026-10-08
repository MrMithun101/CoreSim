#pragma once

#include <coresim/scene/Entity.hpp>
#include <limits>
#include <span>
#include <vector>
#include <utility>

namespace coresim {
template <class T> struct ComponentEntry {
    Entity entity;
    T value;
};

namespace detail {
// Internal storage helper for the fixed value components supported by World.
// Sparse slots point into a packed array; removal swaps the last entry into the hole.
template <class T> class ComponentPool {
public:
    T& set(Entity entity, const T& value) {
        if (auto* existing = get(entity)) {
            *existing = value;
            return *existing;
        }
        const auto slot = static_cast<std::size_t>(entity.index);
        if (slot >= sparse_.size()) {
            sparse_.resize(slot + 1, missing);
        }
        // Publish the sparse index only after the potentially allocating append succeeds.
        dense_.push_back({entity, value});
        sparse_[slot] = dense_.size() - 1;
        return dense_.back().value;
    }
    [[nodiscard]] const T* get(Entity entity) const noexcept {
        if (entity.index >= sparse_.size()) {
            return nullptr;
        }
        const auto index = sparse_[entity.index];
        return index != missing && dense_[index].entity == entity ? &dense_[index].value : nullptr;
    }
    [[nodiscard]] T* get(Entity entity) noexcept {
        return const_cast<T*>(std::as_const(*this).get(entity));
    }
    bool remove(Entity entity) noexcept {
        if (!get(entity)) {
            return false;
        }
        const auto index = sparse_[entity.index];
        if (index != dense_.size() - 1) {
            dense_[index] = dense_.back();
            sparse_[dense_[index].entity.index] = index;
        }
        dense_.pop_back();
        sparse_[entity.index] = missing;
        return true;
    }
    [[nodiscard]] std::span<const ComponentEntry<T>> entries() const noexcept { return dense_; }
private:
    static constexpr auto missing = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> sparse_;
    std::vector<ComponentEntry<T>> dense_;
};
} // namespace detail
} // namespace coresim
