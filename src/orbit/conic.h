// The orbit as a thing the renderer and the map can ask for: elements plus the polyline sampling
// the orrery draws. No state, no caching — every call is a closed-form evaluation.
#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "orbit/kepler.h"

namespace opra::orbit {

struct Conic {
    Elements elements;

    glm::dvec2 position_at(double t) const;
    glm::dvec2 velocity_at(double t) const;
    double period() const;
    double periapsis() const;
    double apoapsis() const;

    /**
     * `count` points along the conic. An ellipse is a closed ring: the points run one full
     * revolution from periapsis and stop one step short, so the caller joins last to first
     * without a duplicate vertex. A hyperbola is an open branch over the escapable range of
     * true anomaly.
     */
    void sample(int count, std::vector<glm::dvec2> &out) const;
};

}  // namespace opra::orbit
