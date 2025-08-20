#include "Camera.h"
#include <cmath>
#include <glm/common.hpp>

glm::vec3 Camera::position() const {
    const float clampedElevation = glm::clamp(elevation, 0.01f, float(M_PI) - 0.01f);
    return {
        radius * std::sin(clampedElevation) * std::cos(azimuth),
        radius * std::cos(clampedElevation),
        radius * std::sin(clampedElevation) * std::sin(azimuth)
    };
}

void Camera::update() {
    target = glm::vec3(0.0f, 0.0f, 0.0f);
    moving = (dragging | resizing | scrolling);
}

void Camera::processMouseMove(float x, float y) {
    float dx = x - lastX;
    float dy = y - lastY;

    if (dragging) {
        azimuth   += dx * orbitSpeed;
        elevation -= dy * orbitSpeed;
        elevation = glm::clamp(elevation, 0.01f, float(M_PI) - 0.01f);
    }
    lastX = x; lastY = y;
}

void Camera::processMouseButton(sf::Mouse::Button button, bool pressed, const sf::Window& win) {
    if (button == sf::Mouse::Button::Left || button == sf::Mouse::Button::Middle) {
        if (pressed) {
            dragging = true;
            auto p = sf::Mouse::getPosition(win);
            lastX = (float)p.x;
            lastY = (float)p.y;
        } else {
            dragging = false;
        }
    }
}

void Camera::processScroll(float /*xoffset*/, float yoffset) {
    radius -= yoffset * zoomSpeed;
    radius = glm::clamp(radius, minRadius, maxRadius);
    scrolling = true;
}

void Camera::processKey(sf::Keyboard::Scancode key, bool pressed) { }
