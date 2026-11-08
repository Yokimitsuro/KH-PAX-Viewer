#include "camera.h"

#include <algorithm>
#include <cmath>

namespace pax {

namespace {

constexpr float pi = 3.14159265358979F;

void unit(float v[3])
{
    const float n = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (n > 0)
        for (int i = 0; i < 3; ++i)
            v[i] /= n;
}

void cross(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

} // namespace

// the view's axes as KINGDOM HEARTS II has them: x to the right of the screen, y down it, z into it
void Orbit::axes(const float e[3], float x[3], float y[3], float z[3]) const
{
    z[0] = target[0] - e[0], z[1] = target[1] - e[1], z[2] = target[2] - e[2];
    unit(z);
    const float down[3] = {0, 1, 0};
    cross(down, z, x);
    unit(x);
    cross(z, x, y);
}

void Orbit::reset()
{
    *this = Orbit{};
}

void Orbit::turn(float dx, float dy)
{
    yaw += dx * 0.008F;
    pitch = std::clamp(pitch + dy * 0.008F, -1.55F, 1.55F);
}

void Orbit::pan(float dx, float dy, float view_height)
{
    // move the target across the view, a pixel the size of one at the target's distance
    const float per_pixel = 2.0F * distance * std::tan(fov * pi / 360.0F) / std::max(1.0F, view_height);
    float e[3];
    eye(e);
    float x[3], y[3], z[3];
    axes(e, x, y, z);
    for (int i = 0; i < 3; ++i)
        target[i] += (-x[i] * dx - y[i] * dy) * per_pixel;
}

void Orbit::zoom(float wheel)
{
    distance = std::clamp(distance * std::pow(0.88F, wheel), 5.0F, 200000.0F);
}

void Orbit::eye(float out[3]) const
{
    // above the target is -y (KINGDOM HEARTS II's y points down)
    out[0] = target[0] + distance * std::cos(pitch) * std::sin(yaw);
    out[1] = target[1] - distance * std::sin(pitch);
    out[2] = target[2] - distance * std::cos(pitch) * std::cos(yaw);
}

khdays::assets::kh2::EffectCamera Orbit::camera(float aspect) const
{
    khdays::assets::kh2::EffectCamera c;
    c.world = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float e[3];
    eye(e);
    float x[3], y[3], z[3];
    axes(e, x, y, z);
    auto &v = c.view;
    v = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (int i = 0; i < 3; ++i) {
        v[std::size_t(i * 4 + 0)] = x[i];
        v[std::size_t(i * 4 + 1)] = y[i];
        v[std::size_t(i * 4 + 2)] = z[i];
    }
    for (int j = 0; j < 3; ++j)
        v[std::size_t(12 + j)] = -(e[0] * v[std::size_t(j)] + e[1] * v[std::size_t(4 + j)] + e[2] * v[std::size_t(8 + j)]);
    const float near = std::max(1.0F, distance * 0.01F), far = distance * 50.0F + 20000.0F;
    const float f = 1.0F / std::tan(fov * pi / 360.0F);
    auto &p = c.projection;
    p = {};
    p[0] = f / aspect;
    p[5] = -f; // the view's y points down the screen; OpenGL's up
    p[10] = far / (far - near);
    p[11] = 1;
    p[14] = -near * far / (far - near);
    return c;
}

} // namespace pax
