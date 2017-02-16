#include "ui/render.h"

#include "util/err.h"
#include "util/config.h"

#define BYTES_PER_PIXEL 4 // RGBA

void render_init(struct render * render, GLint texture) {
    GLenum e;

    memset(render, 0, sizeof *render);
    render->pixels = calloc(config.pattern.master_width * config.pattern.master_height * BYTES_PER_PIXEL, sizeof(uint8_t));
    if(render->pixels == NULL) MEMFAIL();

    glGenFramebuffersEXT(1, &render->fb);
    if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, render->fb);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texture, 0);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));

    render->mutex = SDL_CreateMutex();
    if(render->mutex == NULL) FAIL("Could not create mutex: %s\n", SDL_GetError());
}

void render_term(struct render * render) {
    free(render->pixels);
    glDeleteFramebuffersEXT(1, &render->fb);
    SDL_DestroyMutex(render->mutex);
    memset(render, 0, sizeof *render);
}

void render_readback(struct render * render) {
    GLenum e;

    if(SDL_TryLockMutex(render->mutex) == 0) {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, render->fb);
        glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
        glReadPixels(0, 0, config.pattern.master_width, config.pattern.master_height, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)render->pixels);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));
        SDL_UnlockMutex(render->mutex);
    }
}

void render_freeze(struct render * render) {
    SDL_LockMutex(render->mutex);
}

void render_thaw(struct render * render) {
    SDL_UnlockMutex(render->mutex);
}

SDL_Color render_sample(struct render * render, float x, float y) {
    int col = 0.5 * (x + 1) * config.pattern.master_width;
    int row = 0.5 * (-y + 1) * config.pattern.master_height;
    if(col < 0) col = 0;
    if(row < 0) row = 0;
    if(col >= config.pattern.master_width) col = config.pattern.master_width - 1;
    if(row >= config.pattern.master_height) row = config.pattern.master_height - 1;
    long index = BYTES_PER_PIXEL * (row * config.pattern.master_height + col);

    // Use NEAREST interpolation for now
    SDL_Color c;
    c.r = render->pixels[index];
    c.g = render->pixels[index + 1];
    c.b = render->pixels[index + 2];
    c.a = render->pixels[index + 3];
    return c;
}
