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
out vec4 FragColor;

uniform sampler2D u_texture;
uniform vec2 u_textureSize;
uniform float u_sigma;      // для Gaussian
uniform float u_sharpness;  // коэффициент USM, например 0.4

float gaussian(float x, float sigma) {
    return exp(-(x*x)/(2.0*sigma*sigma));
}

vec4 ewaGaussian(vec2 texCoord) {
    vec2 coord = texCoord * u_textureSize;
    vec4 color = vec4(0.0);
    float totalWeight = 0.0;

    int r = int(ceil(3.0*u_sigma));
    for (int j = -r; j <= r; j++) {
        for (int i = -r; i <= r; i++) {
            vec2 sampleCoord = floor(coord) + vec2(i, j);
            vec2 tCoord = sampleCoord / u_textureSize;
            vec4 sample = texture(u_texture, tCoord);

            float d = length(coord - sampleCoord);
            float w = gaussian(d, u_sigma);

            color += sample * w;
            totalWeight += w;
        }
    }
    return color / totalWeight;
}

void main()
{
    vec4 blurred = ewaGaussian(TexCoord);
    vec4 original = texture(u_texture, TexCoord); // bilinear оригинал

    // маска резкости
    vec4 mask = original - blurred;

    // финальный результат с лёгкой резкостью
    FragColor = blurred + u_sharpness * mask;
}


)";

#endif
