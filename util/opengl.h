#ifdef __LINUX__
    #include <GL/glu.h>
#elif __APPLE__
    #include <OpenGL/glu.h>
#endif

#ifdef __LINUX__
    #define GLU_ERROR_STRING(e) gluErrorString(e)
#elif __APPLE__
    const char* osxGluErrorString(GLenum error);
    #define GLU_ERROR_STRING(e) osxGluErrorString(e)
#endif
