// Sphere of influence: the boundary at which a body's gravity stops dominating its parent's.
// Patched conics need it to decide which primary the ship is orbiting (PLAN-02 §3.3).
#pragma once

namespace opra::orbit {

/** Hill-sphere radius a (m_body / m_parent)^(2/5); metres, from the body's own orbit radius. */
double soi_radius(double a_body, double m_body, double m_parent);

}  // namespace opra::orbit
