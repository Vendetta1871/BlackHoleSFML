#ifndef BLACKHOLESFML_BLIT_FRAQ_H
#define BLACKHOLESFML_BLIT_FRAQ_H

inline auto blitVert = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

inline auto blitFraq = R"(
#version 330 core
in vec2 TexCoord;
uniform sampler2D screenTexture;

out vec4 FragColor;

void main() {
    FragColor = texture(screenTexture, TexCoord);
}
)";

#endif
