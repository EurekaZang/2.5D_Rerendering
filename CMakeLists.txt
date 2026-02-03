cmake_minimum_required(VERSION 3.20)
project(RGBD_Rerendering VERSION 1.0.0 LANGUAGES C CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui)
find_package(OpenGL REQUIRED)

# Try to find OpenEXR
set(OPENEXR_FOUND FALSE)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(OPENEXR QUIET OpenEXR)
    if(OPENEXR_FOUND)
        message(STATUS "Found OpenEXR via pkg-config")
    endif()
endif()

# EGL
find_library(EGL_LIBRARY EGL HINTS /usr/lib/x86_64-linux-gnu /usr/lib)
find_path(EGL_INCLUDE_DIR EGL/egl.h HINTS /usr/include)

if(NOT EGL_LIBRARY OR NOT EGL_INCLUDE_DIR)
    message(FATAL_ERROR "EGL not found. Please install libegl1-mesa-dev")
endif()

# GLAD (OpenGL loader) - we'll generate our own
set(GLAD_DIR ${CMAKE_SOURCE_DIR}/external/glad)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    ${EGL_INCLUDE_DIR}
    ${GLAD_DIR}/include
)

# GLAD source
set(GLAD_SOURCES ${GLAD_DIR}/src/glad.c)

# Source files
set(IO_SOURCES
    src/io/image_io.cpp
    src/io/depth_io.cpp
)

set(MESH_SOURCES
    src/mesh/mesh_generator.cpp
    src/mesh/depth_mesh.cpp
)

set(RENDER_SOURCES
    src/render/egl_context.cpp
    src/render/gl_renderer.cpp
    src/render/shader.cpp
    src/render/framebuffer.cpp
)

set(APP_SOURCES
    src/app/config.cpp
)

# Create libraries
add_library(rgbd_io STATIC ${IO_SOURCES})
target_link_libraries(rgbd_io PUBLIC ${OpenCV_LIBS})
if(OPENEXR_FOUND)
    target_include_directories(rgbd_io PUBLIC ${OPENEXR_INCLUDE_DIRS})
    target_link_libraries(rgbd_io PUBLIC ${OPENEXR_LIBRARIES})
    target_compile_definitions(rgbd_io PUBLIC HAS_OPENEXR)
endif()

add_library(rgbd_mesh STATIC ${MESH_SOURCES})
target_link_libraries(rgbd_mesh PUBLIC rgbd_io)

add_library(rgbd_render STATIC ${RENDER_SOURCES} ${GLAD_SOURCES})
target_link_libraries(rgbd_render PUBLIC 
    ${OPENGL_LIBRARIES}
    ${EGL_LIBRARY}
    ${CMAKE_DL_LIBS}
)

add_library(rgbd_app STATIC ${APP_SOURCES})
target_link_libraries(rgbd_app PUBLIC rgbd_io rgbd_mesh rgbd_render)

# Main executable
add_executable(rgbd_rerender src/main.cpp)
target_link_libraries(rgbd_rerender PRIVATE rgbd_app)

# Test executable
add_executable(test_rerender test/test_rerender.cpp)
target_link_libraries(test_rerender PRIVATE rgbd_app)

# Generate sample data tool
add_executable(generate_sample test/generate_sample.cpp)
target_link_libraries(generate_sample PRIVATE rgbd_io)

# Install targets
install(TARGETS rgbd_rerender DESTINATION bin)
install(DIRECTORY shaders/ DESTINATION share/rgbd_rerender/shaders)

# Copy shaders to build directory
file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})

# Print configuration summary
message(STATUS "")
message(STATUS "=== RGBD Rerendering Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenGL found: ${OPENGL_FOUND}")
message(STATUS "EGL library: ${EGL_LIBRARY}")
if(OpenEXR_FOUND OR OPENEXR_FOUND)
    message(STATUS "OpenEXR: Found")
else()
    message(STATUS "OpenEXR: Not found (EXR output disabled)")
endif()
message(STATUS "")
