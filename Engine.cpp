#include "Engine.h"

#include <fstream>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

Engine::Engine() {
    // Request core 4.3 context for compute shaders
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.stencilBits = 8;
    settings.majorVersion = 4;
    settings.minorVersion = 3;
    settings.attributeFlags = sf::ContextSettings::Core;

    window = new sf::RenderWindow(sf::VideoMode({WIDTH, HEIGHT}), "Black Hole (SFML + OpenGL)");
    window->setVerticalSyncEnabled(true);
    //window->setActive(true);

    // init GLEW after context is active
    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << (const char*)glewGetErrorString(glewErr) << std::endl;
        exit(EXIT_FAILURE);
    }
    glGetError(); // GLEW might leave a GL_INVALID_ENUM, clear it

    std::cout << "OpenGL " << glGetString(GL_VERSION) << std::endl;

    glEnable(GL_DEPTH_TEST);
    glClearDepth(1.0);

    shaderProgram = CreateShaderProgram();                 // quad blit
    gridShaderProgram = CreateShaderProgram("shaders/grid.vert", "shaders/grid.frag");
    computeProgram = CreateComputeProgram("shaders/geodesic.comp");

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
    GLsizeiptr objUBOSize = sizeof(int) + 3*sizeof(float)
        + 16*(sizeof(glm::vec4) + sizeof(glm::vec4))
        + 16*sizeof(float);
    glBufferData(GL_UNIFORM_BUFFER, objUBOSize, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, objectsUBO);

    auto result = QuadVAO();
    quadVAO = result[0];
    texture = result[1];
}

Engine::~Engine() {
    if (!window) return;
    window->setActive(false);
    window->close();
    delete window;
    window = nullptr;
}

void Engine::generateGrid(const std::vector<ObjectData>& objs) {
    const int gridSize = 25;
    const float spacing = 1e10f;
    constexpr double c = 299792458.0;
    constexpr double G = 6.67430e-11;

    std::vector<glm::vec3> vertices;
    std::vector<GLuint> indices;

    for (int z = 0; z <= gridSize; ++z) {
        for (int x = 0; x <= gridSize; ++x) {
            float worldX = (float)(x - gridSize / 2) * spacing;
            float worldZ = (float)(z - gridSize / 2) * spacing;

            float y = 0.0f;
            for (const auto& obj : objs) {
                auto objPos = glm::vec3(obj.posRadius);
                double mass = obj.mass;
                double r_s = 2.0 * G * mass / (c * c);
                double dx = worldX - objPos.x;
                double dz = worldZ - objPos.z;
                double dist = sqrt(dx*dx + dz*dz);

                if (dist > r_s) {
                    double deltaY = 2.0 * sqrt(r_s * (dist - r_s));
                    y += static_cast<float>(deltaY) - 3e10f;
                } else {
                    y += 2.0f * static_cast<float>(sqrt(r_s * r_s)) - 3e10f;
                }
            }
            vertices.emplace_back(worldX, y, worldZ);
        }
    }

    for (int z = 0; z < gridSize; ++z) {
        for (int x = 0; x < gridSize; ++x) {
            int i = z * (gridSize + 1) + x;
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    gridIndexCount = (int)indices.size();
    glBindVertexArray(0);
}


void Engine::drawGrid(const glm::mat4& viewProj) {
    glUseProgram(gridShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(gridShaderProgram, "viewProj"),
                       1, GL_FALSE, glm::value_ptr(viewProj));

    glBindVertexArray(gridVAO);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawElements(GL_LINES, gridIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void Engine::drawFullScreenQuad() {
    glUseProgram(shaderProgram);
    glBindVertexArray(quadVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(shaderProgram, "screenTexture"), 0);

    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
    glEnable(GL_DEPTH_TEST);
}

GLuint Engine::CreateShaderProgram() {
    const char* vsSrc = R"(
            #version 330 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTexCoord;
            out vec2 TexCoord;
            void main(){
                gl_Position = vec4(aPos, 0.0, 1.0);
                TexCoord = aTexCoord;
            }
        )";
    const char* fsSrc = R"(
            #version 330 core
            in vec2 TexCoord;
            uniform sampler2D screenTexture;
            out vec4 FragColor;
            void main(){
                FragColor = texture(screenTexture, TexCoord);
            }
        )";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, nullptr);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, nullptr);
    glCompileShader(fs);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

GLuint Engine::CreateShaderProgram(const char* vertPath, const char* fragPath) {
    auto loadShader = [](const char* path, GLenum type) -> GLuint {
        std::ifstream in(path);
        if (!in.is_open()) {
            std::cerr << "Failed to open shader: " << path << "\n";
            exit(EXIT_FAILURE);
        }
        std::stringstream ss; ss << in.rdbuf();
        std::string srcStr = ss.str();
        const char* src = srcStr.c_str();

        GLuint sh = glCreateShader(type);
        glShaderSource(sh, 1, &src, nullptr);
        glCompileShader(sh);

        GLint ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len; glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(len);
            glGetShaderInfoLog(sh, len, nullptr, log.data());
            std::cerr << "Shader compile error (" << path << "):\n" << log.data() << "\n";
            exit(EXIT_FAILURE);
        }
        return sh;
    };

    GLuint vs = loadShader(vertPath, GL_VERTEX_SHADER);
    GLuint fs = loadShader(fragPath, GL_FRAGMENT_SHADER);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len; glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << "Shader link error:\n" << log.data() << "\n";
        exit(EXIT_FAILURE);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

GLuint Engine::CreateComputeProgram(const char* path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "Failed to open compute shader: " << path << "\n";
        exit(EXIT_FAILURE);
    }
    std::stringstream ss; ss << in.rdbuf();
    std::string srcStr = ss.str();
    const char* src = srcStr.c_str();

    GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(cs, 1, &src, nullptr);
    glCompileShader(cs);
    GLint ok; glGetShaderiv(cs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len; glGetShaderiv(cs, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetShaderInfoLog(cs, len, nullptr, log.data());
        std::cerr << "Compute shader compile error:\n" << log.data() << "\n";
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
    //int cw = cam.moving ? COMPUTE_WIDTH  : 200;
    //int ch = cam.moving ? COMPUTE_HEIGHT : 150;
    int cw = COMPUTE_WIDTH;
    int ch = COMPUTE_HEIGHT;

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, cw, ch, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glUseProgram(computeProgram);
    uploadCameraUBO(cam);
    uploadDiskUBO(hole);
    uploadObjectsUBO(objs);

    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

    auto groupsX = (GLuint)std::ceil(cw / 16.0f);
    auto groupsY = (GLuint)std::ceil(ch / 16.0f);
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void Engine::uploadCameraUBO(const Camera& cam) {
    struct UBOData {
        glm::vec3 pos; float _pad0;
        glm::vec3 right; float _pad1;
        glm::vec3 up; float _pad2;
        glm::vec3 forward; float _pad3;
        float tanHalfFov;
        float aspect;
        int   moving;
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
    data.tanHalfFov = tan(glm::radians(60.0f * 0.5f));
    data.aspect = float(WIDTH) / float(HEIGHT);
    data.moving = (cam.dragging || cam.panning) ? 1 : 0;

    glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(UBOData), &data);
}

void Engine::uploadObjectsUBO(const std::vector<ObjectData>& objs) {
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

void Engine::uploadDiskUBO(const BlackHole& hole) {
    float r1 = float(hole.r_s) * 2.2f;
    float r2 = float(hole.r_s) * 5.2f;
    float num = 2.0f;
    float thickness = 1e9f;
    float diskData[4] = { r1, r2, num, thickness };

    glBindBuffer(GL_UNIFORM_BUFFER, diskUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(diskData), diskData);
}

std::vector<GLuint> Engine::QuadVAO() {
    float quadVertices[] = {
        // pos      // uv
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, COMPUTE_WIDTH, COMPUTE_HEIGHT, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    return { VAO, tex };
}
