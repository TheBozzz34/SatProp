cmake_minimum_required(VERSION 3.31)
project(SatProp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.15.3
)
FetchContent_MakeAvailable(spdlog)

# Fetch GLM
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.1  # or latest stable version
)

FetchContent_MakeAvailable(glm)


add_compile_definitions(GLFW_STATIC)


include_directories(
        "${CMAKE_SOURCE_DIR}/wrappers"
        "${CMAKE_SOURCE_DIR}/services"
        "${CMAKE_SOURCE_DIR}/extern/imgui"
        "${CMAKE_SOURCE_DIR}/extern/imgui/backends"
        "${CMAKE_SOURCE_DIR}/extern/glad"
        "${CMAKE_SOURCE_DIR}/extern/glfw"
)

link_directories(
        "E:/Sgp4Prop/Lib/Windows"
        "E:/CLionProjects/SatProp/lib"
)

file(GLOB WRAPPER_SOURCES "${CMAKE_SOURCE_DIR}/wrappers/*.c")
file(GLOB SERVICE_SOURCES "${CMAKE_SOURCE_DIR}/services/*.c")

set(IMGUI_SOURCES
        extern/imgui/imgui.cpp
        extern/imgui/imgui_draw.cpp
        extern/imgui/imgui_tables.cpp
        extern/imgui/imgui_widgets.cpp
        extern/imgui/imgui_demo.cpp
        extern/imgui/backends/imgui_impl_glfw.cpp
        extern/imgui/backends/imgui_impl_opengl3.cpp
)


set(GLAD_SOURCES glad.c)

add_executable(SatProp
        main.cpp
        ${WRAPPER_SOURCES}
        ${SERVICE_SOURCES}
        ${IMGUI_SOURCES}
        ${GLAD_SOURCES}
        Propagator.cpp
        Propagator.h
        PropResults.h
        SGP4DataViewer.h
        globe/GlobeViewer.h
        globe/GlobeViewer.cpp
)


target_link_libraries(SatProp
        spdlog::spdlog_header_only
        glfw3
        opengl32
        gdi32
        user32
        shell32
        kernel32
        DllMain
        EnvConst
        TimeFunc
        AstroFunc
        Tle
        Sgp4Prop
        glm::glm
)

# copy DLLs to output folder after building
foreach(dll IN ITEMS
        DllMain.dll
        EnvConst.dll
        TimeFunc.dll
        AstroFunc.dll
        Tle.dll
        Sgp4Prop.dll)
    add_custom_command(TARGET SatProp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "E:/Sgp4Prop/Lib/Windows/${dll}"
            $<TARGET_FILE_DIR:SatProp>)
endforeach()
