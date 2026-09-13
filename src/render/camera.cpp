#include "render/camera.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace opra {

double Camera::eye_distance() const {
    return half_height / std::tan(static_cast<double>(CAMERA_FOV_Y) * 0.5);
}

float Camera::near_z() const {
    // s2.2: both planes ride the eye distance, so the near/far ratio is 1e6 at every scale and
    // reversed-Z has the same precision zoomed onto a hull as zoomed out to the system.
    return static_cast<float>(eye_distance() * 1.0e-3);
}

float Camera::far_z() const { return static_cast<float>(eye_distance() * 1.0e3); }

glm::vec3 orbit_eye(double half_height, float pitch) {
    // The distance that makes `half_height` the half-height of the view *at the target*: everything
    // the HUD scales by hand (screen pixels per metre) is measured at that plane.
    const double radius = half_height / std::tan(static_cast<double>(CAMERA_FOV_Y) * 0.5);
    return {0.0f, static_cast<float>(-std::cos(static_cast<double>(pitch)) * radius),
            static_cast<float>(std::sin(static_cast<double>(pitch)) * radius)};
}

bool ViewFrustum::contains(const glm::vec3 &at, float radius) const {
    const glm::vec3 offset = at - eye;
    const float depth = glm::dot(offset, forward);
    if (depth + radius < near_z) return false;  // entirely behind the near plane
    if (depth - radius > far_z) return false;   // entirely beyond the far plane
    // At or behind the eye plane the cone test does not hold; such an object straddles the frame,
    // so it is kept. The pitch clamp keeps this case off the play plane, but debris can sit there.
    if (depth + radius <= 0.0f) return true;
    const float z = std::max(depth, 0.0f);
    const float lateral = std::abs(glm::dot(offset, right));
    const float vertical = std::abs(glm::dot(offset, up));
    return lateral - radius <= z * tan_x + margin && vertical - radius <= z * tan_y + margin;
}

ViewFrustum view_frustum(const Camera &camera, float margin) {
    ViewFrustum frustum;
    frustum.eye = camera.eye;
    frustum.forward = glm::normalize(camera.target - camera.eye);
    frustum.right = glm::normalize(glm::cross(frustum.forward, glm::vec3(0.0f, 0.0f, 1.0f)));
    frustum.up = glm::cross(frustum.right, frustum.forward);
    frustum.tan_y = std::tan(CAMERA_FOV_Y * 0.5f);
    frustum.tan_x = frustum.tan_y * camera.aspect;
    frustum.near_z = camera.near_z();
    frustum.far_z = camera.far_z();
    frustum.margin = margin;
    return frustum;
}

glm::dvec2 unproject(const Camera &camera, float px, float py, float width, float height,
                     float plane_z) {
    const glm::mat4 inverse = glm::inverse(view_projection(camera));
    const float ndc_x = (px / width) * 2.0f - 1.0f;
    const float ndc_y = 1.0f - (py / height) * 2.0f;
    // Reversed-Z: the near plane carries depth 1 and the far plane depth 0.
    glm::vec4 near_point = inverse * glm::vec4(ndc_x, ndc_y, 1.0f, 1.0f);
    glm::vec4 far_point = inverse * glm::vec4(ndc_x, ndc_y, 0.0f, 1.0f);
    near_point /= near_point.w;
    far_point /= far_point.w;
    const glm::vec3 direction = glm::normalize(glm::vec3(far_point - near_point));
    // Never parallel to the plane: the pitch clamp stops the eye at 15 degrees, and a ray parallel
    // to z would put the answer at infinity instead of reporting a miss.
    const float target_z = plane_z - static_cast<float>(camera.origin.z);
    const float t = (target_z - near_point.z) / direction.z;
    const glm::vec3 at = glm::vec3(near_point) + direction * t;
    return {camera.origin.x + static_cast<double>(at.x),
            camera.origin.y + static_cast<double>(at.y)};
}

glm::mat4 view_projection(const Camera &camera) {
    // Up is +Y, not the plane normal: at 90 degrees (straight down) +Z is parallel to the view, and
    // at every other pitch +Y is the axis that keeps world north pointing up the screen.
    const glm::mat4 view = glm::lookAt(camera.eye, camera.target, glm::vec3(0.0f, 1.0f, 0.0f));
    // Reversed-Z: near maps to 1 and far to 0, so the depth target is cleared to 0 and the mesh
    // pipeline compares GREATER. Both planes are derived from the eye distance (s2.2), so the
    // depth ratio - and therefore reversed-Z's precision - is the same at every zoom.
    const float near_z = camera.near_z();
    const float far_z = camera.far_z();
    const glm::mat4 proj = glm::perspectiveZO(CAMERA_FOV_Y, camera.aspect, far_z, near_z);
    return proj * view;
}

glm::vec2 project(const glm::mat4 &matrix, const glm::dvec3 &origin, Real x, Real y, float plane_z,
                  float width, float height) {
    const glm::vec4 clip = matrix * glm::vec4(static_cast<float>(x - origin.x),
                                              static_cast<float>(y - origin.y),
                                              plane_z - static_cast<float>(origin.z), 1.0f);
    const float w = clip.w != 0.0f ? clip.w : 1.0f;
    return {(clip.x / w * 0.5f + 0.5f) * width, (0.5f - clip.y / w * 0.5f) * height};
}

}  // namespace opra
