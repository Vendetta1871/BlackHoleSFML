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

void processEvents(const std::optional<sf::Event>& event, Engine& engine, Camera& camera);
void draw(Engine& engine, const Camera& camera, const std::vector<ObjectData>& objects, const BlackHole& hole);

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
    while (engine.window->isOpen()) {
        camera.resizing = false;
        camera.scrolling = false; // need to reset cuz there is no way to know if scroll stopped
        while (const std::optional event = engine.window->pollEvent())
            processEvents(event, engine, camera);
        camera.update();

        // --- Dynamic resolution ---
        const float dt = sfClock.restart().asSeconds();
        const sf::Vector2u size = engine.window->getSize();
        if (camera.moving) {
            if (dt > 1.f / 24.f) {
                engine.computeSize.x = std::max(engine.computeSize.x * 3u / 4u, size.x / 32u);
                engine.computeSize.y = std::max(engine.computeSize.y * 3u / 4u, size.y / 32u);
            }
            engine.isTextureReady = false;
        } else if (dt < 1.f / 10.f && !engine.isTextureReady) {
            engine.computeSize.x = std::min(engine.computeSize.x * 4u / 3u, size.x);
            engine.computeSize.y = std::min(engine.computeSize.y * 4u / 3u, size.y);
        } else {
            engine.isTextureReady = true;
            sf::sleep(sf::seconds(1.f / 24.f - dt));
            continue;
        }

        draw(engine, camera, objects, SagA);
    }

    return 0;
}

void processEvents(const std::optional<sf::Event>& event, Engine& engine, Camera& camera) {
    if (event->is<sf::Event::Closed>()) {
        engine.window->close();
    } else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
        camera.resizing = true;
        engine.isTextureReady = false;
    } else if (const auto* moved = event->getIf<sf::Event::MouseMoved>()) {
        camera.processMouseMove((float)moved->position.x, (float)moved->position.y);
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

void draw(Engine& engine, const Camera& camera, const std::vector<ObjectData>& objects, const BlackHole& hole) {
    // --- Clear ---
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, (int)engine.window->getSize().x, (int)engine.window->getSize().y);

    // --- Grid ---
    engine.generateGrid(objects);
    engine.drawGrid(camera);

    // --- Compute Raytracer -> Texture ---
    engine.dispatchCompute(camera, hole, objects);
    engine.drawFullScreenQuad();

    // --- Present ---
    engine.window->display();
}
