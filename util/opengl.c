#include "opengl.h"

const char* osxGluErrorString(GLenum error) {
    switch (error) {
        case GL_INVALID_ENUM: return "invalid enumerant";
        case GL_INVALID_VALUE: return "invalid value";
        case GL_INVALID_OPERATION: return "invalid operation";
        case GL_STACK_OVERFLOW: return "stack overflow";
        case GL_STACK_UNDERFLOW: return "stack underflow";
        case GL_OUT_OF_MEMORY: return "out of memory";
        case GL_TABLE_TOO_LARGE: return "table too large";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "invalid framebuffer operation";
        default: return "unknown GL error";
     }
}
