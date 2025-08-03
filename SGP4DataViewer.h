//
// Created by sansm on 8/2/2025.
//

#include "imgui.h"
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>

#include "PropResults.h"

class SGP4DataViewer
{
private:
    PropagationResults m_results;
    int m_selectedSatellite = 0;
    int m_selectedTimeStep = 0;
    bool m_showOnlyErrors = false;
    bool m_autoScroll = true;

    // Helper function to format double values
    std::string FormatDouble(double value, int precision = 6)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << value;
        return oss.str();
    }

    // Helper function to format time (MSE)
    std::string FormatTime(double mse)
    {
        // Convert MSE to a more readable format if needed
        return FormatDouble(mse, 8) + " (MSE)";
    }

    // Helper to get satellite name from TLE line 1
    std::string GetSatelliteName(const std::string& line1)
    {
        if (line1.length() > 24)
            return line1.substr(2, 22); // Extract name from TLE
        return "Unknown Satellite";
    }

public:
    void SetData(const PropagationResults& results)
    {
        m_results = results;
        m_selectedSatellite = 0;
        m_selectedTimeStep = 0;
    }

    void Render()
    {
        if (!ImGui::Begin("SGP4 Propagation Results", nullptr, ImGuiWindowFlags_MenuBar))
        {
            ImGui::End();
            return;
        }

        // Menu bar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("View"))
            {
                ImGui::Checkbox("Show Only Errors", &m_showOnlyErrors);
                ImGui::Checkbox("Auto Scroll", &m_autoScroll);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Overall status
        RenderOverallStatus();

        ImGui::Separator();

        // Main content area with splitter
        ImGui::BeginChild("MainContent");

        // Left panel - Satellite list
        ImGui::BeginChild("SatelliteList", ImVec2(300, 0), true);
        RenderSatelliteList();
        ImGui::EndChild();

        ImGui::SameLine();

        // Right panel - Selected satellite details
        ImGui::BeginChild("SatelliteDetails", ImVec2(0, 0), true);
        RenderSatelliteDetails();
        ImGui::EndChild();

        ImGui::EndChild();
        ImGui::End();
    }

private:
    void RenderOverallStatus()
    {
        // Status indicators
        if (m_results.overallSuccess)
        {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ SUCCESS");
        }
        else
        {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "✗ FAILED");
            if (!m_results.generalError.empty())
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0.5, 0, 1), "- %s", m_results.generalError.c_str());
            }
        }

        ImGui::SameLine();
        ImGui::Text("| Satellites: %d", m_results.totalSatellites);

        // Count successful satellites
        int successCount = 0;
        for (const auto& sat : m_results.satellites)
        {
            if (sat.propagationSuccess) successCount++;
        }

        ImGui::SameLine();
        ImGui::Text("| Successful: %d", successCount);

        if (successCount < m_results.totalSatellites)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0.5, 0, 1), "| Failed: %d",
                             m_results.totalSatellites - successCount);
        }
    }

    void RenderSatelliteList()
    {
        ImGui::Text("Satellites");
        ImGui::Separator();

        for (int i = 0; i < (int)m_results.satellites.size(); i++)
        {
            const auto& sat = m_results.satellites[i];

            // Skip if showing only errors and this satellite is successful
            if (m_showOnlyErrors && sat.propagationSuccess) continue;

            ImGui::PushID(i);

            // Status icon
            const char* icon = sat.propagationSuccess ? "✓" : "✗";
            ImVec4 color = sat.propagationSuccess ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1);

            bool isSelected = (i == m_selectedSatellite);

            if (ImGui::Selectable(("##sat" + std::to_string(i)).c_str(), isSelected,
                                ImGuiSelectableFlags_SpanAllColumns))
            {
                m_selectedSatellite = i;
                m_selectedTimeStep = 0;
            }

            ImGui::SameLine();
            ImGui::TextColored(color, "%s", icon);
            ImGui::SameLine();

            std::string satName = GetSatelliteName(sat.line1);
            ImGui::Text("%s", satName.c_str());

            // Show step count
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7, 0.7, 0.7, 1), "(%d steps)", (int)sat.timeSteps.size());

            ImGui::PopID();
        }
    }

    void RenderSatelliteDetails()
    {
        if (m_selectedSatellite >= (int)m_results.satellites.size())
        {
            ImGui::Text("No satellite selected");
            return;
        }

        const auto& sat = m_results.satellites[m_selectedSatellite];

        // Satellite header
        std::string satName = GetSatelliteName(sat.line1);
        ImGui::Text("Satellite: %s", satName.c_str());

        // Status
        ImGui::SameLine();
        if (sat.propagationSuccess)
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "[SUCCESS]");
        else
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "[FAILED]");

        ImGui::Separator();

        // TLE Data in collapsible section
        if (ImGui::CollapsingHeader("TLE Data"))
        {
            ImGui::Text("Line 1: %s", sat.line1.c_str());
            ImGui::Text("Line 2: %s", sat.line2.c_str());
        }

        // Time steps summary and navigation
        if (ImGui::CollapsingHeader("Propagation Steps", ImGuiTreeNodeFlags_DefaultOpen))
        {
            RenderTimeStepsNavigation(sat);
        }

        // Detailed view of selected time step
        if (!sat.timeSteps.empty() && m_selectedTimeStep < (int)sat.timeSteps.size())
        {
            if (ImGui::CollapsingHeader("Selected Step Details", ImGuiTreeNodeFlags_DefaultOpen))
            {
                RenderTimeStepDetails(sat.timeSteps[m_selectedTimeStep]);
            }
        }
    }

    void RenderTimeStepsNavigation(const SatelliteData& sat)
    {
        if (sat.timeSteps.empty())
        {
            ImGui::Text("No time steps available");
            return;
        }

        int totalSteps = (int)sat.timeSteps.size();

        // Summary information
        ImGui::Text("Total Steps: %d", totalSteps);

        // Count errors
        int errorCount = 0;
        int lastErrorIndex = -1;
        int firstErrorIndex = -1;
        for (int i = 0; i < totalSteps; i++)
        {
            if (sat.timeSteps[i].hasError)
            {
                errorCount++;
                if (firstErrorIndex == -1) firstErrorIndex = i;
                lastErrorIndex = i;
            }
        }

        if (errorCount > 0)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "| Errors: %d", errorCount);
        }
        else
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "| All steps successful");
        }

        ImGui::Separator();

        // Navigation controls
        ImGui::Text("Navigate Steps:");

        // Step selector slider
        ImGui::PushItemWidth(200);
        if (ImGui::SliderInt("##StepSlider", &m_selectedTimeStep, 0, totalSteps - 1))
        {
            // Clamp to valid range
            if (m_selectedTimeStep < 0) m_selectedTimeStep = 0;
            if (m_selectedTimeStep >= totalSteps) m_selectedTimeStep = totalSteps - 1;
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::Text("Step %d / %d", m_selectedTimeStep + 1, totalSteps);

        // Navigation buttons
        if (ImGui::Button("<<First"))
        {
            m_selectedTimeStep = 0;
        }

        ImGui::SameLine();
        if (ImGui::Button("<Prev"))
        {
            m_selectedTimeStep = std::max(0, m_selectedTimeStep - 1);
        }

        ImGui::SameLine();
        if (ImGui::Button("Next>"))
        {
            m_selectedTimeStep = std::min(totalSteps - 1, m_selectedTimeStep + 1);
        }

        ImGui::SameLine();
        if (ImGui::Button("Last>>"))
        {
            m_selectedTimeStep = totalSteps - 1;
        }

        // Error navigation buttons (if errors exist)
        if (errorCount > 0)
        {
            ImGui::Spacing();
            ImGui::Text("Jump to Errors:");

            if (ImGui::Button("First Error"))
            {
                m_selectedTimeStep = firstErrorIndex;
            }

            ImGui::SameLine();
            if (ImGui::Button("Last Error"))
            {
                m_selectedTimeStep = lastErrorIndex;
            }

            ImGui::SameLine();
            if (ImGui::Button("Next Error"))
            {
                // Find next error after current step
                for (int i = m_selectedTimeStep + 1; i < totalSteps; i++)
                {
                    if (sat.timeSteps[i].hasError)
                    {
                        m_selectedTimeStep = i;
                        break;
                    }
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Prev Error"))
            {
                // Find previous error before current step
                for (int i = m_selectedTimeStep - 1; i >= 0; i--)
                {
                    if (sat.timeSteps[i].hasError)
                    {
                        m_selectedTimeStep = i;
                        break;
                    }
                }
            }
        }

        // Quick jump controls
        ImGui::Spacing();
        ImGui::Text("Quick Jump:");

        if (ImGui::Button("25%"))
        {
            m_selectedTimeStep = totalSteps / 4;
        }

        ImGui::SameLine();
        if (ImGui::Button("50%"))
        {
            m_selectedTimeStep = totalSteps / 2;
        }

        ImGui::SameLine();
        if (ImGui::Button("75%"))
        {
            m_selectedTimeStep = (totalSteps * 3) / 4;
        }

        // Show current step info
        if (m_selectedTimeStep >= 0 && m_selectedTimeStep < totalSteps)
        {
            const auto& currentStep = sat.timeSteps[m_selectedTimeStep];

            ImGui::Separator();
            ImGui::Text("Current Step Info:");

            if (currentStep.hasError)
            {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Status: ERROR");
                ImGui::Text("Message: %s", currentStep.errorMsg.c_str());
            }
            else
            {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: OK");
                ImGui::Text("Time (MSE): %s", FormatDouble(currentStep.mse, 8).c_str());
                ImGui::Text("Height: %.3f km", currentStep.llh[2]);

                if (currentStep.llh[2] < 100.0)
                {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 0.5, 0, 1), "⚠ Low altitude");
                }
            }
        }
    }

    void RenderTimeStepDetails(const TimeStepData& step)
    {
        if (step.hasError)
        {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", step.errorMsg.c_str());
            return;
        }

        // Create tabs for different data categories
        if (ImGui::BeginTabBar("StepDetailsTab"))
        {
            if (ImGui::BeginTabItem("State Vectors"))
            {
                RenderStateVectors(step);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Orbital Elements"))
            {
                RenderOrbitalElements(step);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Geographic"))
            {
                RenderGeographicData(step);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    void RenderStateVectors(const TimeStepData& step)
    {
        ImGui::Text("Time (MSE): %s", FormatTime(step.mse).c_str());
        ImGui::Separator();

        // Position
        ImGui::Text("Position (km):");
        ImGui::Indent();
        ImGui::Text("X: %s", FormatDouble(step.pos[0], 3).c_str());
        ImGui::Text("Y: %s", FormatDouble(step.pos[1], 3).c_str());
        ImGui::Text("Z: %s", FormatDouble(step.pos[2], 3).c_str());

        double posMag = sqrt(step.pos[0]*step.pos[0] + step.pos[1]*step.pos[1] + step.pos[2]*step.pos[2]);
        ImGui::Text("Magnitude: %s", FormatDouble(posMag, 3).c_str());
        ImGui::Unindent();

        ImGui::Spacing();

        // Velocity
        ImGui::Text("Velocity (km/s):");
        ImGui::Indent();
        ImGui::Text("X: %s", FormatDouble(step.vel[0], 6).c_str());
        ImGui::Text("Y: %s", FormatDouble(step.vel[1], 6).c_str());
        ImGui::Text("Z: %s", FormatDouble(step.vel[2], 6).c_str());

        double velMag = sqrt(step.vel[0]*step.vel[0] + step.vel[1]*step.vel[1] + step.vel[2]*step.vel[2]);
        ImGui::Text("Magnitude: %s", FormatDouble(velMag, 6).c_str());
        ImGui::Unindent();
    }

    void RenderOrbitalElements(const TimeStepData& step)
    {
        // Mean Keplerian Elements
        ImGui::Text("Mean Keplerian Elements:");
        ImGui::Indent();
        ImGui::Text("Semi-major axis: %s km", FormatDouble(step.meanKep[0], 3).c_str());
        ImGui::Text("Eccentricity: %s", FormatDouble(step.meanKep[1], 8).c_str());
        ImGui::Text("Inclination: %s deg", FormatDouble(step.meanKep[2], 6).c_str());
        ImGui::Text("RAAN: %s deg", FormatDouble(step.meanKep[3], 6).c_str());
        ImGui::Text("Arg of Perigee: %s deg", FormatDouble(step.meanKep[4], 6).c_str());
        ImGui::Text("Mean Anomaly: %s deg", FormatDouble(step.meanKep[5], 6).c_str());
        ImGui::Unindent();

        ImGui::Spacing();

        // Osculating Keplerian Elements
        ImGui::Text("Osculating Keplerian Elements:");
        ImGui::Indent();
        ImGui::Text("Semi-major axis: %s km", FormatDouble(step.oscKep[0], 3).c_str());
        ImGui::Text("Eccentricity: %s", FormatDouble(step.oscKep[1], 8).c_str());
        ImGui::Text("Inclination: %s deg", FormatDouble(step.oscKep[2], 6).c_str());
        ImGui::Text("RAAN: %s deg", FormatDouble(step.oscKep[3], 6).c_str());
        ImGui::Text("Arg of Perigee: %s deg", FormatDouble(step.oscKep[4], 6).c_str());
        ImGui::Text("True Anomaly: %s deg", FormatDouble(step.oscKep[5], 6).c_str());
        ImGui::Unindent();

        ImGui::Spacing();

        // Additional orbital parameters
        ImGui::Text("Orbital Parameters:");
        ImGui::Indent();
        ImGui::Text("Mean Motion: %s rev/day", FormatDouble(step.meanMotion, 8).c_str());
        ImGui::Text("Nodal Period: %s min", FormatDouble(step.nodalApPer[0], 3).c_str());
        ImGui::Text("Apogee: %s km", FormatDouble(step.nodalApPer[1], 3).c_str());
        ImGui::Text("Perigee: %s km", FormatDouble(step.nodalApPer[2], 3).c_str());
        ImGui::Unindent();
    }

    void RenderGeographicData(const TimeStepData& step)
    {
        ImGui::Text("Geographic Position:");
        ImGui::Indent();
        ImGui::Text("Latitude: %s deg", FormatDouble(step.llh[0], 6).c_str());
        ImGui::Text("Longitude: %s deg", FormatDouble(step.llh[1], 6).c_str());
        ImGui::Text("Height: %s km", FormatDouble(step.llh[2], 3).c_str());

        if (step.llh[2] < 100.0)
        {
            ImGui::TextColored(ImVec4(1, 0.5, 0, 1), "⚠ Warning: Low altitude");
        }

        if (step.llh[2] < 0)
        {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "⚠ Warning: Below surface");
        }

        ImGui::Unindent();
    }
};
