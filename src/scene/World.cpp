#include <coresim/scene/World.hpp>

#include <limits>
#include <stdexcept>

namespace coresim {
Entity World::create() {
    std::uint32_t index = free_head_;
    if (index == Entity::invalid_index) {
        if (slots_.size() >= Entity::invalid_index) {
            throw std::length_error("Entity slot capacity exhausted");
        }
        index = static_cast<std::uint32_t>(slots_.size());
        slots_.push_back({});
    } else {
        free_head_ = slots_[index].next_free;
    }
    auto& slot = slots_[index];
    slot.alive = true;
    slot.next_free = Entity::invalid_index;
    ++count_;
    return {index, slot.generation};
}
bool World::alive(Entity entity) const noexcept {
    return entity.index < slots_.size() && slots_[entity.index].alive &&
           slots_[entity.index].generation == entity.generation;
}
bool World::destroy(Entity entity) noexcept {
    if (!alive(entity)) {
        return false;
    }
    transforms_.remove(entity);
    meshes_.remove(entity);
    spins_.remove(entity);
    auto& slot = slots_[entity.index];
    slot.alive = false;
    --count_;
    // Retire an exhausted slot instead of wrapping a generation and reviving stale IDs.
    if (slot.generation != std::numeric_limits<std::uint64_t>::max()) {
        ++slot.generation;
        slot.next_free = free_head_;
        free_head_ = entity.index;
    }
    return true;
}
void World::require_alive(Entity entity) const {
    if (!alive(entity)) {
        throw std::invalid_argument("Component attachment requires a live entity from this World");
    }
}
Transform& World::set_transform(Entity entity, const Transform& value) {
    require_alive(entity);
    return transforms_.set(entity, value);
}
MeshComponent& World::set_mesh(Entity entity, const MeshComponent& value) {
    require_alive(entity);
    return meshes_.set(entity, value);
}
SpinComponent& World::set_spin(Entity entity, const SpinComponent& value) {
    require_alive(entity);
    return spins_.set(entity, value);
}
const Transform* World::transform(Entity entity) const noexcept {
    return alive(entity) ? transforms_.get(entity) : nullptr;
}
Transform* World::transform(Entity entity) noexcept {
    return const_cast<Transform*>(std::as_const(*this).transform(entity));
}
const MeshComponent* World::mesh(Entity entity) const noexcept {
    return alive(entity) ? meshes_.get(entity) : nullptr;
}
MeshComponent* World::mesh(Entity entity) noexcept {
    return const_cast<MeshComponent*>(std::as_const(*this).mesh(entity));
}
const SpinComponent* World::spin(Entity entity) const noexcept {
    return alive(entity) ? spins_.get(entity) : nullptr;
}
SpinComponent* World::spin(Entity entity) noexcept {
    return const_cast<SpinComponent*>(std::as_const(*this).spin(entity));
}
bool World::remove_transform(Entity entity) noexcept {
    return alive(entity) && transforms_.remove(entity);
}
bool World::remove_mesh(Entity entity) noexcept {
    return alive(entity) && meshes_.remove(entity);
}
bool World::remove_spin(Entity entity) noexcept {
    return alive(entity) && spins_.remove(entity);
}
} // namespace coresim
