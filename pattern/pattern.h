#pragma once

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "util/opengl.h"
#include <stdbool.h>

#define MAX_INTEGRAL 1024

#define RADIANCE_PATTERN_GIF_SPEED 100

struct pattern {
    GLhandleARB * shader;
    int n_shaders;
    char * name;
    double intensity;
    double intensity_integral;

    int flip;
    GLuint * tex;
    GLuint fb;
    GLint * uni_tex;
    GLuint tex_output;

    // We don't actually need both of these ints as given
    // the current frame you can compute the start of the image
    // listing, but it's unnecessarily complicated and makes cleanup
    // more unpleasant
    int n_frames;
    int current_frame;
    GLuint * frames;

    // The last time_master.wall_ms we were rendered at
    long last_ms;
};

int pattern_init(struct pattern * pattern, const char * prefix);
void pattern_term(struct pattern * pattern);
void pattern_render(struct pattern * pattern, GLuint input_tex);
