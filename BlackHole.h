#ifndef BLACKHOLESFML_BLACKHOLE_H
#define BLACKHOLESFML_BLACKHOLE_H
#include <glm/vec3.hpp>

struct BlackHole {
public:
    glm::vec<3, float> position;
    double mass;
    double radius;
    double r_s;

    BlackHole(glm::vec3 pos, float m); // : position(pos), mass(m);
    bool Intercept(float px, float py, float pz) const;
};

#endif //BLACKHOLESFML_BLACKHOLE_H
