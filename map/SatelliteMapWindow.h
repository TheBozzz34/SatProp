//
// SatelliteMapWindow.h
// Created for satellite propagation app
//

#ifndef SATELLITEMAPWINDOW_H
#define SATELLITEMAPWINDOW_H

#include "../PropResults.h"
#include <imgui.h>
#include <vector>
#include <string>

struct MapProjection {
    float width;
    float height;
    float centerLat;
    float centerLon;

    // Convert lat/lon to screen coordinates (Equirectangular projection)
    ImVec2 projectToScreen(double lat, double lon) const {
        // Normalize longitude to [-180, 180]
        while (lon > 180.0) lon -= 360.0;
        while (lon < -180.0) lon += 360.0;

        // Convert to screen coordinates
        float x = (float)((lon + 180.0) / 360.0 * width);
        float y = (float)((90.0 - lat) / 180.0 * height);

        return ImVec2(x, y);
    }
};

struct SatelliteTrack {
    std::vector<ImVec2> screenPoints;
    ImU32 color;
    std::string name;
    bool visible;
    int currentStep;  // For animation
};

class SatelliteMapWindow {
private:
    std::vector<SatelliteTrack> tracks;
    MapProjection projection;
    ImVec2 mapSize;
    bool showGrid;
    bool animateTracks;
    int animationSpeed;
    float zoomLevel;
    ImVec2 panOffset;

    // UI state
    bool isDragging;
    ImVec2 lastMousePos;

    // Drawing helpers
    void drawWorldMap(ImDrawList* drawList, const ImVec2& canvasPos);

    void drawContinentOutlines(ImDrawList *drawList, const ImVec2 &canvasPos);

    void drawGrid(ImDrawList* drawList, const ImVec2& canvasPos);
    void drawSatelliteTracks(ImDrawList* drawList, const ImVec2& canvasPos);
    void drawControls();

    // Coordinate conversion with zoom/pan
    ImVec2 worldToScreen(const ImVec2& worldPos, const ImVec2& canvasPos) const;
    ImVec2 screenToWorld(const ImVec2& screenPos, const ImVec2& canvasPos) const;

public:
    SatelliteMapWindow();

    ~SatelliteMapWindow() = default;

    static void loadEarthTexture(const std::string &filePath);

    void render();
    void updateSatelliteData(const PropagationResults& results);
    void clearTracks();
    void setTrackVisibility(int trackIndex, bool visible);
    void setTrackColor(int trackIndex, ImU32 color);

    // Animation controls
    void playAnimation();
    void pauseAnimation();
    void resetAnimation();
    void setAnimationSpeed(int speed);
};

#endif //SATELLITEMAPWINDOW_H