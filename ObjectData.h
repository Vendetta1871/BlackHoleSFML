#ifndef BLACKHOLESFML_OBJECTDATA_H
#define BLACKHOLESFML_OBJECTDATA_H
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

struct ObjectData {
    glm::vec4 posRadius; // xyz pos, w radius
    glm::vec4 color;     // rgba
    float  mass;
    glm::vec3 velocity{};
};

#endif //BLACKHOLESFML_OBJECTDATA_H
