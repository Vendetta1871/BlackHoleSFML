#ifndef BLACKHOLESFML_ENGINE_H
#define BLACKHOLESFML_ENGINE_H
#include <GL/glew.h>
#include <glm/fwd.hpp>
#include <SFML/Window/Window.hpp>
#include <vector>

#include "BlackHole.h"
#include "Camera.h"
#include "ObjectData.h"

class Engine {
public:
    // Window / context via SFML
    sf::Window* window = nullptr;

    // GL programs & buffers
    GLuint gridShaderProgram = 0;
    GLuint quadVAO = 0;
    GLuint texture = 0;
    GLuint shaderProgram = 0;
    GLuint computeProgram = 0;

    GLuint cameraUBO = 0;
    GLuint diskUBO = 0;
    GLuint objectsUBO = 0;

    GLuint gridVAO = 0, gridVBO = 0, gridEBO = 0;
    int gridIndexCount = 0;

    unsigned int WIDTH = 800;
    unsigned int HEIGHT = 600;
    int COMPUTE_WIDTH  = 200;
    int COMPUTE_HEIGHT = 150;

    float width = 1e11f;
    float height = 7.5e10f;

    Engine();

    ~Engine();

    void generateGrid(const std::vector<ObjectData>& objs);

    void drawGrid(const glm::mat4& viewProj);

    void drawFullScreenQuad();

    GLuint CreateShaderProgram();

    GLuint CreateShaderProgram(const char* vertPath, const char* fragPath);

    GLuint CreateComputeProgram(const char* path);

    void dispatchCompute(const Camera& cam, const BlackHole& hole, const std::vector<ObjectData>& objs);

    void uploadCameraUBO(const Camera& cam);

    void uploadObjectsUBO(const std::vector<ObjectData>& objs);

    void uploadDiskUBO(const BlackHole& hole);

    std::vector<GLuint> QuadVAO();
};

#endif //BLACKHOLESFML_ENGINE_H
