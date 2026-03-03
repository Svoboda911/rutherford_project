#include <iostream>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <string>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <utility>

#define _USE_MATH_DEFINES
#include <cmath>

#include <glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shaders.h"

#ifndef SHADER_DIR
#define SHADER_DIR "../shaders"
#endif

namespace {

constexpr int WINDOW_WIDTH = 1100;
constexpr int WINDOW_HEIGHT = 760;

constexpr int MAX_ACTIVE_PARTICLES = 420;
constexpr int TRAIL_LENGTH = 28;
constexpr int PROTON_MIN = 20;
constexpr int PROTON_MAX = 100;
constexpr int NEUTRON_MIN = 20;
constexpr int NEUTRON_MAX = 150;
constexpr int DEFAULT_PROTONS = 79;
constexpr int DEFAULT_NEUTRONS = 118;
constexpr int MAX_NUCLEONS = PROTON_MAX + NEUTRON_MAX;
constexpr int SHELL_SEGMENTS = 80;
constexpr int MAX_ELECTRON_SHELLS = 7;

constexpr float COULOMB_SCALE = 0.016f;
constexpr float PARTICLE_MASS = 10.0f;
constexpr float FIXED_DT = 1.0f / 120.0f;
constexpr int MAX_SIM_STEPS_PER_FRAME = 6;

constexpr float SPAWN_RATE = 34.0f; // particles per second
constexpr float PARTICLE_LIFETIME = 9.0f;

constexpr float BEAM_SPAWN_X = -1.95f;
constexpr float BEAM_TARGET_X = 0.20f;
constexpr float BEAM_Y_HALF = 0.11f;
constexpr float BEAM_Z_HALF = 0.08f;
constexpr float BEAM_SPEED = 0.70f;
constexpr float BEAM_SPEED_JITTER = 0.08f;
constexpr float BEAM_HALF_MIN = 0.03f;
constexpr float BEAM_HALF_MAX = 0.30f;
constexpr float BEAM_HALF_STEP = 0.01f;

constexpr bool SHOW_BEAM_GUIDE = true;
constexpr float BEAM_GUIDE_RADIUS = 0.07f;
constexpr int BEAM_GUIDE_SLICES = 14;
constexpr float BEAM_GUIDE_LINE_WIDTH = 2.4f;
constexpr bool PRINT_FPS_TO_CONSOLE = false;

constexpr float PANEL_LEFT_NDC = 0.66f;
constexpr float PANEL_RIGHT_NDC = 0.98f;
constexpr float PANEL_TOP_NDC = 0.94f;
constexpr float PANEL_BOTTOM_NDC = -0.94f;
constexpr float PANEL_INNER_LEFT = PANEL_LEFT_NDC + 0.030f;
constexpr float PANEL_INNER_RIGHT = PANEL_RIGHT_NDC - 0.030f;
constexpr float PANEL_BAR_HEIGHT = 0.036f;
constexpr std::size_t PANEL_MAX_VERTICES = 4096;

constexpr bool USE_VSYNC = true;
constexpr bool LIMIT_TO_60_FPS = false;
constexpr double TARGET_FRAME_SECONDS = 1.0 / 60.0;

constexpr float HEAD_POINT_SIZE = 9.4f;
constexpr float TRAIL_LINE_WIDTH = 2.7f;
constexpr bool USE_LINE_SMOOTH = false;

constexpr float X_MIN = -2.1f;
constexpr float X_MAX = 2.3f;
constexpr float Y_MIN = -1.7f;
constexpr float Y_MAX = 1.7f;
constexpr float Z_MIN = -1.7f;
constexpr float Z_MAX = 1.7f;
constexpr float NUCLEUS_BASE_RADIUS = 0.040f;
constexpr float NUCLEUS_RADIUS_SCALE = 0.0105f;
constexpr float TRAIL_NUCLEUS_MASK_SCALE = 1.08f;

int gFramebufferWidth = WINDOW_WIDTH;
int gFramebufferHeight = WINDOW_HEIGHT;
bool gShowHud = true;
bool gHudToggleLatch = false;

struct Particle {
    glm::vec3 pos;
    glm::vec3 vel;
    std::array<glm::vec3, TRAIL_LENGTH> trail;
    int trailCount = 0;
    int trailHead = 0;
    float age = 0.0f;

    void resetTrail(const glm::vec3& p) {
        trailCount = 1;
        trailHead = 1 % TRAIL_LENGTH;
        trail[0] = p;
    }

    void pushTrail(const glm::vec3& p) {
        trail[trailHead] = p;
        trailHead = (trailHead + 1) % TRAIL_LENGTH;
        if (trailCount < TRAIL_LENGTH) {
            trailCount++;
        }
    }
};

struct SphereMesh {
    std::vector<float> vertices; // pos (3) + normal (3)
    std::vector<unsigned int> indices;
};

struct Camera {
    glm::vec3 position = glm::vec3(0.0f, 0.9f, 4.8f);
    glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw = -90.0f;
    float pitch = -6.0f;
    float moveSpeed = 3.2f;
    float lookSpeed = 85.0f;
};

float randomFloat(float minV, float maxV) {
    return minV + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (maxV - minV);
}

float nucleusRadiusFromNucleons(int protonCount, int neutronCount) {
    const int massNumber = std::max(1, protonCount + neutronCount);
    return NUCLEUS_BASE_RADIUS + NUCLEUS_RADIUS_SCALE * std::cbrt(static_cast<float>(massNumber));
}

std::vector<glm::vec3> buildUnitSphereDistribution(int count) {
    std::vector<glm::vec3> unitPoints;
    unitPoints.reserve(static_cast<std::size_t>(count));
    constexpr float goldenAngle = 2.39996323f;

    for (int i = 0; i < count; ++i) {
        const float y = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
        const float radial = std::sqrt(std::max(0.0f, 1.0f - y * y));
        const float phi = goldenAngle * static_cast<float>(i);
        unitPoints.emplace_back(std::cos(phi) * radial, y, std::sin(phi) * radial);
    }

    return unitPoints;
}

int electronShellCount(int electronCount) {
    constexpr std::array<int, MAX_ELECTRON_SHELLS> shellCaps = {2, 8, 18, 32, 32, 18, 8};
    int remaining = std::max(0, electronCount);
    int shells = 0;
    for (int cap : shellCaps) {
        if (remaining <= 0) {
            break;
        }
        remaining -= std::min(remaining, cap);
        shells++;
    }
    return shells;
}

void appendQuad(std::vector<glm::vec3>& verts, float left, float bottom, float right, float top) {
    verts.emplace_back(left, bottom, 0.0f);
    verts.emplace_back(right, bottom, 0.0f);
    verts.emplace_back(right, top, 0.0f);
    verts.emplace_back(left, bottom, 0.0f);
    verts.emplace_back(right, top, 0.0f);
    verts.emplace_back(left, top, 0.0f);
}

void appendRectOutline(std::vector<glm::vec3>& verts, float left, float bottom, float right, float top) {
    verts.emplace_back(left, bottom, 0.0f);
    verts.emplace_back(right, bottom, 0.0f);
    verts.emplace_back(right, bottom, 0.0f);
    verts.emplace_back(right, top, 0.0f);
    verts.emplace_back(right, top, 0.0f);
    verts.emplace_back(left, top, 0.0f);
    verts.emplace_back(left, top, 0.0f);
    verts.emplace_back(left, bottom, 0.0f);
}

void updateCameraVectors(Camera& cam) {
    glm::vec3 dir;
    dir.x = std::cos(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
    dir.y = std::sin(glm::radians(cam.pitch));
    dir.z = std::sin(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
    cam.front = glm::normalize(dir);

    glm::vec3 right = glm::normalize(glm::cross(cam.front, cam.worldUp));
    cam.up = glm::normalize(glm::cross(right, cam.front));
}

void resetCamera(Camera& cam) {
    cam.position = glm::vec3(0.0f, 0.9f, 4.8f);
    cam.yaw = -90.0f;
    cam.pitch = -6.0f;
    updateCameraVectors(cam);
}

void processCameraInput(GLFWwindow* window, Camera& cam, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    const int hState = glfwGetKey(window, GLFW_KEY_H);
    if (hState == GLFW_PRESS && !gHudToggleLatch) {
        gShowHud = !gShowHud;
        gHudToggleLatch = true;
    } else if (hState == GLFW_RELEASE) {
        gHudToggleLatch = false;
    }

    float speed = cam.moveSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        speed *= 2.2f;
    }

    const float lookStep = cam.lookSpeed * dt;
    const glm::vec3 right = glm::normalize(glm::cross(cam.front, cam.worldUp));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cam.position += cam.front * speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cam.position -= cam.front * speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cam.position -= right * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cam.position += right * speed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) cam.position += cam.worldUp * speed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) cam.position -= cam.worldUp * speed;

    // Both arrow keys and IJKL are supported for looking around.
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) {
        cam.yaw -= lookStep;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
        cam.yaw += lookStep;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
        cam.pitch += lookStep;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
        cam.pitch -= lookStep;
    }

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        resetCamera(cam);
    }

    cam.pitch = std::clamp(cam.pitch, -89.0f, 89.0f);
    updateCameraVectors(cam);
}

SphereMesh buildSphereMesh(int slices, int stacks, float radius) {
    SphereMesh mesh;
    mesh.vertices.reserve(static_cast<std::size_t>((slices + 1) * (stacks + 1) * 6));
    mesh.indices.reserve(static_cast<std::size_t>(slices * stacks * 6));

    for (int st = 0; st <= stacks; ++st) {
        float v = static_cast<float>(st) / static_cast<float>(stacks);
        float phi = v * static_cast<float>(M_PI);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (int sl = 0; sl <= slices; ++sl) {
            float u = static_cast<float>(sl) / static_cast<float>(slices);
            float theta = u * 2.0f * static_cast<float>(M_PI);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            glm::vec3 normal(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
            glm::vec3 pos = radius * normal;

            mesh.vertices.push_back(pos.x);
            mesh.vertices.push_back(pos.y);
            mesh.vertices.push_back(pos.z);
            mesh.vertices.push_back(normal.x);
            mesh.vertices.push_back(normal.y);
            mesh.vertices.push_back(normal.z);
        }
    }

    for (int st = 0; st < stacks; ++st) {
        for (int sl = 0; sl < slices; ++sl) {
            unsigned int first = static_cast<unsigned int>(st * (slices + 1) + sl);
            unsigned int second = first + static_cast<unsigned int>(slices + 1);

            mesh.indices.push_back(first);
            mesh.indices.push_back(second);
            mesh.indices.push_back(first + 1);

            mesh.indices.push_back(first + 1);
            mesh.indices.push_back(second);
            mesh.indices.push_back(second + 1);
        }
    }

    return mesh;
}

Particle spawnParticle(float beamHalfY, float beamHalfZ) {
    Particle p;
    p.pos = glm::vec3(
        BEAM_SPAWN_X,
        randomFloat(-beamHalfY, beamHalfY),
        randomFloat(-beamHalfZ, beamHalfZ)
    );
    p.vel = glm::vec3(
        BEAM_SPEED + randomFloat(-BEAM_SPEED_JITTER, BEAM_SPEED_JITTER),
        randomFloat(-0.010f, 0.010f),
        randomFloat(-0.010f, 0.010f)
    );
    p.age = 0.0f;
    p.resetTrail(p.pos);
    return p;
}

bool updateParticle(
    Particle& p,
    const glm::vec3& nucleusPos,
    float nucleusRadius,
    int protonCount,
    float outerShellRadius,
    float dt
) {
    constexpr float qAlpha = 2.0f;
    const float qGold = static_cast<float>(std::max(1, protonCount));
    const float accelK = (COULOMB_SCALE * qAlpha * qGold) / PARTICLE_MASS;

    glm::vec3 dir = p.pos - nucleusPos;
    const float actualRSquared = glm::dot(dir, dir);
    const float actualR = std::sqrt(std::max(1.0e-8f, actualRSquared));
    float rSquared = actualRSquared;
    const float minRSquared = std::max(0.01f * 0.01f, nucleusRadius * nucleusRadius * 0.35f);
    rSquared = std::max(rSquared, minRSquared);
    const float invR = glm::inversesqrt(rSquared);
    const float invR3 = invR * invR * invR;
    float screening = 1.0f;
    if (outerShellRadius > 0.0f && actualR > outerShellRadius) {
        const float shellDelta = actualR - outerShellRadius;
        screening = std::clamp(std::exp(-1.9f * shellDelta), 0.18f, 1.0f);
    }
    const float accelScale = accelK * screening * invR3;
    p.vel += dir * (accelScale * dt);
    p.pos += p.vel * dt;
    p.age += dt;

    const glm::vec3 centerDelta = p.pos - nucleusPos;
    const float centerDistanceSquared = glm::dot(centerDelta, centerDelta);
    if (centerDistanceSquared < nucleusRadius * nucleusRadius) {
        return false;
    }

    if (p.pos.x < X_MIN || p.pos.x > X_MAX ||
        p.pos.y < Y_MIN || p.pos.y > Y_MAX ||
        p.pos.z < Z_MIN || p.pos.z > Z_MAX ||
        p.age > PARTICLE_LIFETIME) {
        return false;
    }

    p.pushTrail(p.pos);
    return true;
}

void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height) {
    gFramebufferWidth = std::max(1, width);
    gFramebufferHeight = std::max(1, height);
    glViewport(0, 0, gFramebufferWidth, gFramebufferHeight);
}

std::string resolveShaderPath(const char* fileName) {
    const std::array<std::filesystem::path, 5> roots = {
        std::filesystem::path(SHADER_DIR),
        std::filesystem::path("../shaders"),
        std::filesystem::path("../../shaders"),
        std::filesystem::path("shaders"),
        std::filesystem::path(".")
    };

    for (const auto& root : roots) {
        std::filesystem::path candidate = root / fileName;
        if (std::filesystem::exists(candidate)) {
            return candidate.lexically_normal().string();
        }
    }
    return {};
}

} // namespace

void program() {
    srand(static_cast<unsigned int>(time(nullptr)));

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Rutherford Scattering 3D", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(USE_VSYNC ? 1 : 0);
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    int initW = 1, initH = 1;
    glfwGetFramebufferSize(window, &initW, &initH);
    gFramebufferWidth = std::max(1, initW);
    gFramebufferHeight = std::max(1, initH);
    glViewport(0, 0, gFramebufferWidth, gFramebufferHeight);

    std::cout << "Controls: WASD move, Q/E up-down, IJKL or arrows look, Shift boost, R reset, H HUD, Esc quit" << std::endl;
    std::cout << "Atom controls: Z/X protons -, + | C/V neutrons -, + | N/M beam narrow/wide" << std::endl;
    std::cout << "Visual controls: 1 toggle electron shells | 2 toggle proton/neutron core" << std::endl;
    std::cout << "Tip: click the OpenGL window once to ensure keyboard focus." << std::endl;
    std::cout << "Render pacing: " << (USE_VSYNC ? "vsync" : "uncapped")
              << (LIMIT_TO_60_FPS ? " + manual 60fps cap" : "") << std::endl;
    std::cout << "Shader directory hint: " << SHADER_DIR << std::endl;
    glfwSetWindowTitle(
        window,
        "Rutherford 3D | WASD/QE/IJKL + Z/X/C/V atom controls"
    );

    const std::string vertexShaderPath = resolveShaderPath("vertex.glsl");
    const std::string fragmentShaderPath = resolveShaderPath("fragment.glsl");
    if (vertexShaderPath.empty() || fragmentShaderPath.empty()) {
        std::cerr << "Could not locate shader files.\n"
                  << "Checked from cwd: " << std::filesystem::current_path() << "\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }

    std::cout << "Using vertex shader: " << vertexShaderPath << std::endl;
    std::cout << "Using fragment shader: " << fragmentShaderPath << std::endl;

    shaderClass shader(vertexShaderPath.c_str(), fragmentShaderPath.c_str());
    if (shader.ID_program == 0) {
        std::cerr << "Shader program failed to initialize; aborting render loop." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }

    int locModel = glGetUniformLocation(shader.ID_program, "model");
    int locView = glGetUniformLocation(shader.ID_program, "view");
    int locProjection = glGetUniformLocation(shader.ID_program, "projection");
    int locColor = glGetUniformLocation(shader.ID_program, "color");
    int locOpacity = glGetUniformLocation(shader.ID_program, "opacity");
    int locUseLighting = glGetUniformLocation(shader.ID_program, "useLighting");
    int locLightPos = glGetUniformLocation(shader.ID_program, "lightPos");
    int locViewPos = glGetUniformLocation(shader.ID_program, "viewPos");
    if (locModel == -1 ||
        locView == -1 ||
        locProjection == -1 ||
        locColor == -1 ||
        locOpacity == -1 ||
        locUseLighting == -1 ||
        locLightPos == -1 ||
        locViewPos == -1) {
        std::cerr << "One or more required shader uniforms were not found." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }

    // Nucleus sphere mesh (3D)
    SphereMesh nucleusMesh = buildSphereMesh(24, 16, 1.0f);
    unsigned int nucleusVAO = 0, nucleusVBO = 0, nucleusEBO = 0;
    glGenVertexArrays(1, &nucleusVAO);
    glGenBuffers(1, &nucleusVBO);
    glGenBuffers(1, &nucleusEBO);

    glBindVertexArray(nucleusVAO);
    glBindBuffer(GL_ARRAY_BUFFER, nucleusVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(nucleusMesh.vertices.size() * sizeof(float)),
        nucleusMesh.vertices.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, nucleusEBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(nucleusMesh.indices.size() * sizeof(unsigned int)),
        nucleusMesh.indices.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // Batched particle heads (single draw call)
    std::vector<glm::vec3> headPositions;
    headPositions.reserve(static_cast<std::size_t>(MAX_ACTIVE_PARTICLES));
    const GLsizeiptr maxHeadBufferBytes = static_cast<GLsizeiptr>(
        static_cast<std::size_t>(MAX_ACTIVE_PARTICLES) * sizeof(glm::vec3)
    );

    unsigned int headVAO = 0, headVBO = 0;
    glGenVertexArrays(1, &headVAO);
    glGenBuffers(1, &headVBO);

    glBindVertexArray(headVAO);
    glBindBuffer(GL_ARRAY_BUFFER, headVBO);
    glBufferData(GL_ARRAY_BUFFER, maxHeadBufferBytes, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // Batched trails as GL_LINES (single draw call)
    std::vector<glm::vec3> trailVertices;
    trailVertices.reserve(
        static_cast<std::size_t>(MAX_ACTIVE_PARTICLES) * static_cast<std::size_t>((TRAIL_LENGTH - 1) * 2)
    );
    const GLsizeiptr maxTrailBufferBytes = static_cast<GLsizeiptr>(
        static_cast<std::size_t>(MAX_ACTIVE_PARTICLES) * static_cast<std::size_t>((TRAIL_LENGTH - 1) * 2) *
        sizeof(glm::vec3)
    );

    unsigned int trailVAO = 0, trailVBO = 0;
    glGenVertexArrays(1, &trailVAO);
    glGenBuffers(1, &trailVBO);

    glBindVertexArray(trailVAO);
    glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
    glBufferData(GL_ARRAY_BUFFER, maxTrailBufferBytes, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // Static beam guide (visual only, helps beam look continuous).
    std::vector<glm::vec3> beamVertices;
    beamVertices.reserve(static_cast<std::size_t>((BEAM_GUIDE_SLICES + 1) * 2));
    beamVertices.push_back(glm::vec3(BEAM_SPAWN_X, 0.0f, 0.0f));
    beamVertices.push_back(glm::vec3(BEAM_TARGET_X, 0.0f, 0.0f));
    for (int i = 0; i < BEAM_GUIDE_SLICES; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(BEAM_GUIDE_SLICES);
        const float angle = t * 2.0f * static_cast<float>(M_PI);
        const float yOffset = std::cos(angle) * BEAM_GUIDE_RADIUS;
        const float zOffset = std::sin(angle) * BEAM_GUIDE_RADIUS;
        beamVertices.push_back(glm::vec3(BEAM_SPAWN_X, yOffset, zOffset));
        beamVertices.push_back(glm::vec3(BEAM_TARGET_X, yOffset, zOffset));
    }

    unsigned int beamVAO = 0, beamVBO = 0;
    glGenVertexArrays(1, &beamVAO);
    glGenBuffers(1, &beamVBO);
    glBindVertexArray(beamVAO);
    glBindBuffer(GL_ARRAY_BUFFER, beamVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(beamVertices.size() * sizeof(glm::vec3)),
        beamVertices.data(),
        GL_STATIC_DRAW
    );
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    std::vector<glm::vec3> nucleonUnitPoints = buildUnitSphereDistribution(MAX_NUCLEONS);
    std::vector<glm::vec3> protonCorePositions;
    std::vector<glm::vec3> neutronCorePositions;
    protonCorePositions.reserve(static_cast<std::size_t>(PROTON_MAX));
    neutronCorePositions.reserve(static_cast<std::size_t>(NEUTRON_MAX));

    const GLsizeiptr maxProtonCoreBytes = static_cast<GLsizeiptr>(
        static_cast<std::size_t>(PROTON_MAX) * sizeof(glm::vec3)
    );
    const GLsizeiptr maxNeutronCoreBytes = static_cast<GLsizeiptr>(
        static_cast<std::size_t>(NEUTRON_MAX) * sizeof(glm::vec3)
    );

    unsigned int protonCoreVAO = 0, protonCoreVBO = 0;
    glGenVertexArrays(1, &protonCoreVAO);
    glGenBuffers(1, &protonCoreVBO);
    glBindVertexArray(protonCoreVAO);
    glBindBuffer(GL_ARRAY_BUFFER, protonCoreVBO);
    glBufferData(GL_ARRAY_BUFFER, maxProtonCoreBytes, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    unsigned int neutronCoreVAO = 0, neutronCoreVBO = 0;
    glGenVertexArrays(1, &neutronCoreVAO);
    glGenBuffers(1, &neutronCoreVBO);
    glBindVertexArray(neutronCoreVAO);
    glBindBuffer(GL_ARRAY_BUFFER, neutronCoreVBO);
    glBufferData(GL_ARRAY_BUFFER, maxNeutronCoreBytes, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    std::vector<glm::vec3> electronShellVertices;
    electronShellVertices.reserve(static_cast<std::size_t>(MAX_ELECTRON_SHELLS) * static_cast<std::size_t>(SHELL_SEGMENTS + 1));
    const GLsizeiptr maxShellBytes = static_cast<GLsizeiptr>(
        static_cast<std::size_t>(MAX_ELECTRON_SHELLS) * static_cast<std::size_t>(SHELL_SEGMENTS + 1) * sizeof(glm::vec3)
    );

    unsigned int shellVAO = 0, shellVBO = 0;
    glGenVertexArrays(1, &shellVAO);
    glGenBuffers(1, &shellVBO);
    glBindVertexArray(shellVAO);
    glBindBuffer(GL_ARRAY_BUFFER, shellVBO);
    glBufferData(GL_ARRAY_BUFFER, maxShellBytes, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    std::vector<glm::vec3> panelFillVertices;
    panelFillVertices.reserve(static_cast<std::size_t>(PANEL_MAX_VERTICES));
    std::vector<glm::vec3> panelLineVertices;
    panelLineVertices.reserve(static_cast<std::size_t>(PANEL_MAX_VERTICES));
    const GLsizeiptr maxPanelBytes = static_cast<GLsizeiptr>(
        static_cast<std::size_t>(PANEL_MAX_VERTICES) * sizeof(glm::vec3)
    );

    unsigned int panelVAO = 0, panelVBO = 0;
    glGenVertexArrays(1, &panelVAO);
    glGenBuffers(1, &panelVBO);
    glBindVertexArray(panelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, panelVBO);
    glBufferData(GL_ARRAY_BUFFER, maxPanelBytes, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // Simulation state
    std::vector<Particle> particles;
    particles.reserve(static_cast<std::size_t>(MAX_ACTIVE_PARTICLES));
    double spawnAccumulator = 0.0;
    int protonCount = DEFAULT_PROTONS;
    int neutronCount = DEFAULT_NEUTRONS;
    float beamHalfY = BEAM_Y_HALF;
    float beamHalfZ = BEAM_Z_HALF;
    bool showElectronShells = true;
    bool showNucleonCore = true;
    bool protonDownLatch = false;
    bool protonUpLatch = false;
    bool neutronDownLatch = false;
    bool neutronUpLatch = false;
    bool beamNarrowLatch = false;
    bool beamWideLatch = false;
    bool shellsToggleLatch = false;
    bool coreToggleLatch = false;

    Camera cam;
    resetCamera(cam);

    glm::vec3 nucleusPos(0.20f, 0.0f, 0.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (USE_LINE_SMOOTH) {
        glEnable(GL_LINE_SMOOTH);
    } else {
        glDisable(GL_LINE_SMOOTH);
    }

    shader.use();
    glUniform3f(locLightPos, 1.9f, 2.1f, 2.5f);

    glm::mat4 projection(1.0f);
    const glm::mat4 panelProjection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    int projectionW = -1;
    int projectionH = -1;

    float lastTime = static_cast<float>(glfwGetTime());
    double simAccumulator = 0.0;
    double fpsAccumSeconds = 0.0;
    int fpsAccumFrames = 0;
    auto keyPressedOnce = [&](int key, bool& latch) -> bool {
        const int state = glfwGetKey(window, key);
        if (state == GLFW_PRESS && !latch) {
            latch = true;
            return true;
        }
        if (state == GLFW_RELEASE) {
            latch = false;
        }
        return false;
    };

    while (!glfwWindowShouldClose(window)) {
        const double frameStart = glfwGetTime();

        float currentTime = static_cast<float>(glfwGetTime());
        float frameDt = currentTime - lastTime;
        lastTime = currentTime;
        frameDt = std::clamp(frameDt, 0.0f, 0.10f);

        glfwPollEvents();
        processCameraInput(window, cam, frameDt);

        if (keyPressedOnce(GLFW_KEY_Z, protonDownLatch)) {
            protonCount = std::max(PROTON_MIN, protonCount - 1);
        }
        if (keyPressedOnce(GLFW_KEY_X, protonUpLatch)) {
            protonCount = std::min(PROTON_MAX, protonCount + 1);
        }
        if (keyPressedOnce(GLFW_KEY_C, neutronDownLatch)) {
            neutronCount = std::max(NEUTRON_MIN, neutronCount - 1);
        }
        if (keyPressedOnce(GLFW_KEY_V, neutronUpLatch)) {
            neutronCount = std::min(NEUTRON_MAX, neutronCount + 1);
        }
        if (keyPressedOnce(GLFW_KEY_N, beamNarrowLatch)) {
            beamHalfY = std::max(BEAM_HALF_MIN, beamHalfY - BEAM_HALF_STEP);
            beamHalfZ = std::max(BEAM_HALF_MIN, beamHalfZ - BEAM_HALF_STEP);
        }
        if (keyPressedOnce(GLFW_KEY_M, beamWideLatch)) {
            beamHalfY = std::min(BEAM_HALF_MAX, beamHalfY + BEAM_HALF_STEP);
            beamHalfZ = std::min(BEAM_HALF_MAX, beamHalfZ + BEAM_HALF_STEP);
        }
        if (keyPressedOnce(GLFW_KEY_1, shellsToggleLatch)) {
            showElectronShells = !showElectronShells;
        }
        if (keyPressedOnce(GLFW_KEY_2, coreToggleLatch)) {
            showNucleonCore = !showNucleonCore;
        }

        const float nucleusRadius = nucleusRadiusFromNucleons(protonCount, neutronCount);
        const int electronShells = electronShellCount(protonCount);
        const float outerShellRadius = (electronShells > 0)
            ? (nucleusRadius + 0.08f + 0.085f * static_cast<float>(electronShells - 1))
            : nucleusRadius;

        simAccumulator += static_cast<double>(frameDt);
        int simSteps = 0;
        while (simAccumulator >= static_cast<double>(FIXED_DT) &&
               simSteps < MAX_SIM_STEPS_PER_FRAME) {
            spawnAccumulator += static_cast<double>(SPAWN_RATE * FIXED_DT);
            if (particles.size() >= static_cast<std::size_t>(MAX_ACTIVE_PARTICLES)) {
                // Avoid backlog bursts when we are already saturated.
                spawnAccumulator = 0.0;
            }
            while (spawnAccumulator >= 1.0 &&
                   particles.size() < static_cast<std::size_t>(MAX_ACTIVE_PARTICLES)) {
                particles.push_back(spawnParticle(beamHalfY, beamHalfZ));
                spawnAccumulator -= 1.0;
            }

            std::size_t aliveCount = 0;
            for (std::size_t i = 0; i < particles.size(); ++i) {
                if (updateParticle(particles[i], nucleusPos, nucleusRadius, protonCount, outerShellRadius, FIXED_DT)) {
                    if (aliveCount != i) {
                        particles[aliveCount] = std::move(particles[i]);
                    }
                    aliveCount++;
                }
            }
            particles.resize(aliveCount);

            simAccumulator -= static_cast<double>(FIXED_DT);
            simSteps++;
        }
        if (simSteps == MAX_SIM_STEPS_PER_FRAME) {
            simAccumulator = 0.0;
        }

        // Rebuild batched render buffers
        headPositions.clear();
        trailVertices.clear();
        protonCorePositions.clear();
        neutronCorePositions.clear();
        electronShellVertices.clear();
        const float trailMaskRadius = nucleusRadius * TRAIL_NUCLEUS_MASK_SCALE;
        const float trailMaskRadiusSquared = trailMaskRadius * trailMaskRadius;
        for (const Particle& p : particles) {
            headPositions.push_back(p.pos);
            if (p.trailCount >= 2) {
                int idx = (p.trailHead - p.trailCount + TRAIL_LENGTH) % TRAIL_LENGTH;
                glm::vec3 prev = p.trail[idx];
                idx = (idx + 1) % TRAIL_LENGTH;
                for (int j = 1; j < p.trailCount; ++j) {
                    const glm::vec3 curr = p.trail[idx];
                    const glm::vec3 prevToNucleus = prev - nucleusPos;
                    const glm::vec3 currToNucleus = curr - nucleusPos;
                    const bool prevInside = glm::dot(prevToNucleus, prevToNucleus) <= trailMaskRadiusSquared;
                    const bool currInside = glm::dot(currToNucleus, currToNucleus) <= trailMaskRadiusSquared;
                    if (!prevInside && !currInside) {
                        trailVertices.push_back(prev);
                        trailVertices.push_back(curr);
                    }
                    prev = curr;
                    idx = (idx + 1) % TRAIL_LENGTH;
                }
            }
        }

        const int totalNucleons = protonCount + neutronCount;
        for (int i = 0; i < totalNucleons; ++i) {
            const float t = (static_cast<float>(i) + 0.5f) / (static_cast<float>(totalNucleons) + 0.5f);
            const float radialFactor = std::cbrt(t);
            const glm::vec3 offset = nucleonUnitPoints[static_cast<std::size_t>(i)] * (radialFactor * nucleusRadius * 0.95f);
            if (i < protonCount) {
                protonCorePositions.push_back(nucleusPos + offset);
            } else {
                neutronCorePositions.push_back(nucleusPos + offset);
            }
        }

        const int shellCount = electronShellCount(protonCount);
        for (int shell = 0; shell < shellCount; ++shell) {
            const float shellRadius = nucleusRadius + 0.08f + 0.085f * static_cast<float>(shell);
            for (int seg = 0; seg <= SHELL_SEGMENTS; ++seg) {
                const float angle = (static_cast<float>(seg) / static_cast<float>(SHELL_SEGMENTS)) * 2.0f * static_cast<float>(M_PI);
                electronShellVertices.emplace_back(
                    nucleusPos.x + std::cos(angle) * shellRadius,
                    nucleusPos.y,
                    nucleusPos.z + std::sin(angle) * shellRadius
                );
            }
        }

        const GLsizeiptr headUploadBytes = static_cast<GLsizeiptr>(headPositions.size() * sizeof(glm::vec3));
        const GLsizeiptr trailUploadBytes = static_cast<GLsizeiptr>(trailVertices.size() * sizeof(glm::vec3));
        const GLsizeiptr protonCoreBytes = static_cast<GLsizeiptr>(protonCorePositions.size() * sizeof(glm::vec3));
        const GLsizeiptr neutronCoreBytes = static_cast<GLsizeiptr>(neutronCorePositions.size() * sizeof(glm::vec3));
        const GLsizeiptr electronShellBytes = static_cast<GLsizeiptr>(electronShellVertices.size() * sizeof(glm::vec3));

        glBindBuffer(GL_ARRAY_BUFFER, headVBO);
        glBufferData(GL_ARRAY_BUFFER, maxHeadBufferBytes, nullptr, GL_DYNAMIC_DRAW);
        if (headUploadBytes > 0) {
            glBufferSubData(GL_ARRAY_BUFFER, 0, headUploadBytes, headPositions.data());
        }

        glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
        glBufferData(GL_ARRAY_BUFFER, maxTrailBufferBytes, nullptr, GL_DYNAMIC_DRAW);
        if (trailUploadBytes > 0) {
            glBufferSubData(GL_ARRAY_BUFFER, 0, trailUploadBytes, trailVertices.data());
        }

        glBindBuffer(GL_ARRAY_BUFFER, protonCoreVBO);
        glBufferData(GL_ARRAY_BUFFER, maxProtonCoreBytes, nullptr, GL_DYNAMIC_DRAW);
        if (protonCoreBytes > 0) {
            glBufferSubData(GL_ARRAY_BUFFER, 0, protonCoreBytes, protonCorePositions.data());
        }

        glBindBuffer(GL_ARRAY_BUFFER, neutronCoreVBO);
        glBufferData(GL_ARRAY_BUFFER, maxNeutronCoreBytes, nullptr, GL_DYNAMIC_DRAW);
        if (neutronCoreBytes > 0) {
            glBufferSubData(GL_ARRAY_BUFFER, 0, neutronCoreBytes, neutronCorePositions.data());
        }

        glBindBuffer(GL_ARRAY_BUFFER, shellVBO);
        glBufferData(GL_ARRAY_BUFFER, maxShellBytes, nullptr, GL_DYNAMIC_DRAW);
        if (electronShellBytes > 0) {
            glBufferSubData(GL_ARRAY_BUFFER, 0, electronShellBytes, electronShellVertices.data());
        }

        if (gFramebufferWidth != projectionW || gFramebufferHeight != projectionH) {
            projectionW = gFramebufferWidth;
            projectionH = gFramebufferHeight;
            const float aspect = static_cast<float>(projectionW) / static_cast<float>(projectionH);
            projection = glm::perspective(glm::radians(52.0f), aspect, 0.1f, 18.0f);
        }

        glm::mat4 view = glm::lookAt(cam.position, cam.position + cam.front, cam.up);
        glm::mat4 identity(1.0f);
        glm::mat4 nucleusModel =
            glm::translate(glm::mat4(1.0f), nucleusPos) *
            glm::scale(glm::mat4(1.0f), glm::vec3(nucleusRadius));

        glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(locProjection, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(locViewPos, 1, glm::value_ptr(cam.position));

        // Draw nucleus with lighting (3D shading)
        glUniform1i(locUseLighting, 1);
        glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(nucleusModel));
        glUniform3f(locColor, 0.95f, 0.78f, 0.16f);
        glUniform1f(locOpacity, 1.0f);
        glBindVertexArray(nucleusVAO);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(nucleusMesh.indices.size()),
            GL_UNSIGNED_INT,
            nullptr
        );

        if (showNucleonCore) {
            glUniform1i(locUseLighting, 0);
            glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(identity));

            glUniform3f(locColor, 0.96f, 0.42f, 0.24f);
            glUniform1f(locOpacity, 0.92f);
            glBindVertexArray(protonCoreVAO);
            glPointSize(4.6f);
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(protonCorePositions.size()));

            glUniform3f(locColor, 0.78f, 0.80f, 0.84f);
            glUniform1f(locOpacity, 0.86f);
            glBindVertexArray(neutronCoreVAO);
            glPointSize(3.8f);
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(neutronCorePositions.size()));
        }

        if (showElectronShells) {
            const int shellCount = electronShellCount(protonCount);
            if (shellCount > 0) {
                glUniform1i(locUseLighting, 0);
                glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(identity));
                glUniform3f(locColor, 0.88f, 0.90f, 0.62f);
                glUniform1f(locOpacity, 0.25f);
                glBindVertexArray(shellVAO);
                glLineWidth(1.2f);
                for (int shell = 0; shell < shellCount; ++shell) {
                    const GLsizei shellStart = static_cast<GLsizei>(shell * (SHELL_SEGMENTS + 1));
                    glDrawArrays(GL_LINE_STRIP, shellStart, SHELL_SEGMENTS + 1);
                }
            }
        }

        if (SHOW_BEAM_GUIDE) {
            glUniform1i(locUseLighting, 0);
            glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(identity));
            glUniform3f(locColor, 0.62f, 0.88f, 1.0f);
            glUniform1f(locOpacity, 0.30f);
            glBindVertexArray(beamVAO);
            glLineWidth(BEAM_GUIDE_LINE_WIDTH);
            glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(beamVertices.size()));
        }

        // Draw all trails in one call
        glUniform1i(locUseLighting, 0);
        glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(identity));
        glUniform3f(locColor, 0.44f, 0.76f, 1.0f);
        glUniform1f(locOpacity, 0.97f);
        glBindVertexArray(trailVAO);
        glLineWidth(TRAIL_LINE_WIDTH);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(trailVertices.size()));

        // Draw all heads in one call
        glUniform3f(locColor, 1.0f, 0.66f, 0.25f);
        glUniform1f(locOpacity, 1.0f);
        glBindVertexArray(headVAO);
        glPointSize(HEAD_POINT_SIZE);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(headPositions.size()));

        const auto uploadAndDrawPanel = [&](const std::vector<glm::vec3>& verts, GLenum mode, const glm::vec3& color, float opacity) {
            if (verts.empty()) {
                return;
            }
            glBindBuffer(GL_ARRAY_BUFFER, panelVBO);
            glBufferSubData(
                GL_ARRAY_BUFFER,
                0,
                static_cast<GLsizeiptr>(verts.size() * sizeof(glm::vec3)),
                verts.data()
            );
            glUniform3f(locColor, color.r, color.g, color.b);
            glUniform1f(locOpacity, opacity);
            glDrawArrays(mode, 0, static_cast<GLsizei>(verts.size()));
        };

        const float protonNorm =
            static_cast<float>(protonCount - PROTON_MIN) / static_cast<float>(PROTON_MAX - PROTON_MIN);
        const float neutronNorm =
            static_cast<float>(neutronCount - NEUTRON_MIN) / static_cast<float>(NEUTRON_MAX - NEUTRON_MIN);
        const float beamNorm =
            (beamHalfY - BEAM_HALF_MIN) / (BEAM_HALF_MAX - BEAM_HALF_MIN);
        const float electronEnergyNorm =
            static_cast<float>(electronShells) / static_cast<float>(MAX_ELECTRON_SHELLS);

        glDisable(GL_DEPTH_TEST);
        glUniform1i(locUseLighting, 0);
        glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(identity));
        glUniformMatrix4fv(locProjection, 1, GL_FALSE, glm::value_ptr(panelProjection));
        glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(identity));
        glBindVertexArray(panelVAO);

        panelFillVertices.clear();
        appendQuad(panelFillVertices, PANEL_LEFT_NDC, PANEL_BOTTOM_NDC, PANEL_RIGHT_NDC, PANEL_TOP_NDC);
        uploadAndDrawPanel(panelFillVertices, GL_TRIANGLES, glm::vec3(0.06f, 0.08f, 0.11f), 0.86f);

        panelLineVertices.clear();
        appendRectOutline(panelLineVertices, PANEL_LEFT_NDC, PANEL_BOTTOM_NDC, PANEL_RIGHT_NDC, PANEL_TOP_NDC);
        uploadAndDrawPanel(panelLineVertices, GL_LINES, glm::vec3(0.62f, 0.74f, 0.88f), 0.78f);

        const auto drawBar = [&](float yCenter, float value, const glm::vec3& fillColor) {
            const float clamped = std::clamp(value, 0.0f, 1.0f);
            const float y0 = yCenter - PANEL_BAR_HEIGHT * 0.5f;
            const float y1 = yCenter + PANEL_BAR_HEIGHT * 0.5f;
            const float x0 = PANEL_INNER_LEFT;
            const float x1 = PANEL_INNER_RIGHT;
            const float xFill = x0 + (x1 - x0) * clamped;

            panelFillVertices.clear();
            appendQuad(panelFillVertices, x0, y0, x1, y1);
            uploadAndDrawPanel(panelFillVertices, GL_TRIANGLES, glm::vec3(0.14f, 0.17f, 0.22f), 0.92f);

            panelFillVertices.clear();
            appendQuad(panelFillVertices, x0, y0, xFill, y1);
            uploadAndDrawPanel(panelFillVertices, GL_TRIANGLES, fillColor, 0.92f);

            panelLineVertices.clear();
            appendRectOutline(panelLineVertices, x0, y0, x1, y1);
            uploadAndDrawPanel(panelLineVertices, GL_LINES, glm::vec3(0.66f, 0.76f, 0.92f), 0.70f);

            const float knobHalfX = 0.008f;
            const float knobPadY = 0.006f;
            panelFillVertices.clear();
            appendQuad(panelFillVertices, xFill - knobHalfX, y0 - knobPadY, xFill + knobHalfX, y1 + knobPadY);
            uploadAndDrawPanel(panelFillVertices, GL_TRIANGLES, glm::vec3(0.93f, 0.95f, 0.98f), 0.95f);
        };

        drawBar(0.68f, protonNorm, glm::vec3(0.95f, 0.38f, 0.20f));         // protons
        drawBar(0.52f, neutronNorm, glm::vec3(0.74f, 0.76f, 0.82f));        // neutrons
        drawBar(0.36f, beamNorm, glm::vec3(0.24f, 0.80f, 0.94f));           // beam width
        drawBar(0.20f, electronEnergyNorm, glm::vec3(0.93f, 0.87f, 0.28f)); // electron energy (horizontal)

        panelFillVertices.clear();
        const float toggleY = 0.02f;
        appendQuad(panelFillVertices, PANEL_INNER_LEFT, toggleY - 0.022f, PANEL_INNER_LEFT + 0.040f, toggleY + 0.022f);
        uploadAndDrawPanel(
            panelFillVertices,
            GL_TRIANGLES,
            showElectronShells ? glm::vec3(0.88f, 0.90f, 0.62f) : glm::vec3(0.25f, 0.27f, 0.30f),
            0.92f
        );
        panelFillVertices.clear();
        appendQuad(panelFillVertices, PANEL_INNER_LEFT + 0.062f, toggleY - 0.022f, PANEL_INNER_LEFT + 0.102f, toggleY + 0.022f);
        uploadAndDrawPanel(
            panelFillVertices,
            GL_TRIANGLES,
            showNucleonCore ? glm::vec3(0.95f, 0.60f, 0.30f) : glm::vec3(0.25f, 0.27f, 0.30f),
            0.92f
        );

        glEnable(GL_DEPTH_TEST);

        glBindVertexArray(0);
        glfwSwapBuffers(window);

        if (LIMIT_TO_60_FPS && !USE_VSYNC) {
            const double frameElapsed = glfwGetTime() - frameStart;
            if (frameElapsed < TARGET_FRAME_SECONDS) {
                std::this_thread::sleep_for(
                    std::chrono::duration<double>(TARGET_FRAME_SECONDS - frameElapsed)
                );
            }
        }

        const double frameEnd = glfwGetTime();
        const double frameDuration = frameEnd - frameStart;
        fpsAccumSeconds += frameDuration;
        fpsAccumFrames++;
        if (fpsAccumSeconds >= 1.0) {
            const double fpsAverage = static_cast<double>(fpsAccumFrames) / fpsAccumSeconds;
            if (PRINT_FPS_TO_CONSOLE) {
                std::cout << "FPS(avg): " << fpsAverage
                          << " | Particles(active): " << particles.size() << "/" << MAX_ACTIVE_PARTICLES
                          << " | TrailLen: " << TRAIL_LENGTH << '\n';
            }

            std::ostringstream title;
            title << "Rutherford 3D | FPS(avg): " << static_cast<int>(fpsAverage + 0.5);
            if (gShowHud) {
                title << " | Z=" << protonCount
                      << " N=" << neutronCount
                      << " beam=" << static_cast<int>(beamHalfY * 100.0f)
                      << " | Z/X C/V N/M 1/2";
            } else {
                title << " | Press H for HUD";
            }
            glfwSetWindowTitle(window, title.str().c_str());

            fpsAccumSeconds = 0.0;
            fpsAccumFrames = 0;
        }
    }

    glDeleteVertexArrays(1, &nucleusVAO);
    glDeleteBuffers(1, &nucleusVBO);
    glDeleteBuffers(1, &nucleusEBO);

    glDeleteVertexArrays(1, &headVAO);
    glDeleteBuffers(1, &headVBO);

    glDeleteVertexArrays(1, &trailVAO);
    glDeleteBuffers(1, &trailVBO);

    glDeleteVertexArrays(1, &beamVAO);
    glDeleteBuffers(1, &beamVBO);

    glDeleteVertexArrays(1, &protonCoreVAO);
    glDeleteBuffers(1, &protonCoreVBO);

    glDeleteVertexArrays(1, &neutronCoreVAO);
    glDeleteBuffers(1, &neutronCoreVBO);

    glDeleteVertexArrays(1, &shellVAO);
    glDeleteBuffers(1, &shellVBO);

    glDeleteVertexArrays(1, &panelVAO);
    glDeleteBuffers(1, &panelVBO);

    glfwDestroyWindow(window);
    glfwTerminate();
}

int main() {
    program();
    return 0;
}
