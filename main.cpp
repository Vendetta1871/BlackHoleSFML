// ---- OpenGL loader first
#include <GL/glew.h>

// ---- SFML (window/context/events)
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>

// ---- GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ---- STL
#include <vector>
#include <chrono>

#include "Camera.h"
#include "BlackHole.h"
#include "Engine.h"
#include "ObjectData.h"

using Clock = std::chrono::high_resolution_clock;

int main() {
Camera camera;
    BlackHole SagA(glm::vec3(0.0f, 0.0f, 0.0f), 8.54e36); // Sagittarius A
    std::vector<ObjectData> objects = {
        { glm::vec4(4e11f, 0.0f, 0.0f, 4e10f)   , glm::vec4(1,1,0,1), 1.98892e30f },
        { glm::vec4(0.0f, 0.0f, 4e11f, 4e10f)   , glm::vec4(1,0,0,1), 1.98892e30f },
        { glm::vec4(0.0f, 0.0f, 0.0f, (float)SagA.r_s) , glm::vec4(0,0,0,1), (float)SagA.mass  },
    };
    Engine engine;

    double lastTime = 0.0;
    while (engine.window->isOpen()) {
        sf::Clock sfClock;
        while (const std::optional event = engine.window->pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                engine.window->close();
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                engine.WIDTH  = (int)resized->size.x;
                engine.HEIGHT = (int)resized->size.y;
                glViewport(0, 0, (int)engine.WIDTH, (int)engine.HEIGHT);
            }else if (const auto* moved = event->getIf<sf::Event::MouseMoved>()) {
                camera.processMouseMove(moved->position.x, moved->position.y);
            } else if (const auto* pressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                camera.processMouseButton(pressed->button, true, *engine.window);
            } else if (const auto* released = event->getIf<sf::Event::MouseButtonReleased>()) {
                camera.processMouseButton(released->button, false, *engine.window);
            } else if (const auto* scrolled = event->getIf<sf::Event::MouseWheelScrolled>()) {
                camera.processScroll(0.0, scrolled->delta);
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                camera.processKey(keyPressed->scancode, true);
            }
            else if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>()) {
                if (keyReleased->scancode == sf::Keyboard::Scancode::Equal) {
                   // engine.COMPUTE_HEIGHT *= 2;
                   // engine.COMPUTE_WIDTH *= 2;
                }
                if (keyReleased->scancode == sf::Keyboard::Scancode::Hyphen) {
                   // engine.COMPUTE_HEIGHT /= 2;
                   // engine.COMPUTE_WIDTH /= 2;
                }
            }
        }

        // --- Timing ---
        double now = sfClock.getElapsedTime().asSeconds();
        double dt = now - lastTime;
        lastTime = now;
        (void)dt; // if not used further

        // --- Clear ---
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- Grid ---
        engine.generateGrid(objects);
        glm::mat4 view = lookAt(camera.position(), camera.target, glm::vec3(0,1,0));
        glm::mat4 proj = glm::perspective(glm::radians(60.0f),
            float(engine.WIDTH)/float(engine.HEIGHT), 1e9f, 1e14f);
        glm::mat4 viewProj = proj * view;
        engine.drawGrid(viewProj);

        // --- Compute Raytracer -> Texture ---
        glViewport(0, 0, (int)engine.WIDTH, (int)engine.HEIGHT);
        engine.dispatchCompute(camera, SagA, objects);
        engine.drawFullScreenQuad();

        // --- Present ---
        engine.window->display();
    }

    return 0;
}
