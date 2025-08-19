#include "BlackHole.h"

BlackHole::BlackHole(glm::vec3 pos, float m) : position(pos), mass(m), radius(0) {
    position = pos;
    mass = m;
    radius = 0;

    constexpr double c = 299792458.0;
    constexpr double G = 6.67430e-11;
    r_s = 2.0 * G * mass / (c * c);
}

bool BlackHole::Intercept(float px, float py, float pz) const {
    const double dx = double(px) - double(position.x);
    const double dy = double(py) - double(position.y);
    const double dz = double(pz) - double(position.z);
    const double dist2 = dx*dx + dy*dy + dz*dz;
    return dist2 < r_s * r_s;
}
