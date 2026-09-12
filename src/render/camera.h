// The flight camera: a narrow-field perspective view of the navigation plane. The eye orbits the
// render origin at a constrained pitch with yaw locked, so the plane reads with depth while world
// north stays screen up - the collar's bearing frame does not have to change.
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
/** Depth range in metres. Reversed-Z is what keeps the hull detail at the near end usable. */
inline constexpr float CAMERA_NEAR = 1.0f;
inline constexpr float CAMERA_FAR = 12000.0f;

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
    /** Half the visible height at the target plane: the zoom control, in metres. */
    float half_height = 252.0f;
    float aspect = 16.0f / 9.0f;
};

/** The eye offset for a pitch: up and back along the orbit, sized so `half_height` holds. */
glm::vec3 orbit_eye(float half_height, float pitch);

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
    float near_z = CAMERA_NEAR;
    float far_z = CAMERA_FAR;
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
