#include "Camera.h"
#include <cmath>
#include <glm/common.hpp>

glm::vec3 Camera::position() const {
    float clampedElevation = glm::clamp(elevation, 0.01f, float(M_PI) - 0.01f);
    return {
        radius * std::sin(clampedElevation) * std::cos(azimuth),
        radius * std::cos(clampedElevation),
        radius * std::sin(clampedElevation) * std::sin(azimuth)
    };
}

void Camera::update() {
    target = glm::vec3(0.0f, 0.0f, 0.0f);
    moving = (dragging | panning);
}

void Camera::processMouseMove(double x, double y) {
    auto dx = static_cast<float>(x - lastX);
    auto dy = static_cast<float>(y - lastY);

    if (dragging && panning) {
        // camera is always around the center
    } else if (dragging && !panning) {
        azimuth   += dx * orbitSpeed;
        elevation -= dy * orbitSpeed;
        elevation = glm::clamp(elevation, 0.01f, float(M_PI) - 0.01f);
    }
    lastX = x; lastY = y;
    update();
}

void Camera::processMouseButton(sf::Mouse::Button button, bool pressed, const sf::Window& win) {
    if (button == sf::Mouse::Button::Left || button == sf::Mouse::Button::Middle) {
        if (pressed) {
            dragging = true;
            panning = false; // keep center on a black hole
            auto p = sf::Mouse::getPosition(win);
            lastX = p.x; lastY = p.y;
        } else {
            dragging = false;
            panning = false;
        }
    }
    update();
}

void Camera::processScroll(double /*xoffset*/, double yoffset) {
    radius -= yoffset * zoomSpeed;
    radius = glm::clamp(radius, minRadius, maxRadius);
    update();
}

void Camera::processKey(sf::Keyboard::Scancode key, bool pressed) { }
