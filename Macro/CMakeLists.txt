cmake_minimum_required(VERSION 3.16)
project(MacroApp VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC: force C++17 mode explicitly
if(MSVC)
    add_compile_options(/std:c++17 /permissive-)
endif()

# Windows-specific settings
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
endif()

# ── Paths (all files are flat in the same folder as CMakeLists.txt) ──
set(SRC_DIR     ${CMAKE_SOURCE_DIR})
set(IMGUI_DIR   ${CMAKE_SOURCE_DIR}/ImGui)

# ImGui sources
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_impl_win32.cpp
    ${IMGUI_DIR}/imgui_impl_dx11.cpp
)

# Disable precompiled headers for ImGui files — they may reference pch.h
# from a VS project template which we don't use in this CMake build.
foreach(IMGUI_SRC ${IMGUI_SOURCES})
    set_source_files_properties(${IMGUI_SRC} PROPERTIES COMPILE_FLAGS "/Y-")
endforeach()

# Main app sources (flat alongside CMakeLists.txt)
set(APP_SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/hook_manager.cpp
    ${SRC_DIR}/input_sim.cpp
    ${SRC_DIR}/macro_manager.cpp
    ${SRC_DIR}/persistence.cpp
    ${SRC_DIR}/gui.cpp
    ${SRC_DIR}/recorder.cpp
    ${SRC_DIR}/sound_player.cpp
    ${CMAKE_SOURCE_DIR}/app.rc
)

add_executable(MacroApp WIN32 ${APP_SOURCES} ${IMGUI_SOURCES})

target_include_directories(MacroApp PRIVATE
    ${SRC_DIR}      # picks up all .h files flat in Macro/
    ${IMGUI_DIR}    # picks up imgui.h, imgui_impl_*.h from Macro/ImGui/
)

target_link_libraries(MacroApp PRIVATE
    d3d11
    dxgi
    dwmapi
    user32
    gdi32
    imm32
    ole32
    shell32
    winmm
    windowscodecs
)

# Copy the toggle-chime sound files next to the built executable.
add_custom_command(TARGET MacroApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/enabled.mp3
        ${CMAKE_SOURCE_DIR}/disabled.mp3
        $<TARGET_FILE_DIR:MacroApp>
)