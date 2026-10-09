#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace coresim {
class World;
struct CellKey {
    std::int64_t x{}, y{}, z{};
    bool operator==(const CellKey&) const = default;
};
struct CellHash { std::size_t operator()(const CellKey& key) const noexcept; };
// Rebuilt each tick. Collider indices and query spans are valid until the next build/query.
class SpatialHash {
public:
    explicit SpatialHash(float cell_size = 3.0F);
    void build(const World& world);
    // Sorted, unique j > i candidates preserve the naive solver's contact ordering.
    std::span<const std::size_t> query(std::size_t i);
private:
    struct Range { CellKey low, high; };
    struct Entry { bool active{}; std::optional<Range> range; };
    float cell_size_;
    std::unordered_map<CellKey, std::vector<std::size_t>, CellHash> cells_;
    std::vector<Entry> entries_;
    std::vector<std::size_t> global_, candidates_, seen_;
};
} // namespace coresim
