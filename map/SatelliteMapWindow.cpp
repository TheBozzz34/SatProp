//
// SatelliteMapWindow.cpp
// Implementation of satellite map visualization
//

#include "SatelliteMapWindow.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#define STB_IMAGE_IMPLEMENTATION
#include <iostream>

#include "glad.h"
#include "../stb_image.h"

GLuint earthTexture = 0;

SatelliteMapWindow::SatelliteMapWindow()
    : mapSize(800, 400)
    , showGrid(true)
    , animateTracks(false)
    , animationSpeed(1)
    , zoomLevel(1.0f)
    , panOffset(0, 0)
    , isDragging(false)
    , lastMousePos(0, 0)
{
    projection.width = mapSize.x;
    projection.height = mapSize.y;
    projection.centerLat = 0.0f;
    projection.centerLon = 0.0f;
    loadEarthTexture("assets/earth_texture.jpg"); // Load the Earth texture
}

void SatelliteMapWindow::loadEarthTexture(const std::string& filePath) {
    int width, height, channels;
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load earth texture.\n";
        return;
    }

    glGenTextures(1, &earthTexture);
    glBindTexture(GL_TEXTURE_2D, earthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
}

void SatelliteMapWindow::render() {
    if (!ImGui::Begin("Satellite World Map")) {
        ImGui::End();
        return;
    }

    drawControls();

    // Get the draw list and canvas position
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // Adjust map size if needed
    if (canvasSize.x > 100 && canvasSize.y > 100) {
        mapSize = ImVec2(std::min(canvasSize.x, 1200.0f),
                        std::min(canvasSize.y - 100, 600.0f)); // Leave space for controls
        projection.width = mapSize.x;
        projection.height = mapSize.y;
    }

    // Create a child window for the map to handle scrolling/zooming
    ImGui::BeginChild("MapCanvas", mapSize, true, ImGuiWindowFlags_NoScrollbar);

    // Handle mouse interaction
    ImVec2 mousePos = ImGui::GetMousePos();
    bool isHovered = ImGui::IsItemHovered();

    if (isHovered) {
        // Zoom with mouse wheel
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0) {
            float oldZoom = zoomLevel;
            zoomLevel = std::max(0.1f, std::min(10.0f, zoomLevel + wheel * 0.1f));

            // Adjust pan to zoom toward mouse cursor
            ImVec2 mapCenter = ImVec2(canvasPos.x + mapSize.x * 0.5f, canvasPos.y + mapSize.y * 0.5f);
            ImVec2 mouseOffset = ImVec2(mousePos.x - mapCenter.x, mousePos.y - mapCenter.y);
            float zoomDelta = zoomLevel / oldZoom - 1.0f;
            panOffset.x -= mouseOffset.x * zoomDelta;
            panOffset.y -= mouseOffset.y * zoomDelta;
        }

        // Pan with mouse drag
        if (ImGui::IsMouseClicked(0)) {
            isDragging = true;
            lastMousePos = mousePos;
        }
    }

    if (isDragging) {
        if (ImGui::IsMouseDown(0)) {
            ImVec2 delta = ImVec2(mousePos.x - lastMousePos.x, mousePos.y - lastMousePos.y);
            panOffset.x += delta.x;
            panOffset.y += delta.y;
            lastMousePos = mousePos;
        } else {
            isDragging = false;
        }
    }

    // Draw the map
    drawWorldMap(drawList, canvasPos);
    if (showGrid) {
        drawGrid(drawList, canvasPos);
    }
    drawSatelliteTracks(drawList, canvasPos);

    ImGui::EndChild();
    ImGui::End();
}

void SatelliteMapWindow::drawControls() {
    ImGui::Text("Map Controls");
    ImGui::Separator();

    ImGui::Checkbox("Show Grid", &showGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Animate Tracks", &animateTracks);

    if (ImGui::SliderFloat("Zoom", &zoomLevel, 0.1f, 10.0f, "%.1fx")) {
        // Clamp zoom level
        zoomLevel = std::max(0.1f, std::min(10.0f, zoomLevel));
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset View")) {
        zoomLevel = 1.0f;
        panOffset = ImVec2(0, 0);
    }

    if (animateTracks) {
        ImGui::SliderInt("Animation Speed", &animationSpeed, 1, 10);
    }

    ImGui::Text("Satellites: %zu", tracks.size());
    ImGui::Separator();
}

void SatelliteMapWindow::drawWorldMap(ImDrawList* drawList, const ImVec2& canvasPos) {
    if (earthTexture == 0) return;

    // Define corners of the full world in map coordinates
    ImVec2 worldTopLeft = ImVec2(0.0f, 0.0f);
    ImVec2 worldBottomRight = ImVec2(projection.width, projection.height);

    // Convert those to screen space using same transform as everything else
    ImVec2 screenTopLeft = worldToScreen(worldTopLeft, canvasPos);
    ImVec2 screenBottomRight = worldToScreen(worldBottomRight, canvasPos);

    ImVec2 uv0(0.0f, 0.0f);
    ImVec2 uv1(1.0f, 1.0f);

    // Draw the Earth texture transformed by pan/zoom
    drawList->AddImage(
        (void*)(intptr_t)earthTexture,
        screenTopLeft,
        screenBottomRight,
        uv0,
        uv1
    );
}




void SatelliteMapWindow::drawContinentOutlines(ImDrawList* drawList, const ImVec2& canvasPos) {
    ImU32 coastColor = IM_COL32(200, 200, 200, 255);

    // Draw some basic continent shapes (simplified)
    // North America outline
    std::vector<ImVec2> northAmerica = {
        projection.projectToScreen(70, -150), projection.projectToScreen(70, -60),
        projection.projectToScreen(25, -80), projection.projectToScreen(25, -120)
    };

    // South America outline
    std::vector<ImVec2> southAmerica = {
        projection.projectToScreen(10, -80), projection.projectToScreen(10, -35),
        projection.projectToScreen(-55, -70), projection.projectToScreen(-20, -80)
    };

    // Europe/Africa outline
    std::vector<ImVec2> europeAfrica = {
        projection.projectToScreen(70, -10), projection.projectToScreen(70, 40),
        projection.projectToScreen(-35, 20), projection.projectToScreen(-10, -10)
    };

    // Asia outline
    std::vector<ImVec2> asia = {
        projection.projectToScreen(70, 40), projection.projectToScreen(70, 180),
        projection.projectToScreen(10, 140), projection.projectToScreen(30, 60)
    };

    // Draw continent outlines
    auto drawContinent = [&](const std::vector<ImVec2>& points) {
        if (points.size() < 3) return;
        std::vector<ImVec2> screenPoints;
        for (const auto& point : points) {
            screenPoints.push_back(worldToScreen(point, canvasPos));
        }
        drawList->AddPolyline(screenPoints.data(), screenPoints.size(), coastColor, true, 1.0f);
    };

    drawContinent(northAmerica);
    drawContinent(southAmerica);
    drawContinent(europeAfrica);
    drawContinent(asia);
}

void SatelliteMapWindow::drawGrid(ImDrawList* drawList, const ImVec2& canvasPos) {
    ImU32 gridColor = IM_COL32(100, 100, 100, 128);

    // Draw latitude lines (every 30 degrees)
    for (int lat = -90; lat <= 90; lat += 30) {
        ImVec2 start = worldToScreen(projection.projectToScreen(lat, -180), canvasPos);
        ImVec2 end = worldToScreen(projection.projectToScreen(lat, 180), canvasPos);
        drawList->AddLine(start, end, gridColor, 1.0f);
    }

    // Draw longitude lines (every 30 degrees)
    for (int lon = -180; lon <= 180; lon += 30) {
        ImVec2 start = worldToScreen(projection.projectToScreen(-90, lon), canvasPos);
        ImVec2 end = worldToScreen(projection.projectToScreen(90, lon), canvasPos);
        drawList->AddLine(start, end, gridColor, 1.0f);
    }

    // Draw equator and prime meridian more prominently
    ImU32 primaryGridColor = IM_COL32(150, 150, 150, 200);

    // Equator
    ImVec2 equatorStart = worldToScreen(projection.projectToScreen(0, -180), canvasPos);
    ImVec2 equatorEnd = worldToScreen(projection.projectToScreen(0, 180), canvasPos);
    drawList->AddLine(equatorStart, equatorEnd, primaryGridColor, 2.0f);

    // Prime meridian
    ImVec2 primeStart = worldToScreen(projection.projectToScreen(-90, 0), canvasPos);
    ImVec2 primeEnd = worldToScreen(projection.projectToScreen(90, 0), canvasPos);
    drawList->AddLine(primeStart, primeEnd, primaryGridColor, 2.0f);
}

void SatelliteMapWindow::drawSatelliteTracks(ImDrawList* drawList, const ImVec2& canvasPos) {
    for (auto& track : tracks) {
        if (!track.visible || track.screenPoints.size() < 2) continue;

        std::vector<ImVec2> currentSegment;

        for (const auto& point : track.screenPoints) {
            if (point.x < -9999.0f) {
                if (currentSegment.size() >= 2) {
                    drawList->AddPolyline(currentSegment.data(), currentSegment.size(), track.color, false, 2.0f);
                }
                currentSegment.clear();
            } else {
                currentSegment.push_back(worldToScreen(point, canvasPos));
            }
        }

        if (currentSegment.size() >= 2) {
            drawList->AddPolyline(currentSegment.data(), currentSegment.size(), track.color, false, 2.0f);
        }
    }
}



ImVec2 SatelliteMapWindow::worldToScreen(const ImVec2& worldPos, const ImVec2& canvasPos) const {
    ImVec2 centered = ImVec2(worldPos.x - projection.width * 0.5f,
                            worldPos.y - projection.height * 0.5f);
    ImVec2 zoomed = ImVec2(centered.x * zoomLevel, centered.y * zoomLevel);
    ImVec2 panned = ImVec2(zoomed.x + panOffset.x, zoomed.y + panOffset.y);

    return ImVec2(canvasPos.x + mapSize.x * 0.5f + panned.x,
                  canvasPos.y + mapSize.y * 0.5f + panned.y);
}

ImVec2 SatelliteMapWindow::screenToWorld(const ImVec2& screenPos, const ImVec2& canvasPos) const {
    ImVec2 relative = ImVec2(screenPos.x - canvasPos.x - mapSize.x * 0.5f,
                            screenPos.y - canvasPos.y - mapSize.y * 0.5f);
    ImVec2 unpanned = ImVec2(relative.x - panOffset.x, relative.y - panOffset.y);
    ImVec2 unzoomed = ImVec2(unpanned.x / zoomLevel, unpanned.y / zoomLevel);

    return ImVec2(unzoomed.x + projection.width * 0.5f,
                  unzoomed.y + projection.height * 0.5f);
}

void SatelliteMapWindow::updateSatelliteData(const PropagationResults& results) {
    tracks.clear();

    const auto& satellite = results.satellites.front();  // Only one satellite

    if (!satellite.propagationSuccess || satellite.timeSteps.empty())
        return;

    SatelliteTrack track;
    track.color = IM_COL32(255, 100, 100, 255);  // Red
    track.name = "Current Orbit";
    track.visible = true;
    track.currentStep = 0;

    bool hasLast = false;
    float lastLon = 0.0f;

    // Estimate one orbit's worth of points — adjust based on timestep spacing
    constexpr size_t maxOrbitSteps = 90; // e.g., 90 time steps ≈ 1 orbit

    size_t totalSteps = satellite.timeSteps.size();
    size_t startIdx = totalSteps > maxOrbitSteps ? totalSteps - maxOrbitSteps : 0;

    for (size_t i = startIdx; i < totalSteps; ++i) {
        const auto& step = satellite.timeSteps[i];
        if (step.hasError) continue;

        float lat = static_cast<float>(step.llh[0]);
        float lon = static_cast<float>(step.llh[1]);

        // Normalize longitude to [-180, 180]
        while (lon < -180.0f) lon += 360.0f;
        while (lon > 180.0f) lon -= 360.0f;

        if (hasLast) {
            float lonDiff = std::fabs(lon - lastLon);
            if (lonDiff > 180.0f) {
                // Insert break if path wraps
                track.screenPoints.push_back(ImVec2(-10000, -10000));
            }
        }

        lastLon = lon;
        hasLast = true;

        ImVec2 screen = projection.projectToScreen(lat, lon);
        track.screenPoints.push_back(screen);
    }

    tracks.push_back(track);  // ✅ Only the latest orbit
}





void SatelliteMapWindow::clearTracks() {
    tracks.clear();
}

void SatelliteMapWindow::setTrackVisibility(int trackIndex, bool visible) {
    if (trackIndex >= 0 && trackIndex < tracks.size()) {
        tracks[trackIndex].visible = visible;
    }
}

void SatelliteMapWindow::setTrackColor(int trackIndex, ImU32 color) {
    if (trackIndex >= 0 && trackIndex < tracks.size()) {
        tracks[trackIndex].color = color;
    }
}

void SatelliteMapWindow::playAnimation() {
    animateTracks = true;
    // Animation logic would be called from your main loop
}

void SatelliteMapWindow::pauseAnimation() {
    animateTracks = false;
}

void SatelliteMapWindow::resetAnimation() {
    for (auto& track : tracks) {
        track.currentStep = 0;
    }
}

void SatelliteMapWindow::setAnimationSpeed(int speed) {
    animationSpeed = std::max(1, std::min(10, speed));
}

