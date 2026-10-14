#pragma once
#include <cstddef>
#include <iosfwd>
#include <span>
#include <string_view>
namespace coresim::gpu {
inline constexpr std::string_view demo_usage="--gpu-demo vector-add [--backend cpu|hip] [--elements 0..16777216] [--device index] [--info] [--help]\n";
struct DemoOptions {
    bool hip=false;
    std::size_t elements=1025;
    int device=0;
    bool info=false;
    bool help=false;
};
[[nodiscard]] DemoOptions parse_demo_options(std::span<const std::string_view> arguments);
void run_demo(const DemoOptions& options,std::ostream& output);
}
