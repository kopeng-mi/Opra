// Conic wrapper over kepler.cpp, plus the sampling the orrery draws with.
#include "orbit/conic.h"

#include <cmath>

namespace opra::orbit {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

/** Position from the true anomaly: r = p / (1 + e cos nu), valid on both branches. */
glm::dvec2 perifocal_position(const Elements &elements, double nu) {
    const double p = elements.a * (1.0 - elements.e * elements.e);
    const double r = p / (1.0 + elements.e * std::cos(nu));
    const double c = std::cos(elements.omega);
    const double s = std::sin(elements.omega);
    return glm::dvec2(c * r * std::cos(nu) - s * r * std::sin(nu),
                      s * r * std::cos(nu) + c * r * std::sin(nu));
}

}  // namespace

glm::dvec2 Conic::position_at(double t) const { return opra::orbit::position_at(elements, t); }

glm::dvec2 Conic::velocity_at(double t) const { return opra::orbit::velocity_at(elements, t); }

double Conic::period() const { return opra::orbit::period(elements); }

double Conic::periapsis() const { return opra::orbit::periapsis(elements); }

double Conic::apoapsis() const { return opra::orbit::apoapsis(elements); }

void Conic::sample(int count, std::vector<glm::dvec2> &out) const {
    out.clear();
    if (count < 2) return;
    out.reserve(static_cast<std::size_t>(count));

    if (is_elliptic(elements)) {
        // Uniform in true anomaly: the points bunch where the orbit is tight and spread where it
        // is not, which is how an eccentric ellipse should be drawn at a fixed vertex budget.
        for (int i = 0; i < count; ++i) {
            out.push_back(perifocal_position(elements, kTwoPi * i / count));
        }
        return;
    }

    // Open branch: stop just short of the asymptote at nu = acos(-1/e), where r runs away.
    const double limit = std::acos(-1.0 / elements.e) * 0.98;
    for (int i = 0; i < count; ++i) {
        const double u = static_cast<double>(i) / static_cast<double>(count - 1);
        out.push_back(perifocal_position(elements, -limit + 2.0 * limit * u));
    }
}

}  // namespace opra::orbit
