#include "GlobeViewer.h"
#include <iostream>
#include <fstream>
#include <sstream>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

auto globe_logger = spdlog::stdout_color_mt("globe_console");
auto globe_err_logger = spdlog::stderr_color_mt("globe_stderr");

// Basic vertex shader for the globe
const char* globeVertexShader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
    Normal = mat3(transpose(inverse(model))) * aNormal;
    FragPos = vec3(model * vec4(aPos, 1.0));
}
)";

// Basic fragment shader for the globe
const char* globeFragmentShader = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D earthTexture;
uniform vec3 lightDir;
uniform vec3 viewPos;

void main()
{
    vec3 color = texture(earthTexture, TexCoord).rgb;

    // Simple lighting
    vec3 norm = normalize(Normal);
    vec3 lightColor = vec3(1.0);

    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 result = (ambient + diffuse) * color;
    FragColor = vec4(result, 1.0);
}
)";

GlobeViewer::GlobeViewer()
    : m_framebuffer(0), m_colorTexture(0), m_depthTexture(0)
      , m_globeVAO(0), m_globeVBO(0), m_globeEBO(0)
      , m_pathVAO(0), m_pathVBO(0), m_shaderProgram(0), m_earthTexture(0)
      , m_width(800), m_height(600)
      , m_cameraDistance(3.0f), m_cameraRotationX(0.0f), m_cameraRotationY(0.0f)
      , m_isDragging(false), m_propResults(), m_currentTimeStep(0), m_animating(false)
      , m_animationSpeed(1.0f), m_showPaths(true), m_showCurrentPositions(true)
      , m_pathOpacity(0.7f) {
    m_lastMousePos = ImVec2(0, 0);
}

GlobeViewer::~GlobeViewer() {
    Shutdown();
}

bool GlobeViewer::Initialize(int width, int height) {
    m_width = width;
    m_height = height;

    // Initialize OpenGL objects
    if (!CreateFramebuffer()) {
        globe_err_logger->error("Failed to create framebuffer");
        return false;
    }


    if (!LoadShaders()) {
        globe_err_logger->error("Failed to load shaders");
        return false;
    }

    CreateGlobeGeometry();

    // Setup matrices
    m_projMatrix = glm::perspective(glm::radians(45.0f),
                                   (float)m_width / (float)m_height,
                                   0.1f, 100.0f);

    // Create VAOs for paths
    glGenVertexArrays(1, &m_pathVAO);
    glGenBuffers(1, &m_pathVBO);

    UpdateCamera();

    return true;
}

bool GlobeViewer::CreateFramebuffer() {
    // Generate framebuffer
    glGenFramebuffers(1, &m_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    // Color texture
    glGenTextures(1, &m_colorTexture);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);

    // Depth texture
    glGenTextures(1, &m_depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer not complete!" << std::endl;
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool GlobeViewer::LoadShaders() {
    // Vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &globeVertexShader, NULL);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "Vertex shader compilation failed: " << infoLog << std::endl;
        return false;
    }

    // Fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &globeFragmentShader, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "Fragment shader compilation failed: " << infoLog << std::endl;
        return false;
    }

    // Shader program
    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertexShader);
    glAttachShader(m_shaderProgram, fragmentShader);
    glLinkProgram(m_shaderProgram);

    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(m_shaderProgram, 512, NULL, infoLog);
        globe_err_logger->error("Shader program linking failed: {}", infoLog);
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return true;
}

void GlobeViewer::CreateGlobeGeometry() {
    const int latSegments = 50;
    const int lonSegments = 50;
    const float radius = 1.0f;

    m_globeVertices.clear();
    m_globeIndices.clear();

    // Generate vertices
    for (int lat = 0; lat <= latSegments; ++lat) {
        float theta = lat * M_PI / latSegments;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);

        for (int lon = 0; lon <= lonSegments; ++lon) {
            float phi = lon * 2 * M_PI / lonSegments;
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);

            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            float u = 1.0f - (float)lon / lonSegments;
            float v = 1.0f - (float)lat / latSegments;

            // Position
            m_globeVertices.push_back(radius * x);
            m_globeVertices.push_back(radius * y);
            m_globeVertices.push_back(radius * z);

            // Texture coordinates
            m_globeVertices.push_back(u);
            m_globeVertices.push_back(v);

            // Normal
            m_globeVertices.push_back(x);
            m_globeVertices.push_back(y);
            m_globeVertices.push_back(z);
        }
    }

    // Generate indices
    for (int lat = 0; lat < latSegments; ++lat) {
        for (int lon = 0; lon < lonSegments; ++lon) {
            int first = lat * (lonSegments + 1) + lon;
            int second = first + lonSegments + 1;

            m_globeIndices.push_back(first);
            m_globeIndices.push_back(second);
            m_globeIndices.push_back(first + 1);

            m_globeIndices.push_back(second);
            m_globeIndices.push_back(second + 1);
            m_globeIndices.push_back(first + 1);
        }
    }

    // Create VAO/VBO/EBO
    glGenVertexArrays(1, &m_globeVAO);
    glGenBuffers(1, &m_globeVBO);
    glGenBuffers(1, &m_globeEBO);

    glBindVertexArray(m_globeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_globeVBO);
    glBufferData(GL_ARRAY_BUFFER, m_globeVertices.size() * sizeof(float),
                 m_globeVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_globeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_globeIndices.size() * sizeof(unsigned int),
                 m_globeIndices.data(), GL_STATIC_DRAW);

    // Position attribute (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coord attribute (location = 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Normal attribute (location = 2)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void GlobeViewer::UpdateCamera() {
    glm::vec3 position;
    position.x = m_cameraDistance * cos(glm::radians(m_cameraRotationY)) * cos(glm::radians(m_cameraRotationX));
    position.y = m_cameraDistance * sin(glm::radians(m_cameraRotationX));
    position.z = m_cameraDistance * sin(glm::radians(m_cameraRotationY)) * cos(glm::radians(m_cameraRotationX));

    m_viewMatrix = glm::lookAt(position, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

void GlobeViewer::SetPropagationResults(const PropagationResults& results) {
    m_propResults = results;

    // Initialize visible satellites (show all by default)
    m_visibleSatellites.clear();
    for (int i = 0; i < results.satellites.size(); ++i) {
        m_visibleSatellites.push_back(i);
    }

    m_currentTimeStep = 0;
}

void GlobeViewer::SetVisibleSatellites(const std::vector<int>& satelliteIndices) {
    m_visibleSatellites = satelliteIndices;
}

void GlobeViewer::Render() {
    if (!m_framebuffer) return;

    // Render to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glViewport(0, 0, m_width, m_height);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    RenderGlobe();
    RenderSatellitePaths();
    RenderSatellitePositions();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Render ImGui window
    RenderUI();
}

void GlobeViewer::RenderGlobe() {
    if (!m_shaderProgram || !m_globeVAO) return;

    glUseProgram(m_shaderProgram);

    // Set matrices
    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(m_viewMatrix));
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(m_projMatrix));

    // Set light direction (sun direction)
    glm::vec3 lightDir = glm::normalize(glm::vec3(1.0f, 0.5f, 0.2f));
    glUniform3fv(glGetUniformLocation(m_shaderProgram, "lightDir"), 1, glm::value_ptr(lightDir));

    // Bind earth texture if available
    if (m_earthTexture) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_earthTexture);
        glUniform1i(glGetUniformLocation(m_shaderProgram, "earthTexture"), 0);
    }
    
    glBindVertexArray(m_globeVAO);
    glDrawElements(GL_TRIANGLES, m_globeIndices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void GlobeViewer::RenderSatellitePaths() {
    // TODO: Implement satellite path rendering
    // This will render orbital tracks as line strips
}

void GlobeViewer::RenderSatellitePositions() {
    // TODO: Implement current satellite position rendering
    // This will render satellites as small spheres or points
}

void GlobeViewer::RenderUI() {
    ImGui::Begin("Globe Viewer");
    
    // Display the rendered globe
    ImVec2 size = ImGui::GetContentRegionAvail();
    if (size.x > 0 && size.y > 0) {
        // Handle mouse input for camera control
        HandleMouseInput();
        
        ImGui::Image((void*)(intptr_t)m_colorTexture, size, ImVec2(0, 1), ImVec2(1, 0));
    }
    
    // Controls
    ImGui::Separator();
    ImGui::SliderFloat("Camera Distance", &m_cameraDistance, 1.5f, 10.0f);
    if (ImGui::SliderFloat("Rotation X", &m_cameraRotationX, -90.0f, 90.0f) ||
        ImGui::SliderFloat("Rotation Y", &m_cameraRotationY, 0.0f, 360.0f)) {
        UpdateCamera();
    }
    
    ImGui::Checkbox("Show Paths", &m_showPaths);
    ImGui::Checkbox("Show Current Positions", &m_showCurrentPositions);
    ImGui::SliderFloat("Path Opacity", &m_pathOpacity, 0.1f, 1.0f);
    
    // Animation controls
    ImGui::Separator();
    ImGui::Text("Animation");
    if (ImGui::Button(m_animating ? "Pause" : "Play")) {
        m_animating = !m_animating;
    }
    ImGui::SameLine();
    ImGui::SliderFloat("Speed", &m_animationSpeed, 0.1f, 10.0f);
    
    // Time step control
    if (!m_propResults.satellites.empty()) {
        int maxTimeSteps = 0;
        for (const auto& sat : m_propResults.satellites) {
            maxTimeSteps = std::max(maxTimeSteps, (int)sat.timeSteps.size());
        }
        if (maxTimeSteps > 0) {
            ImGui::SliderInt("Time Step", &m_currentTimeStep, 0, maxTimeSteps - 1);
        }
    }
    
    ImGui::End();
}

void GlobeViewer::HandleMouseInput() {
    // TODO: Implement mouse dragging for camera rotation
    // This would handle mouse input within the ImGui image area
}

glm::vec3 GlobeViewer::LLHToCartesian(double lat, double lon, double height) {
    // Convert lat/lon/height to Cartesian coordinates
    // This is a simplified conversion - you might want to use a more accurate Earth model
    const double R = 6371.0; // Earth radius in km
    
    double latRad = glm::radians(lat);
    double lonRad = glm::radians(lon);
    
    double r = (R + height) / R; // Normalize to unit sphere
    
    float x = r * cos(latRad) * cos(lonRad);
    float y = r * sin(latRad);
    float z = r * cos(latRad) * sin(lonRad);
    
    return glm::vec3(x, y, z);
}

glm::vec3 GlobeViewer::ECIToECEF(const double pos[3], double mse) {
    // Convert ECI to ECEF coordinates
    // This is a simplified conversion - you'll need proper GMST calculation
    // For now, just pass through the position (assuming it's already in the right frame)
    return glm::vec3(pos[0] / 6371.0, pos[1] / 6371.0, pos[2] / 6371.0); // Normalize to Earth radii
}

void GlobeViewer::Shutdown() {
    if (m_framebuffer) {
        glDeleteFramebuffers(1, &m_framebuffer);
        glDeleteTextures(1, &m_colorTexture);
        glDeleteTextures(1, &m_depthTexture);
    }
    
    if (m_globeVAO) {
        glDeleteVertexArrays(1, &m_globeVAO);
        glDeleteBuffers(1, &m_globeVBO);
        glDeleteBuffers(1, &m_globeEBO);
    }
    
    if (m_pathVAO) {
        glDeleteVertexArrays(1, &m_pathVAO);
        glDeleteBuffers(1, &m_pathVBO);
    }
    
    if (m_shaderProgram) {
        glDeleteProgram(m_shaderProgram);
    }
    
    if (m_earthTexture) {
        glDeleteTextures(1, &m_earthTexture);
    }
}