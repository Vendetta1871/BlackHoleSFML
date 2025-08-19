#ifndef BLACKHOLESFML_CAMERA_H
#define BLACKHOLESFML_CAMERA_H
#include <glm/vec3.hpp>
#include <SFML/Window.hpp>
#include <cmath>

class Camera {
public:
    glm::vec3 target{0.f, 0.f, 0.f};

    bool dragging = false;
    bool panning = false;
    bool moving = false;

    Camera() = default;

    [[nodiscard]] glm::vec3 position() const;

    void processMouseMove(double x, double y);

    void processMouseButton(sf::Mouse::Button button, bool pressed, const sf::Window& win);

    void processScroll(double /*xoffset*/, double yoffset);

    void processKey(sf::Keyboard::Scancode key, bool pressed);

private:
    float radius = 6.34194e10f;
    float minRadius = 1e10f;
    float maxRadius = 1e12f;

    float azimuth = 0.0f;
    float elevation =  (float)M_PI / 2.0f;

    float orbitSpeed = 0.01f;
    float panSpeed = 0.01f;
    double zoomSpeed = 25e9f;

    double lastX = 0.f;
    double lastY = 0.f;

    void update();
};

#endif //BLACKHOLESFML_CAMERA_H
