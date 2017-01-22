#include "pattern/pattern.h"
#include "time/timebase.h"
#include "util/glsl.h"
#include "util/string.h"
#include "util/err.h"
#include "util/config.h"
#include "main.h"

#include <assert.h>
#include <errno.h>
#include <IL/il.h>
#include <IL/ilu.h>
#include <IL/ilut.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

static bool il_initted = false;

int pattern_init(struct pattern * pattern, const char * prefix) {
    GLenum e;

    memset(pattern, 0, sizeof *pattern);

    pattern->intensity = 0;
    pattern->intensity_integral = 0;
    pattern->name = strdup(prefix);
    if(pattern->name == NULL) ERROR("Could not allocate memory");

    int n = 0;
    for(;;) {
        char * filename;
        struct stat statbuf;
        filename = rsprintf("%s%s.%d.glsl", config.pattern.dir, prefix, n);
        if(filename == NULL) MEMFAIL();

        int rc = stat(filename, &statbuf);
        free(filename);

        if (rc != 0 || S_ISDIR(statbuf.st_mode)) {
            break;
        }
        n++;
    }

    if(n == 0) {
        ERROR("Could not find any shaders for %s, trying to load an image", prefix);

        // Now try to load an image
        char * filename;
        filename = rsprintf("%s%s", config.images.dir, prefix);
        if (filename == NULL) MEMFAIL();

        if (!il_initted) {
            // See http://openil.sourceforge.net/docs/DevIL%20Manual.pdf for this sequence
            ilInit();
            iluInit();
            ilutRenderer(ILUT_OPENGL);

            il_initted = true;
        }

        // Give ourselves an image name to bind to.
        ILuint image_info;
        ilGenImages(1, &image_info);
        ilBindImage(image_info);

        if (!ilLoadImage(filename)) {
            ERROR("Could not load image: %s", iluErrorString(ilGetError()));
        }

        pattern->n_frames = ilGetInteger(IL_NUM_IMAGES) + 1;
        INFO("Found %d frames in image %s", pattern->n_frames, prefix);

        pattern->frames = calloc(pattern->n_frames, sizeof *pattern->frames);

        ILenum bindError;

        for (int i = 0; i < pattern->n_frames; i++) {
            // It's really important to call this each time or it has trouble loading frames (I suspect
            // this is resetting a pointer into an array of frames somewhere)
            ilBindImage(image_info);

            if (ilActiveImage(i) == IL_FALSE) {
                ERROR("Error setting active frame %d of image: %s", i, iluErrorString(ilGetError()));

                return -1;
            }

            pattern->frames[i] = ilutGLBindTexImage();

            bindError = ilGetError();
            if (bindError != IL_NO_ERROR) {
                ERROR("Error binding frame %d of image: %s", i, iluErrorString(bindError));
                return -1;
            }
        }

        INFO("Succcessfully loaded image %s", prefix);

        // Now that it's been loaded into OpenGL delete it.
        ilDeleteImages(1, &image_info);

        pattern->current_frame = 0;

        n = 1;
    }

    pattern->n_shaders = n;

    pattern->shader = calloc(pattern->n_shaders, sizeof *pattern->shader);
    if(pattern->shader == NULL) MEMFAIL();
    pattern->tex = calloc(pattern->n_shaders, sizeof *pattern->tex);
    if(pattern->tex == NULL) MEMFAIL();

    bool success = true;
    for(int i = 0; i < pattern->n_shaders; i++) {
        char * filename;
        if (pattern->frames) {
            filename = rsprintf("%simage.0.glsl", config.pattern.dir);
        } else {
            filename = rsprintf("%s%s.%d.glsl", config.pattern.dir, prefix, i);
        }
        if(filename == NULL) MEMFAIL();

        GLhandleARB h = load_shader(filename);

        if (h == 0) {
            fprintf(stderr, "%s", load_shader_error);
            WARN("Unable to load shader %s", filename);
            success = false;
        } else {
            pattern->shader[i] = h;
            DEBUG("Loaded shader #%d", i);
        }
        free(filename);
    }
    if(!success) {
        ERROR("Failed to load some shaders.");
        return 2;
    }

    if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));

    // Render targets
    glGenFramebuffersEXT(1, &pattern->fb);
    glGenTextures(pattern->n_shaders + 1, pattern->tex);

    if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));

    for(int i = 0; i < pattern->n_shaders + 1; i++) {
        glBindTexture(GL_TEXTURE_2D, pattern->tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, config.pattern.master_width, config.pattern.master_height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, pattern->fb);
    for(int i = 0; i < pattern->n_shaders + 1; i++) {
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D,
                                  pattern->tex[i], 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));

    // Some OpenGL API garbage
    pattern->uni_tex = calloc(pattern->n_shaders, sizeof *pattern->uni_tex);
    if(pattern->uni_tex == NULL) MEMFAIL();
    for(int i = 0; i < pattern->n_shaders; i++) {
        pattern->uni_tex[i] = i + 1;
    }

    return 0;
}

void pattern_term(struct pattern * pattern) {
    GLenum e;

    for (int i = 0; i < pattern->n_shaders; i++) {
        glDeleteObjectARB(pattern->shader[i]);
    }

    glDeleteTextures(pattern->n_shaders + 1, pattern->tex);
    glDeleteFramebuffersEXT(1, &pattern->fb);

    if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));

    free(pattern->name);

    if (pattern->frames) {
        // Free the textures pointed to by the array
        glDeleteTextures(pattern->n_frames, pattern->frames);
        // Free the array itself
        free(pattern->frames);
    }

    memset(pattern, 0, sizeof *pattern);
}

void pattern_render(struct pattern * pattern, GLuint input_tex) {
    GLenum e;

    glLoadIdentity();
    glViewport(0, 0, config.pattern.master_width, config.pattern.master_height);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, pattern->fb);

    pattern->intensity_integral = fmod(pattern->intensity_integral + pattern->intensity / config.ui.fps, MAX_INTEGRAL);

    for (int i = pattern->n_shaders - 1; i >= 0; i--) {
        glUseProgramObjectARB(pattern->shader[i]);

        // Don't worry about this part.
        for(int j = 0; j < pattern->n_shaders; j++) {
            // Or, worry about it, but don't think about it.
            glActiveTexture(GL_TEXTURE1 + j);
            glBindTexture(GL_TEXTURE_2D, pattern->tex[(pattern->flip + j + (i < j)) % (pattern->n_shaders + 1)]);
        }
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D,
                                  pattern->tex[(pattern->flip + i + 1) % (pattern->n_shaders + 1)], 0);

        if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));

        GLint loc;
        loc = glGetUniformLocationARB(pattern->shader[i], "iTime");
        glUniform1fARB(loc, time_master.beat_frac + time_master.beat_index);
        loc = glGetUniformLocationARB(pattern->shader[i], "iAudioHi");
        glUniform1fARB(loc, audio_hi);
        loc = glGetUniformLocationARB(pattern->shader[i], "iAudioMid");
        glUniform1fARB(loc, audio_mid);
        loc = glGetUniformLocationARB(pattern->shader[i], "iAudioLow");
        glUniform1fARB(loc, audio_low);
        loc = glGetUniformLocationARB(pattern->shader[i], "iAudioLevel");
        glUniform1fARB(loc, audio_level);
        loc = glGetUniformLocationARB(pattern->shader[i], "iResolution");
        glUniform2fARB(loc, config.pattern.master_width, config.pattern.master_height);
        loc = glGetUniformLocationARB(pattern->shader[i], "iIntensity");
        glUniform1fARB(loc, pattern->intensity);
        loc = glGetUniformLocationARB(pattern->shader[i], "iIntensityIntegral");
        glUniform1fARB(loc, pattern->intensity_integral);
        loc = glGetUniformLocationARB(pattern->shader[i], "iFPS");
        glUniform1fARB(loc, config.ui.fps);
        loc = glGetUniformLocationARB(pattern->shader[i], "iFrame");
        glUniform1iARB(loc, 0);
        loc = glGetUniformLocationARB(pattern->shader[i], "iChannel");
        glUniform1ivARB(loc, pattern->n_shaders, pattern->uni_tex);

        if (pattern->frames) {
            int texture_index = pattern->n_shaders + 1;
            glActiveTexture(GL_TEXTURE0 + texture_index);
            glBindTexture(GL_TEXTURE_2D, pattern->frames[pattern->current_frame]);

            loc = glGetUniformLocationARB(pattern->shader[i], "iImage");
            glUniform1iARB(loc, texture_index);

            // TODO: Should do something interesting off of beat_frac and beat_index
            // to make the animation go faster/slower depending on the music
            if (pattern->last_ms / RADIANCE_PATTERN_GIF_SPEED != time_master.wall_ms / RADIANCE_PATTERN_GIF_SPEED) {
                pattern->current_frame = (pattern->current_frame + 1) % pattern->n_frames;
            }
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, input_tex);

        if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));

        glClear(GL_COLOR_BUFFER_BIT);
        glBegin(GL_QUADS);
        glVertex2d(-1, -1);
        glVertex2d(-1, 1);
        glVertex2d(1, 1);
        glVertex2d(1, -1);
        glEnd();

        if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));
    }
    pattern->flip = (pattern->flip + 1) % (pattern->n_shaders + 1);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));
    pattern->tex_output = pattern->tex[pattern->flip];

    pattern->last_ms = time_master.wall_ms;
}
