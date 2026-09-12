// The Hill-sphere approximation: r_SOI = a (m / M)^(2/5). It is the radius inside which the
// smaller body's gravity, not the parent's tidal field, sets a ship's orbit — the switch test in
// PLAN-02 §3.3. Good to a few percent, and patched conics are not more accurate than that anyway.
#include "orbit/soi.h"

#include <cmath>

namespace opra::orbit {

double soi_radius(double a_body, double m_body, double m_parent) {
    return a_body * std::pow(m_body / m_parent, 0.4);
}

}  // namespace opra::orbit
