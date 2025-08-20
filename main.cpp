// ---- OpenGL loader first
#include <GL/glew.h>

// ---- SFML (window/context/events)
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>

// ---- GLM
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ---- STL
#include <iostream>
#include <vector>

#include "Camera.h"
#include "BlackHole.h"
#include "Engine.h"
#include "ObjectData.h"

int main() {
Camera camera;
    const BlackHole SagA(glm::vec3(0.0f, 0.0f, 0.0f), 8.54e36); // Sagittarius A
    const std::vector<ObjectData> objects = {
        { glm::vec4(4e11f, 0.0f, 0.0f, 4e10f)   , glm::vec4(1,1,0,1), 1.98892e30f },
        { glm::vec4(0.0f, 0.0f, 4e11f, 4e10f)   , glm::vec4(1,0,0,1), 1.98892e30f },
        { glm::vec4(0.0f, 0.0f, 0.0f, (float)SagA.r_s) , glm::vec4(0,0,0,1), (float)SagA.mass  },
    };
    Engine engine{{800, 600}};

    sf::Clock sfClock;
    int framecount = 0;
    while (engine.window->isOpen()) {
        camera.scrolling = false; // need to reset cuz there is no way to know if scroll stopped
        while (const std::optional event = engine.window->pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                engine.window->close();
            } else if (const auto* moved = event->getIf<sf::Event::MouseMoved>()) {
                camera.processMouseMove(moved->position.x, moved->position.y);
            } else if (const auto* pressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                camera.processMouseButton(pressed->button, true, *engine.window);
            } else if (const auto* released = event->getIf<sf::Event::MouseButtonReleased>()) {
                camera.processMouseButton(released->button, false, *engine.window);
            } else if (const auto* scrolled = event->getIf<sf::Event::MouseWheelScrolled>()) {
                camera.processScroll(0.0, scrolled->delta);
            } else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                camera.processKey(keyPressed->scancode, true);
            } else if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>()) {
                if (keyReleased->scancode == sf::Keyboard::Scancode::Equal)
                   engine.computeSize *= 2u;
                if (keyReleased->scancode == sf::Keyboard::Scancode::Hyphen)
                   engine.computeSize /= 2u;
            }
        }
        camera.update();

        // --- Timing ---
        const float dt = sfClock.restart().asSeconds();
        framecount++;
        if (camera.moving) {
            const float scale = (1.f + 1.f / 60.f / dt) / 2.f;
            const auto x = (uint32_t)((float)engine.computeSize.x * scale);
            const auto y = (uint32_t)((float)engine.computeSize.y * scale);
            engine.computeSize.x = std::max(x, engine.window->getSize().x / 32u);
            engine.computeSize.y = std::max(y, engine.window->getSize().y / 32u);
        } else if (dt < 1.f / 5.f) {
            engine.computeSize = engine.computeSize * 8u / 7u;
        }

        // --- Clear ---
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- Grid ---
        engine.generateGrid(objects);
        engine.drawGrid(camera);

        // --- Compute Raytracer -> Texture ---
        glViewport(0, 0, (int)engine.window->getSize().x, (int)engine.window->getSize().y);
        engine.dispatchCompute(camera, SagA, objects);
        engine.drawFullScreenQuad();

        // --- Present ---
        engine.window->display();
    }

    return 0;
}