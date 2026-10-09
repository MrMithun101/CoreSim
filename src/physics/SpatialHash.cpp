#include <coresim/physics/SpatialHash.hpp>
#include <coresim/scene/World.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace coresim {
std::size_t CellHash::operator()(const CellKey& key) const noexcept {
    // Unsigned arithmetic is intentional; key equality resolves hash collisions.
    auto mix = [](std::uint64_t value) {
        value ^= value >> 30; value *= 0xbf58476d1ce4e5b9ULL;
        value ^= value >> 27; value *= 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    };
    return static_cast<std::size_t>(mix(static_cast<std::uint64_t>(key.x)) ^
        (mix(static_cast<std::uint64_t>(key.y)) << 1) ^
        (mix(static_cast<std::uint64_t>(key.z)) << 2));
}
SpatialHash::SpatialHash(float cell_size) : cell_size_(cell_size) {
    if (!std::isfinite(cell_size) || cell_size <= 0) {
        throw std::invalid_argument("Spatial hash cell size must be positive and finite");
    }
}
void SpatialHash::build(const World& world) {
    cells_.clear();
    global_.clear();
    const auto colliders = world.colliders();
    entries_.assign(colliders.size(), {});
    seen_.assign(colliders.size(), std::numeric_limits<std::size_t>::max());
    for (std::size_t i = 0; i < colliders.size(); ++i) {
        const auto* transform = world.transform(colliders[i].entity);
        if (!transform) { continue; }
        auto& entry = entries_[i];
        entry.active = true;
        const auto& shape = colliders[i].value;
        glm::vec3 extent(0);
        if (const auto* sphere = std::get_if<SphereCollider>(&shape)) { extent = glm::vec3(sphere->radius); }
        else if (const auto* box = std::get_if<BoxCollider>(&shape)) { extent = box->half_extents; }
        else { global_.push_back(i); continue; } // Infinite planes cannot occupy a finite grid.
        Range range{};
        std::int64_t low[3]{}, high[3]{};
        std::uint64_t volume = 1;
        bool bounded = true;
        for (int axis = 0; axis < 3; ++axis) {
            const double p = transform->position[axis];
            const double half = extent[axis];
            // Conservative padding accounts for the float arithmetic in narrow phase.
            const double pad = 4.0 * std::numeric_limits<float>::epsilon() * (std::abs(p) + half + 1.0);
            const double lo = std::floor((p - half - pad) / cell_size_);
            const double hi = std::floor((p + half + pad) / cell_size_);
            if (!std::isfinite(lo) || !std::isfinite(hi) || half < 0 ||
                lo < -1.0e9 || hi > 1.0e9 || hi < lo || hi - lo + 1 > 4096) {
                bounded = false; break;
            }
            low[axis] = static_cast<std::int64_t>(lo);
            high[axis] = static_cast<std::int64_t>(hi);
            volume *= static_cast<std::uint64_t>(high[axis] - low[axis] + 1);
            if (volume > 4096) { bounded = false; break; }
        }
        if (!bounded) { global_.push_back(i); continue; }
        range.low = {low[0], low[1], low[2]};
        range.high = {high[0], high[1], high[2]};
        entry.range = range;
        for (auto x = range.low.x; x <= range.high.x; ++x) {
            for (auto y = range.low.y; y <= range.high.y; ++y) {
                for (auto z = range.low.z; z <= range.high.z; ++z) { cells_[{x,y,z}].push_back(i); }
            }
        }
    }
}
std::span<const std::size_t> SpatialHash::query(std::size_t i) {
    candidates_.clear();
    if (i >= entries_.size()) { throw std::out_of_range("Spatial hash collider index"); }
    if (!entries_[i].active) { return candidates_; }
    // Reset stamps for repeated queries too; no allocation and at most O(N) scratch space.
    const auto add = [&](std::size_t j) {
        if (j > i && entries_[j].active && seen_[j] != i) {
            seen_[j] = i;
            candidates_.push_back(j);
        }
    };
    if (!entries_[i].range) {
        for (std::size_t j = i + 1; j < entries_.size(); ++j) { add(j); }
    } else {
        for (const auto j : global_) { add(j); }
        const auto range = *entries_[i].range;
        for (auto x = range.low.x; x <= range.high.x; ++x) {
            for (auto y = range.low.y; y <= range.high.y; ++y) {
                for (auto z = range.low.z; z <= range.high.z; ++z) {
                    const auto cell = cells_.find({x,y,z});
                    if (cell != cells_.end()) { for (const auto j : cell->second) { add(j); } }
                }
            }
        }
    }
    std::sort(candidates_.begin(), candidates_.end());
    for (const auto j : candidates_) { seen_[j] = std::numeric_limits<std::size_t>::max(); }
    return candidates_;
}
} // namespace coresim
