#pragma once
#include <coresim/particles/ParticleSimulation.hpp>
#include <coresim/jobs/JobSystem.hpp>
#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>
namespace coresim::particles::detail {
inline bool finite(const Particle& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
           std::isfinite(p.vx) && std::isfinite(p.vy) && std::isfinite(p.vz);
}
inline void validate(std::span<const Particle> initial, Acceleration acceleration) {
    if (initial.size() > maximum_particles) { throw std::invalid_argument("Particle count exceeds 100000"); }
    for (const float value : acceleration) {
        if (!std::isfinite(value)) { throw std::invalid_argument("Particle acceleration must be finite"); }
    }
    for (const auto& p : initial) {
        if (!finite(p)) { throw std::invalid_argument("Initial particle state must be finite"); }
    }
}
inline Particle integrate(const Particle& p, Acceleration a) {
    Particle out = p;
    out.vx = p.vx + a[0] * timestep;
    out.vy = p.vy + a[1] * timestep;
    out.vz = p.vz + a[2] * timestep;
    out.x = p.x + out.vx * timestep;
    out.y = p.y + out.vy * timestep;
    out.z = p.z + out.vz * timestep;
    if (!finite(out)) { throw std::overflow_error("Non-finite particle integration result"); }
    return out;
}
// Bounded, balanced tasks; drain all borrowed work even if submission fails.
template<class Range> std::size_t dispatch(std::size_t count, JobSystem* jobs, Range&& range) {
    if (!jobs) { range(0, count); return 0; }
    if (count == 0) { return 0; }
    const auto tasks = std::min(count, jobs->worker_count());
    if (tasks == 0) { throw std::logic_error("Particle parallel execution needs workers"); }
    JobGroup group;
    const auto width = count / tasks, remainder = count % tasks;
    std::size_t begin = 0;
    try {
        for (std::size_t task = 0; task < tasks; ++task) {
            const auto end = begin + width + (task < remainder ? 1U : 0U);
            jobs->submit(group, [&, begin, end] { range(begin, end); });
            begin = end;
        }
    } catch (...) {
        const auto failure = std::current_exception();
        try { group.wait(); } catch (...) {}
        std::rethrow_exception(failure);
    }
    group.wait();
    return tasks;
}
}
