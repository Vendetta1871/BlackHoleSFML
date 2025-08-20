#ifndef BLACKHOLESFML_ENGINE_H
#define BLACKHOLESFML_ENGINE_H
#include <vector>

#include <GL/glew.h>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

#include "BlackHole.h"
#include "Camera.h"
#include "ObjectData.h"

class Engine {
public:
    // Window / context via SFML
    sf::RenderWindow* window = nullptr;
    bool isTextureReady = false;

    sf::Vector2u computeSize{200, 150};

    explicit Engine(const sf::Vector2u& initialSize);
    ~Engine();

    void generateGrid(const std::vector<ObjectData>& objs);
    void drawGrid(const Camera& camera);

    void drawFullScreenQuad();

    void dispatchCompute(const Camera& cam, const BlackHole& hole, const std::vector<ObjectData>& objs);

private:
    // GL programs & buffers
    GLuint texture = 0;
    sf::Shader gridShader;
    sf::Shader blitShader;
    GLuint computeProgram = 0;

    GLuint cameraUBO = 0;
    GLuint diskUBO = 0;
    GLuint objectsUBO = 0;

    GLuint quadVAO = 0;
    GLuint gridVAO = 0, gridVBO = 0, gridEBO = 0;
    int gridIndexCount = 0;

    float width = 1e11f;
    float height = 7.5e10f;

    static GLuint CreateComputeProgram(const char* src);

    void uploadCameraUBO(const Camera& cam) const;
    void uploadObjectsUBO(const std::vector<ObjectData>& objs) const;
    void uploadDiskUBO(const BlackHole& hole) const;

    void genQuadVAO();
    void genBuffers();
};

#endif //BLACKHOLESFML_ENGINE_H
