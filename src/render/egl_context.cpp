#include "egl_context.hpp"

// EGL headers
#define EGL_EGL_PROTOTYPES 1
#include <EGL/egl.h>

// EGL extension function pointer types (manually defined to avoid eglext.h issues)
typedef EGLBoolean (*PFNEGLQUERYDEVICESEXTPROC_LOCAL)(EGLint, void*, EGLint*);
typedef EGLDisplay (*PFNEGLGETPLATFORMDISPLAYEXTPROC_LOCAL)(EGLenum, void*, const EGLint*);

// EGL constants that may not be defined
#ifndef EGL_PLATFORM_DEVICE_EXT
#define EGL_PLATFORM_DEVICE_EXT 0x313F
#endif

#ifndef EGL_CONTEXT_MAJOR_VERSION
#define EGL_CONTEXT_MAJOR_VERSION 0x3098
#endif

#ifndef EGL_CONTEXT_MINOR_VERSION
#define EGL_CONTEXT_MINOR_VERSION 0x30FB
#endif

#ifndef EGL_CONTEXT_OPENGL_PROFILE_MASK
#define EGL_CONTEXT_OPENGL_PROFILE_MASK 0x30FD
#endif

#ifndef EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT 0x00000001
#endif

#include <glad/glad.h>

#include <iostream>
#include <cstring>

namespace rgbd {
namespace render {

GLContext::GLContext() {}

GLContext::~GLContext() {
    destroy();
}

GLContext::GLContext(GLContext&& other) noexcept 
    : display_(other.display_)
    , context_(other.context_)
    , surface_(other.surface_)
    , initialized_(other.initialized_) {
    other.display_ = nullptr;
    other.context_ = nullptr;
    other.surface_ = nullptr;
    other.initialized_ = false;
}

GLContext& GLContext::operator=(GLContext&& other) noexcept {
    if (this != &other) {
        destroy();
        display_ = other.display_;
        context_ = other.context_;
        surface_ = other.surface_;
        initialized_ = other.initialized_;
        other.display_ = nullptr;
        other.context_ = nullptr;
        other.surface_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

bool GLContext::initialize(int deviceIndex) {
    if (initialized_) {
        std::cerr << "EGL context already initialized" << std::endl;
        return true;
    }

    // Get EGL display using platform device extension for headless rendering
    EGLDisplay display = EGL_NO_DISPLAY;
    
    // Try to get display using platform device extension
    auto eglQueryDevicesEXT = (PFNEGLQUERYDEVICESEXTPROC_LOCAL)
        eglGetProcAddress("eglQueryDevicesEXT");
    auto eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC_LOCAL)
        eglGetProcAddress("eglGetPlatformDisplayEXT");
    
    if (eglQueryDevicesEXT && eglGetPlatformDisplayEXT) {
        const int maxDevices = 16;
        void* devices[maxDevices];
        EGLint numDevices = 0;
        
        if (eglQueryDevicesEXT(maxDevices, devices, &numDevices) && numDevices > 0) {
            int targetDevice = (deviceIndex >= 0 && deviceIndex < numDevices) 
                               ? deviceIndex : 0;
            display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, 
                                               devices[targetDevice], nullptr);
            
            if (display != EGL_NO_DISPLAY) {
                std::cout << "Using EGL device " << targetDevice 
                          << " of " << numDevices << " available" << std::endl;
            }
        }
    }
    
    // Fallback to default display
    if (display == EGL_NO_DISPLAY) {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    }
    
    if (display == EGL_NO_DISPLAY) {
        std::cerr << "Error: Failed to get EGL display" << std::endl;
        return false;
    }
    
    display_ = display;
    
    // Initialize EGL
    EGLint major, minor;
    if (!eglInitialize(display, &major, &minor)) {
        std::cerr << "Error: Failed to initialize EGL" << std::endl;
        display_ = nullptr;
        return false;
    }
    
    std::cout << "EGL version: " << major << "." << minor << std::endl;
    
    // Choose config
    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };
    
    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) || numConfigs == 0) {
        std::cerr << "Error: Failed to choose EGL config" << std::endl;
        eglTerminate(display);
        display_ = nullptr;
        return false;
    }
    
    // Bind OpenGL API
    if (!eglBindAPI(EGL_OPENGL_API)) {
        std::cerr << "Error: Failed to bind OpenGL API" << std::endl;
        eglTerminate(display);
        display_ = nullptr;
        return false;
    }
    
    // Create context with OpenGL 4.6 Core Profile
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 4,
        EGL_CONTEXT_MINOR_VERSION, 6,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };
    
    EGLContext eglContext = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    
    // Fallback to OpenGL 4.5
    if (eglContext == EGL_NO_CONTEXT) {
        const EGLint contextAttribs45[] = {
            EGL_CONTEXT_MAJOR_VERSION, 4,
            EGL_CONTEXT_MINOR_VERSION, 5,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
            EGL_NONE
        };
        eglContext = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs45);
    }
    
    // Fallback to OpenGL 4.3
    if (eglContext == EGL_NO_CONTEXT) {
        const EGLint contextAttribs43[] = {
            EGL_CONTEXT_MAJOR_VERSION, 4,
            EGL_CONTEXT_MINOR_VERSION, 3,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
            EGL_NONE
        };
        eglContext = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs43);
    }
    
    if (eglContext == EGL_NO_CONTEXT) {
        std::cerr << "Error: Failed to create EGL context" << std::endl;
        eglTerminate(display);
        display_ = nullptr;
        return false;
    }
    
    context_ = eglContext;
    
    // Create pbuffer surface (1x1, we'll use FBO for actual rendering)
    const EGLint pbufferAttribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    
    EGLSurface surface = eglCreatePbufferSurface(display, config, pbufferAttribs);
    if (surface == EGL_NO_SURFACE) {
        std::cerr << "Warning: Failed to create EGL pbuffer surface, using surfaceless" << std::endl;
        surface_ = nullptr;
    } else {
        surface_ = surface;
    }
    
    // Make context current
    if (!makeCurrent()) {
        std::cerr << "Error: Failed to make EGL context current" << std::endl;
        destroy();
        return false;
    }
    
    // Load OpenGL functions
    if (!gladLoadGL()) {
        std::cerr << "Error: Failed to load OpenGL functions" << std::endl;
        destroy();
        return false;
    }
    
    initialized_ = true;
    
    std::cout << "OpenGL: " << getGLVersion() << std::endl;
    std::cout << "Renderer: " << getGLRenderer() << std::endl;
    
    return true;
}

bool GLContext::makeCurrent() {
    EGLDisplay display = static_cast<EGLDisplay>(display_);
    EGLSurface surface = surface_ ? static_cast<EGLSurface>(surface_) : EGL_NO_SURFACE;
    EGLContext context = static_cast<EGLContext>(context_);
    return eglMakeCurrent(display, surface, surface, context) == EGL_TRUE;
}

void GLContext::releaseCurrent() {
    EGLDisplay display = static_cast<EGLDisplay>(display_);
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

std::string GLContext::getGLVersion() const {
    if (!initialized_) return "N/A";
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    return version ? version : "Unknown";
}

std::string GLContext::getGLRenderer() const {
    if (!initialized_) return "N/A";
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    return renderer ? renderer : "Unknown";
}

void GLContext::destroy() {
    EGLDisplay display = static_cast<EGLDisplay>(display_);
    
    if (context_) {
        eglDestroyContext(display, static_cast<EGLContext>(context_));
        context_ = nullptr;
    }
    
    if (surface_) {
        eglDestroySurface(display, static_cast<EGLSurface>(surface_));
        surface_ = nullptr;
    }
    
    if (display_) {
        eglTerminate(display);
        display_ = nullptr;
    }
    
    initialized_ = false;
}

} // namespace render
} // namespace rgbd
