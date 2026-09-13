// The flight camera: one continuous zoom (plan 05 J2) from hull scale to system scale, a narrow
// field of view, and an eye that orbits the follow point at a constrained pitch with yaw near
// locked - the plane reads with depth while world north stays screen up, so the collar's bearing
// frame does not have to change.
#pragma once

// SDL3 GPU renders to zero-to-one depth, which has to be selected before glm is included: an
// OpenGL-style projection is clipped away entirely by D3D12.
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include "core/units.h"

namespace opra {

/** Vertical field of view, radians: 24 degrees, narrow enough that depth reads as parallax. */
inline constexpr float CAMERA_FOV_Y = 0.41887903f;
/** Pitch limits. Below 15 degrees the view flattens back into a chart; 90 is straight down. */
inline constexpr float CAMERA_PITCH_MIN = 0.26179939f;
inline constexpr float CAMERA_PITCH_MAX = 1.57079633f;
inline constexpr float CAMERA_PITCH_DEFAULT = 0.55850536f;  // 32 degrees

/** The zoom range (plan 05 s2.1): half the visible height at the target plane, in metres, from a
 *  5 m hull shot to 5e10 m - ten orders of magnitude, one wheel. */
inline constexpr double ZOOM_HALF_MIN = 5.0;
inline constexpr double ZOOM_HALF_MAX = 5.0e10;
/** The home framing: the `0` key's half-height, 3.00 px per metre at 900 px tall (s3.1). */
inline constexpr double HOME_HALF = 150.0;
/** Scale presets (s2.1): `1`..`5` jump to hull / flight / local / orbital / system. */
inline constexpr double SCALE_PRESETS[5] = {30.0, 150.0, 2.0e3, 5.0e6, 5.0e9};
/** The zoom notch: half_height *= exp(-wheel * k), k per notch, 1.0 with Shift held. */
inline constexpr double ZOOM_NOTCH = 0.25;
inline constexpr double ZOOM_NOTCH_FAST = 1.0;
/** Below this half-height the warp rail comes down to 1x: close quarters is real time (s2.7). */
inline constexpr double ZOOM_REAL_TIME_BELOW = 200.0;

/** Clamps a half-height into the zoom range: every writer of the control goes through this. */
inline double clamp_half_height(double half_height) {
    return half_height < ZOOM_HALF_MIN ? ZOOM_HALF_MIN
         : half_height > ZOOM_HALF_MAX ? ZOOM_HALF_MAX
                                       : half_height;
}

/**
 * The camera. `origin` is the render origin in world double metres; `eye` and `target` are
 * origin-relative floats, which is safe because the eye sits a fixed R from the target plane and
 * every consumer that needs precision does the world subtraction in double first.
 */
struct Camera {
    /**
     * World position of the render origin, in double metres. Every instance and every world mark is
     * placed as float(world - origin): at 26 Gm a float holds no metres at all, so the subtraction
     * has to happen in double, once, before the cast.
     */
    glm::dvec3 origin{0.0};
    /** Eye and target, relative to `origin`, so what the shader sees is always a small number. */
    glm::vec3 eye{0.0f};
    glm::vec3 target{0.0f};
    /** Half the visible height at the target plane: the zoom control, metres. Ten decades of it. */
    double half_height = HOME_HALF;
    float aspect = 16.0f / 9.0f;

    /** R = half_height / tan(fov_y / 2): the eye's distance from the target plane. */
    double eye_distance() const;
    /** Derived depth range (s2.2): near = R*1e-3, far = R*1e3, ratio 1e6 at every scale. */
    float near_z() const;
    float far_z() const;
    /** Half-height as the float the old framing code took: kept for the HUD's pixel maths. */
    float half_height_f() const { return static_cast<float>(half_height); }
};

/** The eye offset for a pitch: up and back along the orbit, sized so `half_height` holds. */
glm::vec3 orbit_eye(double half_height, float pitch);

/**
 * The camera's visible volume, as the eye basis plus the cone that opens with distance. A box sized
 * at the target plane would be wrong for a perspective view: a rock 3 km behind the ship is inside
 * the frame long before it is inside that box, because the frame widens with distance.
 */
struct ViewFrustum {
    glm::vec3 eye{0.0f};
    glm::vec3 forward{0.0f, 1.0f, 0.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 0.0f, 1.0f};
    float tan_x = 1.0f;  // half-angle tangent, horizontal
    float tan_y = 1.0f;  // half-angle tangent, vertical
    float near_z = 1.0f;
    float far_z = 1.0e12f;
    /** Slack in metres, for the biggest object in the field: never drop a visible part. */
    float margin = 0.0f;

    /** True unless the sphere at `at` is certainly outside. Positions are origin-relative. */
    bool contains(const glm::vec3 &at, float radius) const;
};

/** The frustum this camera sees, with `margin` slack. */
ViewFrustum view_frustum(const Camera &camera, float margin);

/** Screen pixel -> where that ray crosses the plane z = `plane_z`, in world double metres. */
glm::dvec2 unproject(const Camera &camera, float px, float py, float width, float height,
                     float plane_z = 0.0f);

glm::mat4 view_projection(const Camera &camera);

/** World point on the plane z = `plane_z` -> screen pixels. Double in, pixels out. */
glm::vec2 project(const glm::mat4 &matrix, const glm::dvec3 &origin, Real x, Real y, float plane_z,
                  float width, float height);

}  // namespace opra
