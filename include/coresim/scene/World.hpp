#pragma once

#include <coresim/scene/Components.hpp>
#include <coresim/scene/Transform.hpp>
#include <coresim/scene/detail/ComponentPool.hpp>
#include <utility>

namespace coresim {
// A single-threaded owner of IDs and CPU component data. No graphics resources.
class World {
public:
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;

    [[nodiscard]] Entity create();
    bool destroy(Entity entity) noexcept;
    [[nodiscard]] bool alive(Entity entity) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return count_; }

    Transform& set_transform(Entity entity, const Transform& value = {});
    MeshComponent& set_mesh(Entity entity, const MeshComponent& value = {});
    SpinComponent& set_spin(Entity entity, const SpinComponent& value = {});
    [[nodiscard]] Transform* transform(Entity entity) noexcept;
    [[nodiscard]] const Transform* transform(Entity entity) const noexcept;
    [[nodiscard]] MeshComponent* mesh(Entity entity) noexcept;
    [[nodiscard]] const MeshComponent* mesh(Entity entity) const noexcept;
    [[nodiscard]] SpinComponent* spin(Entity entity) noexcept;
    [[nodiscard]] const SpinComponent* spin(Entity entity) const noexcept;
    bool remove_transform(Entity entity) noexcept;
    bool remove_mesh(Entity entity) noexcept;
    bool remove_spin(Entity entity) noexcept;

    // Structural changes invalidate views/pointers; entity IDs remain stable.
    [[nodiscard]] auto transforms() const noexcept { return transforms_.entries(); }
    [[nodiscard]] auto meshes() const noexcept { return meshes_.entries(); }
    [[nodiscard]] auto spins() const noexcept { return spins_.entries(); }
private:
    void require_alive(Entity entity) const;
    struct Slot {
        std::uint64_t generation{1};
        std::uint32_t next_free{Entity::invalid_index};
        bool alive{};
    };
    std::vector<Slot> slots_;
    std::uint32_t free_head_{Entity::invalid_index};
    std::size_t count_{};
    detail::ComponentPool<Transform> transforms_;
    detail::ComponentPool<MeshComponent> meshes_;
    detail::ComponentPool<SpinComponent> spins_;
};
} // namespace coresim
