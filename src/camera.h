// A camera orbiting a point, as the effect player wants it: KINGDOM HEARTS II's axes (the world's y down; the
// view's x right, y down the screen, z into it), row vectors, a projection whose depth runs 0 to 1 and whose w is
// the view's depth.
#pragma once

#include "khdays/assets/kh2_effect.h"

namespace pax {

struct Orbit {
    float yaw = 0.6F, pitch = 0.3F; // radians: about the vertical, above the ground
    float distance = 700.0F;
    float target[3] = {0.0F, -100.0F, 0.0F};
    float fov = 45.0F; // degrees, vertical

    void reset();
    void turn(float dx, float dy);                  // a drag, in pixels
    void pan(float dx, float dy, float view_height); // a drag, in pixels
    void zoom(float wheel);
    void eye(float out[3]) const;
    void axes(const float eye[3], float x[3], float y[3], float z[3]) const;
    khdays::assets::kh2::EffectCamera camera(float aspect) const;
};

} // namespace pax
