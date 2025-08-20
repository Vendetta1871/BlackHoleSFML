#ifndef BLACKHOLESFML_GRID_SHADER_H
#define BLACKHOLESFML_GRID_SHADER_H

inline auto gridVert = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 viewProj;

void main() {
    gl_Position = viewProj * vec4(aPos, 1.0);
}
)";

inline auto gridFraq = R"(
#version 330 core
out vec4 FragColor;

void main() {
    FragColor = vec4(0.5, 0.5, 0.5, 0.7); // translucent blue lines
}
)";

#endif //BLACKHOLESFML_GRID_SHADER_H
