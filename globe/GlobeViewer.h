#pragma once

#include <glad.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include "../PropResults.h" // Your existing header

class GlobeViewer {
public:
    GlobeViewer();
    ~GlobeViewer();

    bool Initialize(int width = 800, int height = 600);
    void Render();
    void Shutdown();

    // Set the propagation results to display
    void SetPropagationResults(const PropagationResults& results);

    // Control which satellites to show
    void SetVisibleSatellites(const std::vector<int>& satelliteIndices);

private:
    // OpenGL objects
    GLuint m_framebuffer;
    GLuint m_colorTexture;
    GLuint m_depthTexture;
    GLuint m_globeVAO, m_globeVBO, m_globeEBO;
    GLuint m_pathVAO, m_pathVBO;
    GLuint m_shaderProgram;
    GLuint m_earthTexture;

    // Viewport
    int m_width, m_height;

    // Camera/View controls
    glm::mat4 m_viewMatrix{};
    glm::mat4 m_projMatrix{};
    float m_cameraDistance;
    float m_cameraRotationX;
    float m_cameraRotationY;
    bool m_isDragging;
    ImVec2 m_lastMousePos;

    // Globe geometry
    std::vector<float> m_globeVertices;
    std::vector<unsigned int> m_globeIndices;

    // Satellite data
    PropagationResults m_propResults;
    std::vector<int> m_visibleSatellites;
    int m_currentTimeStep;
    bool m_animating;
    float m_animationSpeed;

    // UI state
    bool m_showPaths;
    bool m_showCurrentPositions;
    float m_pathOpacity;

    // Private methods
    bool CreateFramebuffer();
    bool LoadShaders();
    bool LoadEarthTexture();
    void CreateGlobeGeometry();
    void UpdateCamera();
    void HandleMouseInput();
    void RenderGlobe();
    void RenderSatellitePaths();
    void RenderSatellitePositions();
    void RenderUI();

    // Coordinate conversion
    glm::vec3 LLHToCartesian(double lat, double lon, double height);
    glm::vec3 ECIToECEF(const double pos[3], double mse);
};