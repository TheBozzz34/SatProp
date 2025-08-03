#include <stdio.h>
#include <math.h>
#include <string>
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "extern/imgui/imgui.h"
#include "extern/imgui/backends/imgui_impl_glfw.h"
#include "extern/imgui/backends/imgui_impl_opengl3.h"
#define GLFW_STATIC
#define GLFW_INCLUDE_NONE
#include "SGP4DataViewer.h"
#include "extern/GLFW/glfw3.h"
#include "extern/glad/glad.h"
#include "globe/GlobeViewer.h"

// C interface wrapper
#ifdef __cplusplus
extern "C"
{
#endif

#include "services/DllMainDll_Service.h"
#include "services/TimeFuncDll_Service.h"
#include "wrappers/DllMainDll.h"
#include "wrappers/EnvConstDll.h"
#include "wrappers/AstroFuncDll.h"
#include "wrappers/TimeFuncDll.h"
#include "wrappers/TleDll.h"
#include "wrappers/Sgp4PropDll.h"

#include "PropResults.h"

#ifdef __cplusplus
}
#endif

#include "Propagator.h"

// application state
struct AppState {
    char inputFile[512] = "";
    char outputFile[512] = "";
    bool showDemo = true;
    bool showAbout = false;

    // Propagation parameters
    double startTime = 0.0;
    double stopTime = 1440.0; // 24 hours in minutes
    double stepSize = 60.0;    // 1 hour in minutes
    bool useEpochRelative = true;

    std::string statusMessage = "Ready";
    bool isProcessing = false;
    int numSatellites = 0;

    // satellites loaded from tle file
    std::vector<std::string> loadedSatellites;

    SGP4DataViewer viewer;

    GlobeViewer m_globeViewer;
};

// why is sgp4prop so awful
void LoadAstroStdDlls();
void FreeAstroStdDlls();
bool InitializeOpenGL(GLFWwindow** window);
void InitializeImGui(GLFWwindow* window);
void RenderUI(AppState& state);
void ProcessSatellites(AppState& state);
void LoadTLEFile(AppState& state);
void ShowMainMenuBar(AppState& state);
void ShowFileDialog(AppState& state);
void ShowPropagationControls(AppState& state);
void ShowSatelliteList(AppState& state);
void ShowStatusBar(AppState& state);



int  order = 2;   // don't really know what this is or if needed
int  jobNum, errCode, isEndAll; // stuff for batch processing, will use later


// idk what these are either
constexpr int FT_OSC_STATE = 0;
constexpr int FT_OSC_ELEM = 1;
constexpr int FT_MEAN_ELEM = 2;
constexpr int FT_LLH_ELEM = 3;
constexpr int FT_NODAL_AP_PER = 4;

auto logger = spdlog::stdout_color_mt("console");
auto err_logger = spdlog::stderr_color_mt("stderr");


static void glfw_error_callback(int error, const char* description)
{
    err_logger->error("GLFW Error {}: {}", error, description);
}

int main(int argc, char* argv[])
{
    logger->info("SGP4 Satellite Propagation Program Starting...");

    // opengl init stuff
    GLFWwindow* window;
    if (!InitializeOpenGL(&window)) {
        return -1;
    }

    // imgui
    InitializeImGui(window);

    // load AstroStd DLLs
    LoadAstroStdDlls();


    // prints SGP4 DLL info
    char sgp4DllInfo[INFOSTRLEN];
    Sgp4GetInfo(sgp4DllInfo);
    sgp4DllInfo[INFOSTRLEN-1] = 0;
    logger->info("{}", sgp4DllInfo);

    // setup app state
    AppState appState;
    appState.statusMessage = std::string("Loaded: ") + sgp4DllInfo;

    if (!appState.m_globeViewer.Initialize(800, 600)) {
        err_logger->error("Failed to initialize GlobeViewer");
        return false;
    }

    // main window loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents(); // poll events

        // imgui start frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // render the whole ui
        RenderUI(appState);

        // rending the actual frame and swap buffers
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // shutdown imgui and opengl
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    // Free AstroStd DLLs
    FreeAstroStdDlls();

    return 0;
}

bool InitializeOpenGL(GLFWwindow** window)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return false;

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window with graphics context
    *window = glfwCreateWindow(1280, 720, "SGP4 Satellite Propagation", NULL, NULL);
    if (*window == NULL)
        return false;

    glfwMakeContextCurrent(*window);
    glfwSwapInterval(1); // Enable vsync

    // glad init
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        err_logger->error("Failed to initialize OpenGL context");
        return false;
    }

    return true;
}

void InitializeImGui(GLFWwindow* window)
{
    // Setup imgui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // dark mode yuhhhh
    ImGui::StyleColorsDark();

    // setup backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
}

void RenderUI(AppState& state)
{
    ShowMainMenuBar(state);

    // main docking space
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus; // make it look like a docking space

    ImGui::Begin("MainWindow", nullptr, window_flags);

    // Left panel for file ops and propagation controls
    ImGui::BeginChild("LeftPanel", ImVec2(600, -25), true);
    ShowFileDialog(state);
    ImGui::Separator();
    ShowPropagationControls(state);
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel for satellite list and results
    ImGui::BeginChild("RightPanel", ImVec2(0, -25), true);
    ShowSatelliteList(state);
    ImGui::EndChild();

    ShowStatusBar(state);

    ImGui::End();

    // demo
    //if (state.showDemo)
        //ImGui::ShowDemoWindow(&state.showDemo);

    if (state.statusMessage == "Processing complete. Results saved.") {
        state.viewer.Render();
        state.m_globeViewer.Render();

    }

    // about dialog
    if (state.showAbout) {
        ImGui::Begin("About", &state.showAbout);
        ImGui::Text("SGP4 Satellite Propagation Program");
        ImGui::Text("Built with ImGui, GLFW, and AstroStd libraries");
        ImGui::Separator();
        ImGui::Text("This program propagates satellite orbits using the SGP4 algorithm.");
        ImGui::End();
    }
}

void ShowMainMenuBar(AppState& state)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Load TLE File", "Ctrl+O")) {
                // need to implement file dialog
            }
            if (ImGui::MenuItem("Save Results", "Ctrl+S")) {
                // also need to implement save
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(glfwGetCurrentContext(), true);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Show Demo", nullptr, &state.showDemo);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About")) {
                state.showAbout = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void ShowFileDialog(AppState& state)
{
    ImGui::Text("File Operations");

    ImGui::InputText("Input TLE File", state.inputFile, sizeof(state.inputFile));
    ImGui::SameLine();
    if (ImGui::Button("Browse##Input")) {
        // TODO: make this an actual file dialog
        strcpy(state.inputFile, "input.tle");
    }

    ImGui::InputText("Output Base Name", state.outputFile, sizeof(state.outputFile));
    ImGui::SameLine();
    if (ImGui::Button("Browse##Output")) {
        strcpy(state.outputFile, "output");
    }

    if (ImGui::Button("Load TLE File")) {
        LoadTLEFile(state);
    }

    ImGui::SameLine();
    if (ImGui::Button("Process Satellites") && !state.isProcessing) {
        ProcessSatellites(state);
    }

    if (state.isProcessing) {
        ImGui::SameLine();
        ImGui::Text("Processing...");
    }
}

void ShowPropagationControls(AppState& state)
{
    ImGui::Text("Propagation Parameters");

    ImGui::Checkbox("Times relative to epoch", &state.useEpochRelative);

    if (state.useEpochRelative) {
        ImGui::InputDouble("Start Time (min from epoch)", &state.startTime, 1.0, 10.0, "%.1f");
        ImGui::InputDouble("Stop Time (min from epoch)", &state.stopTime, 1.0, 10.0, "%.1f");
    } else {
        // date/time picker maybe?
        ImGui::InputDouble("Start Time (days since 1950)", &state.startTime, 1.0, 10.0, "%.6f");
        ImGui::InputDouble("Stop Time (days since 1950)", &state.stopTime, 1.0, 10.0, "%.6f");
    }

    ImGui::InputDouble("Step Size (minutes)", &state.stepSize, 1.0, 10.0, "%.1f");

    // quick presets for common prop times
    ImGui::Text("Quick Presets:");
    if (ImGui::Button("1 Hour")) {
        state.startTime = 0.0;
        state.stopTime = 60.0;
        state.stepSize = 5.0;
    }
    ImGui::SameLine();
    if (ImGui::Button("1 Day")) {
        state.startTime = 0.0;
        state.stopTime = 1440.0;
        state.stepSize = 60.0;
    }
    ImGui::SameLine();
    if (ImGui::Button("1 Week")) {
        state.startTime = 0.0;
        state.stopTime = 10080.0;
        state.stepSize = 360.0;
    }
}

void ShowSatelliteList(AppState& state)
{
    // show loaded satellites
    ImGui::Text("Loaded Satellites (%d)", state.numSatellites);
    ImGui::Separator();

    if (state.loadedSatellites.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No satellites loaded.");
        ImGui::Text("Load a TLE file to see satellites here.");
    } else {
        for (const auto & loadedSatellite : state.loadedSatellites) {
            ImGui::Selectable(loadedSatellite.c_str());
        }
    }
}

void ShowStatusBar(AppState& state)
{
    ImGui::Separator();
    ImGui::Text("Status: %s", state.statusMessage.c_str());
}

void LoadTLEFile(AppState& state)
{
    if (strlen(state.inputFile) == 0) {
        state.statusMessage = "Error: No input file specified";
        return;
    }

    // reset state
    TleRemoveAllSats();
    state.loadedSatellites.clear();
    state.numSatellites = 0;

    // load tle
    int result = Sgp4LoadFileAll(state.inputFile);
    if (result != 0) {
        state.statusMessage = "Error: Could not load TLE file";
        return;
    }

    // sat count
    state.numSatellites = TleGetCount();
    if (state.numSatellites == 0) {
        state.statusMessage = "Warning: No satellites found in file";
        return;
    }

    // get sat keys
    __int64* satKeys = static_cast<long long *>(malloc(state.numSatellites * sizeof(__int64)));
    TleGetLoaded(2, satKeys); // Order 2 = in order read

    char line1[INPUTCARDLEN], line2[INPUTCARDLEN];
    for (int i = 0; i < state.numSatellites; i++) {
        TleGetLines(satKeys[i], line1, line2);


        std::string satName(line1, 2, 20); // Get the name from line1 char 2 to 22

        // remove trailing spaces
        if (const size_t end = satName.find_last_not_of(' '); end != std::string::npos) {
            satName = satName.substr(0, end + 1);
        }

        state.loadedSatellites.push_back(satName);
    }

    free(satKeys);

    char statusMsg[256];
    sprintf(statusMsg, "Loaded %d satellites from %s", state.numSatellites, state.inputFile);
    logger->info(statusMsg);

    state.statusMessage = statusMsg;
}

// file opener b/c i don't have a file dialog yet
FILE* OpenFile(const char* filename, const char* mode)
{
    FILE* file = fopen(filename, mode);
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
    }
    return file;
}


void ProcessSatellites(AppState& state)
{
    if (state.numSatellites == 0) {
        state.statusMessage = "Error: No satellites loaded";
        return;
    }

    if (strlen(state.outputFile) == 0) { // can remove atm b/c not outputting to file
        state.statusMessage = "Error: No output file specified";
        return;
    }

    state.isProcessing = true;
    state.statusMessage = "Processing satellites...";


    try {
        PropagationResults results = SGP_IMPL::Propagator().RunOneSgp4Job(state.inputFile, state.startTime, state.stopTime, state.stepSize);
        static bool dataSet = false;

        if (!dataSet)
        {
            state.viewer.SetData(results);
            state.m_globeViewer.SetPropagationResults(results);
            dataSet = true;
        }

        logger->info("Processed {} satellites", results.totalSatellites);
        if (results.overallSuccess) {
            for (const auto& sat : results.satellites) {
                logger->info("Satellite: {}", sat.line1);
                for (const auto& step : sat.timeSteps) {
                    if (step.hasError) {
                        err_logger->error("Error in step: {}", step.errorMsg);
                    }
                }
            }
            state.statusMessage = "Processing complete. Results saved.";
            state.isProcessing = false;
        }
    } catch (const std::exception& e) {
        state.statusMessage = std::string("Error: ") + e.what();
        state.isProcessing = false;
    }
}



// load dlls because AstroSTD is a mess and uses GetFnPtr wrappers
void LoadAstroStdDlls()
{
    LoadDllMainDll();
    LoadEnvConstDll();
    LoadTimeFuncDll();
    LoadAstroFuncDll();
    LoadTleDll();
    LoadSgp4PropDll();
}

// free
void FreeAstroStdDlls()
{
    FreeDllMainDll();
    FreeEnvConstDll();
    FreeAstroFuncDll();
    FreeTimeFuncDll();
    FreeTleDll();
    FreeSgp4PropDll();
}

