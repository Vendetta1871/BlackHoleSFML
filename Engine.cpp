#include "Engine.h"
#include "shaders/blit.shader.h"
#include "shaders/grid.shader.h"
#include "shaders/geodesic.shader.h"

#include <fstream>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

Engine::Engine(const sf::Vector2u& initialSize) {
    window = new sf::RenderWindow(sf::VideoMode(initialSize), "Black Hole (SFML + OpenGL)");
    window->setVerticalSyncEnabled(true);

    // init GLEW after context is active
    glewExperimental = GL_TRUE;
    if (const GLenum glewErr = glewInit(); glewErr != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << (const char*)glewGetErrorString(glewErr) << std::endl;
        exit(EXIT_FAILURE);
    }
    glGetError(); // GLEW might leave a GL_INVALID_ENUM, clear it

    std::cout << "OpenGL " << glGetString(GL_VERSION) << std::endl;

    glEnable(GL_DEPTH_TEST);
    glClearDepth(1.0);

    if (!gridShader.loadFromMemory(gridVert, gridFraq)) {
        std::cerr << "Failed to load grid shaders" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    if (!blitShader.loadFromMemory(blitVert, blitFraq)) {
        std::cerr << "Failed to load blit shaders" << std::endl;
        std::exit(EXIT_FAILURE);
    }
    computeProgram = CreateComputeProgram(geodesicComp);

    genBuffers();
    genQuadVAO();

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, computeSize.x, computeSize.y, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

Engine::~Engine() {
    if (!window) return;
    window->close();
    delete window;
    window = nullptr;
}

void Engine::generateGrid(const std::vector<ObjectData>& objs) {
    constexpr int gridSize = 25;
    constexpr double c = 299792458.0;
    constexpr double G = 6.67430e-11;

    std::vector<glm::vec3> vertices;
    std::vector<GLuint> indices;

    for (int z = 0; z <= gridSize; ++z) {
        for (int x = 0; x <= gridSize; ++x) {
            constexpr float spacing = 1e10f;
            float worldX = ((float)x - (float)gridSize / 2.f) * spacing;
            float worldZ = ((float)z - (float)gridSize / 2.f) * spacing;

            float y = 0.0f;
            for (const auto& obj : objs) {
                const auto objPos = glm::vec3(obj.posRadius);
                const double mass = obj.mass;
                const double r_s = 2.0 * G * mass / (c * c);
                const double dx = worldX - objPos.x;
                const double dz = worldZ - objPos.z;

                if (const double dist = std::sqrt(dx*dx + dz*dz); dist > r_s) {
                    const double deltaY = 2.0 * std::sqrt(r_s * (dist - r_s));
                    y += static_cast<float>(deltaY) - 3e10f;
                } else {
                    y += 2.0f * static_cast<float>(std::sqrt(r_s * r_s)) - 3e10f;
                }
            }
            vertices.emplace_back(worldX, y, worldZ);
        }
    }

    for (int z = 0; z < gridSize; ++z) {
        for (int x = 0; x < gridSize; ++x) {
            const int i = z * (gridSize + 1) + x;
            indices.push_back(i); indices.push_back(i + 1);
            indices.push_back(i); indices.push_back(i + gridSize + 1);
        }
    }

    if (gridVAO == 0) glGenVertexArrays(1, &gridVAO);
    if (gridVBO == 0) glGenBuffers(1, &gridVBO);
    if (gridEBO == 0) glGenBuffers(1, &gridEBO);

    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gridEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)nullptr);

    gridIndexCount = (int)indices.size();
    glBindVertexArray(0);
}

void Engine::drawGrid(const Camera& camera) {
    glm::mat4 view = lookAt(camera.position(), camera.target, glm::vec3(0,1,0));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f),
        float(window->getSize().x)/float(window->getSize().y), 1e9f, 1e14f);
    glm::mat4 viewProj = proj * view;

    sf::Shader::bind(&gridShader);

    gridShader.setUniform("viewProj", sf::Glsl::Mat4(glm::value_ptr(viewProj)));

    glBindVertexArray(gridVAO);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawElements(GL_LINES, gridIndexCount, GL_UNSIGNED_INT, nullptr);

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);

    sf::Shader::bind(nullptr);
}

void Engine::drawFullScreenQuad() {
    blitShader.setUniform("u_texture", sf::Shader::CurrentTexture);
    blitShader.setUniform("u_textureSize", sf::Vector2f(computeSize));
    blitShader.setUniform("u_sigma", 1.f);
    blitShader.setUniform("u_sharpness", 0.4f);

    sf::Shader::bind(&blitShader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);

    sf::Shader::bind(nullptr);
}

GLuint Engine::CreateComputeProgram(const char* src) {
    const GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(cs, 1, &src, nullptr);
    glCompileShader(cs);
    GLint ok; glGetShaderiv(cs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len; glGetShaderiv(cs, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetShaderInfoLog(cs, len, nullptr, log.data());
        std::cerr << "Compute shader compile error:\n" << log.data() << std::endl;
        exit(EXIT_FAILURE);
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, cs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len; glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << "Compute shader link error:\n" << log.data() << "\n";
        exit(EXIT_FAILURE);
    }
    glDeleteShader(cs);
    return prog;
}

void Engine::dispatchCompute(const Camera& cam, const BlackHole& hole, const std::vector<ObjectData>& objs) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, computeSize.x, computeSize.y,
        0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glUseProgram(computeProgram);
    uploadCameraUBO(cam);
    uploadDiskUBO(hole);
    uploadObjectsUBO(objs);
    glUniform2i(glGetUniformLocation(computeProgram, "texSize"), computeSize.x, computeSize.y);

    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

    const auto groupsX = (GLuint)std::ceil((float)computeSize.x / 16.f);
    const auto groupsY = (GLuint)std::ceil((float)computeSize.y / 16.f);
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void Engine::uploadCameraUBO(const Camera& cam) const {
    struct UBOData {
        glm::vec3 pos; float _pad0;
        glm::vec3 right; float _pad1;
        glm::vec3 up; float _pad2;
        glm::vec3 forward; float _pad3;
        float tanHalfFov;
        int moving;
        float aspect;
        int   _pad4;
    } data{};

    glm::vec3 fwd = normalize(cam.target - cam.position());
    glm::vec3 up = glm::vec3(0, 1, 0);
    glm::vec3 right = normalize(cross(fwd, up));
    up = cross(right, fwd);

    data.pos = cam.position();
    data.right = right;
    data.up = up;
    data.forward = fwd;
    data.tanHalfFov = std::tan(glm::radians(60.0f * 0.5f));
    data.moving = cam.moving ? 1 : 0;
    data.aspect = static_cast<float>(window->getSize().x) / static_cast<float>(window->getSize().y);

    glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(UBOData), &data);
}

void Engine::uploadObjectsUBO(const std::vector<ObjectData>& objs) const {
    struct UBOData {
        int   numObjects;
        float _pad0, _pad1, _pad2;
        glm::vec4  posRadius[16];
        glm::vec4  color[16];
        float mass[16];
    } data{};

    size_t count = std::min(objs.size(), size_t(16));
    data.numObjects = static_cast<int>(count);
    for (size_t i = 0; i < count; ++i) {
        data.posRadius[i] = objs[i].posRadius;
        data.color[i]     = objs[i].color;
        data.mass[i]      = objs[i].mass;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, objectsUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(data), &data);
}

void Engine::uploadDiskUBO(const BlackHole& hole) const {
    float r1 = float(hole.r_s) * 2.2f;
    float r2 = float(hole.r_s) * 5.2f;
    float num = 2.0f;
    float thickness = 1e9f;
    float diskData[4] = { r1, r2, num, thickness };

    glBindBuffer(GL_UNIFORM_BUFFER, diskUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(diskData), diskData);
}

void Engine::genQuadVAO() {
    constexpr float quadVertices[] = {
        // pos         // uv
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        1.0f, -1.0f,  1.0f, 0.0f,
        1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
    };
    GLuint VBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);
}

void Engine::genBuffers() {
    glGenBuffers(1, &cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, 128, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, cameraUBO);

    glGenBuffers(1, &diskUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, diskUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * 4, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, diskUBO);

    glGenBuffers(1, &objectsUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, objectsUBO);
    constexpr GLsizeiptr objUBOSize = sizeof(int) + 3*sizeof(float)
                                      + 16*(sizeof(glm::vec4) + sizeof(glm::vec4))
                                      + 16*sizeof(float);
    glBufferData(GL_UNIFORM_BUFFER, objUBOSize, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, objectsUBO);
}
