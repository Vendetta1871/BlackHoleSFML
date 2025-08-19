// SFML + OpenGL (compute) port of your GLFW code
#define _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
#include <iostream>
#include <cmath>
#include <sstream>
#include <cstring>
#include <chrono>
#include <fstream>

using namespace glm;
using namespace std;
using Clock = std::chrono::high_resolution_clock;

// VARS
double lastPrintTime = 0.0;
int    framesCount   = 0;
double c = 299792458.0;
double G = 6.67430e-11;
bool   Gravity = false;

struct Ray;

struct Camera {
    vec3 target = vec3(0.0f, 0.0f, 0.0f);
    float radius = 6.34194e10f;
    float minRadius = 1e10f, maxRadius = 1e12f;

    float azimuth = 0.0f;
    float elevation = float(M_PI) / 2.0f;

    float orbitSpeed = 0.01f;
    float panSpeed = 0.01f;
    double zoomSpeed = 25e9f;

    bool dragging = false;
    bool panning = false;
    bool moving = false;
    double lastX = 0.0, lastY = 0.0;



    vec3 position() const {
        float clampedElevation = glm::clamp(elevation, 0.01f, float(M_PI) - 0.01f);
        return vec3(
            radius * sin(clampedElevation) * cos(azimuth),
            radius * cos(clampedElevation),
            radius * sin(clampedElevation) * sin(azimuth)
        );
    }
    void update() {
        target = vec3(0.0f, 0.0f, 0.0f);
        moving = (dragging | panning);
    }

    void processMouseMove(double x, double y) {
        float dx = float(x - lastX);
        float dy = float(y - lastY);

        if (dragging && panning) {
            // панорамирование отключено — камера всегда вокруг центра
        } else if (dragging && !panning) {
            azimuth   += dx * orbitSpeed;
            elevation -= dy * orbitSpeed;
            elevation = glm::clamp(elevation, 0.01f, float(M_PI) - 0.01f);
        }
        lastX = x; lastY = y;
        update();
    }

    void processMouseButton(sf::Mouse::Button button, bool pressed, const sf::Window& win) {
        if (button == sf::Mouse::Button::Left || button == sf::Mouse::Button::Middle) {
            if (pressed) {
                dragging = true;
                panning = false; // держим центр на ЧД
                auto p = sf::Mouse::getPosition(win);
                lastX = p.x; lastY = p.y;
            } else {
                dragging = false;
                panning = false;
            }
        }
        if (button == sf::Mouse::Button::Right) {
            Gravity = pressed;
        }
        update();
    }

    void processScroll(double /*xoffset*/, double yoffset) {
        radius -= yoffset * zoomSpeed;
        radius = glm::clamp(radius, minRadius, maxRadius);
        update();
    }

    void processKey(sf::Keyboard::Scancode key, bool pressed) {
        if (pressed && key == sf::Keyboard::Scancode::G) {
            Gravity = !Gravity;
            cout << "[INFO] Gravity turned " << (Gravity ? "ON" : "OFF") << endl;
        }
    }
};

Camera camera;

struct BlackHole {
    vec3 position;
    double mass;
    double radius;
    double r_s;

    BlackHole(vec3 pos, float m) : position(pos), mass(m) { r_s = 2.0 * G * mass / (c*c); }
    bool Intercept(float px, float py, float pz) const {
        double dx = double(px) - double(position.x);
        double dy = double(py) - double(position.y);
        double dz = double(pz) - double(position.z);
        double dist2 = dx*dx + dy*dy + dz*dz;
        return dist2 < r_s * r_s;
    }
};
BlackHole SagA(vec3(0.0f, 0.0f, 0.0f), 8.54e36); // Sagittarius A

struct ObjectData {
    vec4 posRadius; // xyz pos, w radius
    vec4 color;     // rgba
    float  mass;
    vec3 velocity = vec3(0.0f);
};
vector<ObjectData> objects = {
    { vec4(4e11f, 0.0f, 0.0f, 4e10f)   , vec4(1,1,0,1), 1.98892e30f },
    { vec4(0.0f, 0.0f, 4e11f, 4e10f)   , vec4(1,0,0,1), 1.98892e30f },
    { vec4(0.0f, 0.0f, 0.0f, (float)SagA.r_s) , vec4(0,0,0,1), (float)SagA.mass  },
};

struct Engine {
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

    Engine() {
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
            cerr << "Failed to initialize GLEW: " << (const char*)glewGetErrorString(glewErr) << "\n";
            exit(EXIT_FAILURE);
        }
        glGetError(); // GLEW might leave a GL_INVALID_ENUM, clear it

        cout << "OpenGL " << glGetString(GL_VERSION) << "\n";

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
            + 16*(sizeof(vec4) + sizeof(vec4))
            + 16*sizeof(float);
        glBufferData(GL_UNIFORM_BUFFER, objUBOSize, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 3, objectsUBO);

        auto result = QuadVAO();
        quadVAO = result[0];
        texture = result[1];
    }

    ~Engine() {
        if (window) { window->setActive(false); window->close(); delete window; window = nullptr; }
    }

    void generateGrid(const vector<ObjectData>& objs) {
        const int gridSize = 25;
        const float spacing = 1e10f;

        vector<vec3> vertices;
        vector<GLuint> indices;

        for (int z = 0; z <= gridSize; ++z) {
            for (int x = 0; x <= gridSize; ++x) {
                float worldX = (x - gridSize / 2) * spacing;
                float worldZ = (z - gridSize / 2) * spacing;

                float y = 0.0f;
                for (const auto& obj : objs) {
                    vec3 objPos = vec3(obj.posRadius);
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
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vec3), vertices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gridEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);

        gridIndexCount = (int)indices.size();
        glBindVertexArray(0);
    }

    void drawGrid(const mat4& viewProj) {
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

    void drawFullScreenQuad() {
        glUseProgram(shaderProgram);
        glBindVertexArray(quadVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(shaderProgram, "screenTexture"), 0);

        glDisable(GL_DEPTH_TEST);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
        glEnable(GL_DEPTH_TEST);
    }

    GLuint CreateShaderProgram() {
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

    GLuint CreateShaderProgram(const char* vertPath, const char* fragPath) {
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

    GLuint CreateComputeProgram(const char* path) {
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

    void dispatchCompute(const Camera& cam) {
        //int cw = cam.moving ? COMPUTE_WIDTH  : 200;
        //int ch = cam.moving ? COMPUTE_HEIGHT : 150;
        int cw = COMPUTE_WIDTH;
        int ch = COMPUTE_HEIGHT;

        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, cw, ch, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glUseProgram(computeProgram);
        uploadCameraUBO(cam);
        uploadDiskUBO();
        uploadObjectsUBO(objects);

        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

        GLuint groupsX = (GLuint)std::ceil(cw / 16.0f);
        GLuint groupsY = (GLuint)std::ceil(ch / 16.0f);
        glDispatchCompute(groupsX, groupsY, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void uploadCameraUBO(const Camera& cam) {
        struct UBOData {
            vec3 pos; float _pad0;
            vec3 right; float _pad1;
            vec3 up; float _pad2;
            vec3 forward; float _pad3;
            float tanHalfFov;
            float aspect;
            int   moving;
            int   _pad4;
        } data;

        vec3 fwd = normalize(cam.target - cam.position());
        vec3 up = vec3(0, 1, 0);
        vec3 right = normalize(cross(fwd, up));
        up = cross(right, fwd);

        data.pos = cam.position();
        data.right = right;
        data.up = up;
        data.forward = fwd;
        data.tanHalfFov = tan(radians(60.0f * 0.5f));
        data.aspect = float(WIDTH) / float(HEIGHT);
        data.moving = (cam.dragging || cam.panning) ? 1 : 0;

        glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(UBOData), &data);
    }

    void uploadObjectsUBO(const vector<ObjectData>& objs) {
        struct UBOData {
            int   numObjects;
            float _pad0, _pad1, _pad2;
            vec4  posRadius[16];
            vec4  color[16];
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

    void uploadDiskUBO() {
        float r1 = float(SagA.r_s) * 2.2f;
        float r2 = float(SagA.r_s) * 5.2f;
        float num = 2.0f;
        float thickness = 1e9f;
        float diskData[4] = { r1, r2, num, thickness };

        glBindBuffer(GL_UNIFORM_BUFFER, diskUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(diskData), diskData);
    }

    vector<GLuint> QuadVAO() {
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
};
Engine engine;

// ---- MAIN ----
int main() {
    // timings
    auto t0 = Clock::now();
    lastPrintTime = chrono::duration<double>(t0.time_since_epoch()).count();

    sf::Clock sfClock;
    double lastTime = 0.0;

    while (engine.window->isOpen()) {
        while (const std::optional event = engine.window->pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                engine.window->close();
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                engine.WIDTH  = (int)resized->size.x;
                engine.HEIGHT = (int)resized->size.y;
                glViewport(0, 0, engine.WIDTH, engine.HEIGHT);
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

        // --- Physics (toggle with right mouse or 'G') ---
        for (auto& obj : objects) {
            for (auto& obj2 : objects) {
                if (&obj == &obj2) continue;
                float dx = obj2.posRadius.x - obj.posRadius.x;
                float dy = obj2.posRadius.y - obj.posRadius.y;
                float dz = obj2.posRadius.z - obj.posRadius.z;
                float distance = sqrt(dx*dx + dy*dy + dz*dz);
                if (distance > 0) {
                    vector<double> direction = { dx / distance, dy / distance, dz / distance };
                    double Gforce = (G * obj.mass * obj2.mass) / (distance * distance);
                    double acc1 = Gforce / obj.mass;
                    std::vector<double> acc = { direction[0]*acc1, direction[1]*acc1, direction[2]*acc1 };
                    if (Gravity) {
                        obj.velocity.x += (float)acc[0];
                        obj.velocity.y += (float)acc[1];
                        obj.velocity.z += (float)acc[2];

                        obj.posRadius.x += obj.velocity.x;
                        obj.posRadius.y += obj.velocity.y;
                        obj.posRadius.z += obj.velocity.z;
                        // cout << "velocity: " << obj.velocity.x << ", " << obj.velocity.y << ", " << obj.velocity.z << endl;
                    }
                }
            }
        }

        // --- Clear ---
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- Grid ---
        engine.generateGrid(objects);
        mat4 view = lookAt(camera.position(), camera.target, vec3(0,1,0));
        mat4 proj = perspective(radians(60.0f), float(engine.WIDTH)/float(engine.HEIGHT), 1e9f, 1e14f);
        mat4 viewProj = proj * view;
        engine.drawGrid(viewProj);

        // --- Compute Raytracer -> Texture ---
        glViewport(0, 0, engine.WIDTH, engine.HEIGHT);
        engine.dispatchCompute(camera);
        engine.drawFullScreenQuad();

        // --- Present ---
        engine.window->display();
    }

    return 0;
}
