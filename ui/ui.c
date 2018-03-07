#include "ui/ui.h"

#include <SDL2/SDL.h>
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_ttf.h>
#include "pattern/pattern.h"
#include "util/config.h"
#include "util/err.h"
#include "util/glsl.h"
#include "util/math.h"
#include "midi/midi.h"
#include "output/output.h"
#include "audio/analyze.h"
#include "ui/render.h"
#include "output/slice.h"
#include "main.h"
#include <stdio.h>
#include <stdbool.h>
#include "util/opengl.h"

static SDL_Window * window;
static SDL_GLContext context;
static SDL_Renderer * renderer;
static bool quit;
static GLhandleARB main_shader;
static GLhandleARB pat_shader;
static GLhandleARB blit_shader;
static GLhandleARB crossfader_shader;
static GLhandleARB text_shader;
static GLhandleARB spectrum_shader;
static GLhandleARB waveform_shader;
static GLhandleARB strip_shader;

static GLuint pat_fb;
static GLuint select_fb;
static GLuint crossfader_fb;
static GLuint pat_entry_fb;
static GLuint spectrum_fb;
static GLuint waveform_fb;
static GLuint strip_fb;
static GLuint select_tex;
static GLuint * pattern_textures;
static SDL_Texture ** pattern_name_textures;
static int * pattern_name_width;
static int * pattern_name_height;
static GLuint crossfader_texture;
static GLuint pat_entry_texture;
static GLuint tex_spectrum_data;
static GLuint spectrum_texture;
static GLuint tex_waveform_data;
static GLuint tex_waveform_beats_data;
static GLuint waveform_texture;
static GLuint strip_texture;

// Window
static int ww; // Window width
static int wh; // Window height

// Mouse
static int mx; // Mouse X
static int my; // Mouse Y
static int mcx; // Mouse click X
static int mcy; // Mouse click Y
static enum {MOUSE_NONE, MOUSE_DRAG_INTENSITY, MOUSE_DRAG_CROSSFADER} ma; // Mouse action
static int mp; // Mouse pattern (index)
static double mci; // Mouse click intensity

// Selection
static int selected = 0;

// Strip indicators
static enum {STRIPS_NONE, STRIPS_SOLID, STRIPS_COLORED} strip_indicator = STRIPS_NONE;

// False colors
#define HIT_NOTHING 0
#define HIT_PATTERN 1
#define HIT_INTENSITY 2
#define HIT_CROSSFADER 3
#define HIT_CROSSFADER_POSITION 4

// Mapping from UI pattern -> deck & slot
// TODO make this live in the INI file
static const int map_x[16] = {100, 275, 450, 625, 1150, 1325, 1500, 1675,
                              100, 275, 450, 625, 1150, 1325, 1500, 1675,};
static const int map_y[16] = {295, 295, 295, 295, 295, 295, 295, 295,
                              55, 55, 55, 55, 55, 55, 55, 55};
static const int map_pe_x[16] = {100, 275, 450, 625, 1150, 1325, 1500, 1675,
                                 100, 275, 450, 625, 1150, 1325, 1500, 1675};
static const int map_pe_y[16] = {420, 420, 420, 420, 420, 420, 420, 420,
                                180, 180, 180, 180, 180, 180, 180, 180};
static const int map_deck[16] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3};
static const int map_pattern[16] = {0, 1, 2, 3, 3, 2, 1, 0, 0, 1, 2, 3, 3, 2, 1, 0};
static const int map_selection[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

static const int crossfader_selection_top = 17;
static const int crossfader_selection_bot = 18;

//                                0   1   2   3   4   5   6   7   8   9  10   11  12  13  14  15  16  17  18
static const int map_left[19] =  {8,  1,  1,  2,  3,  17, 5,  6,  7,  9,  9,  10, 11, 18, 13, 14, 15, 4,  12};
static const int map_right[19] = {1,  2,  3,  4,  17, 6,  7,  8,  8,  10, 11, 12, 18, 14, 15, 16, 16, 5,  13};
static const int map_up[19] =    {1,  1,  2,  3,  4,  5,  6,  7,  8,  1,  2,  3,  4,  5,  6,  7,  8,  17, 17};
static const int map_down[19] =  {9,  9,  10, 11, 12, 13, 14, 15, 16, 9,  10, 11, 12, 13, 14, 15, 16, 18, 18};
static const int map_space[19] = {17, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 17, 18};
static const int map_tab[19] =   {1,  2,  3,  4,  17, 17, 5,  6,  7,  10, 11, 12, 18, 18, 13, 14, 15, 17, 18};
static const int map_stab[19] =  {15, 1,  1,  2,  3,  6,  7,  8,  8,  9,  9,  10, 11, 14, 15, 16, 16, 17, 18};
static const int map_home[19] =  {1,  1,  1,  1,  1,  1,  1,  1,  1,  9,  9,  9,  9,  9,  9,  9,  9,  1,  9};
static const int map_end[19] =   {8,  8,  8,  8,  8,  8,  8,  8,  8,  16, 16, 16, 16, 16, 16, 16, 16, 8,  16};

static int snap_states[19];

// Font
TTF_Font * font = NULL;
static const SDL_Color font_color = {255, 255, 255, 255};

// Pat entry
static bool pat_entry;
static char pat_entry_text[255];

// Timing
static double l_t;

// Deck selector
static int left_deck_selector = 0;
static int right_deck_selector = 1;

// Forward declarations
static void handle_text(const char * text);

//

static void fill(float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(0, h);
    glVertex2f(w, h);
    glVertex2f(w, 0);
    glEnd();
}

static SDL_Texture * render_text(char * text, int * w, int * h) {
    // We need to first render to a surface as that's what TTF_RenderText
    // returns, then load that surface into a texture
    SDL_Surface * surf;
    if(strlen(text) > 0) {
        surf = TTF_RenderText_Blended(font, text, font_color);
    } else {
        surf = TTF_RenderText_Blended(font, " ", font_color);
    }
    if(surf == NULL) FAIL("Could not create surface: %s\n", SDL_GetError());
    if(w != NULL) *w = surf->w;
    if(h != NULL) *h = surf->h;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);
    if(texture == NULL) FAIL("Could not create texture: %s\n", SDL_GetError());
    SDL_FreeSurface(surf);
    return texture;
}

static void render_textbox(char * text, int width, int height) {
    GLint location;

    glUseProgramObjectARB(text_shader);
    location = glGetUniformLocationARB(text_shader, "iResolution");
    glUniform2fARB(location, width, height);

    int text_w;
    int text_h;

    SDL_Texture * tex = render_text(text, &text_w, &text_h);

    location = glGetUniformLocationARB(text_shader, "iTextResolution");
    glUniform2fARB(location, text_w, text_h);
    location = glGetUniformLocationARB(text_shader, "iText");
    glUniform1iARB(location, 0);
    glActiveTexture(GL_TEXTURE0);
    SDL_GL_BindTexture(tex, NULL, NULL);

    glLoadIdentity();
    gluOrtho2D(0, width, 0, height);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    fill(width, height);
    SDL_DestroyTexture(tex);
}

void ui_init() {
    // Init SDL
    if(SDL_Init(SDL_INIT_VIDEO) < 0) FAIL("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    ww = config.ui.window_width;
    wh = config.ui.window_height;

    window = SDL_CreateWindow("Radiance", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, ww, wh, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if(window == NULL) FAIL("Window could not be created: %s\n", SDL_GetError());
    context = SDL_GL_CreateContext(window);
    if(context == NULL) FAIL("OpenGL context could not be created: %s\n", SDL_GetError());
    if(SDL_GL_SetSwapInterval(1) < 0) fprintf(stderr, "Warning: Unable to set VSync: %s\n", SDL_GetError());
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if(renderer == NULL) FAIL("Could not create renderer: %s\n", SDL_GetError());
    if(TTF_Init() < 0) FAIL("Could not initialize 
                            library: %s\n", TTF_GetError());

    // Init OpenGL
    GLenum e;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0, 0, 0, 0);

    // OpenGL starts with GL_INVALID_OPERATION set.  Typically this is cleared by creating a context,
    // but this doesn't seem to be getting cleared properly on OS X.  I'm guessing this is because
    // SDL_GL_CreateContext (which is from a different library) isn't clearing it properly.  It doesn't
    // seem to have any ill affect on operation, and in any event all of the OpenGL UI is getting blown
    // away soon in favor of Qt.
    e = glGetError();
    #ifdef __APPLE__
        if ((e != GL_NO_ERROR) && (e != GL_INVALID_OPERATION)) {
    #else
        if (e != GL_NO_ERROR) {
    #endif
        FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));
    }

    // Make framebuffers
    glGenFramebuffersEXT(1, &select_fb);
    glGenFramebuffersEXT(1, &pat_fb);
    glGenFramebuffersEXT(1, &crossfader_fb);
    glGenFramebuffersEXT(1, &pat_entry_fb);
    glGenFramebuffersEXT(1, &spectrum_fb);
    glGenFramebuffersEXT(1, &waveform_fb);
    glGenFramebuffersEXT(1, &strip_fb);
    if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));

    // Init select texture
    glGenTextures(1, &select_tex);
    glBindTexture(GL_TEXTURE_2D, select_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ww, wh, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, select_fb);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, select_tex, 0);

    // Init pattern textures
    pattern_textures = calloc(config.ui.n_patterns, sizeof(GLuint));
    if(pattern_textures == NULL) MEMFAIL();
    pattern_name_textures = calloc(config.ui.n_patterns, sizeof(SDL_Texture *));
    pattern_name_width = calloc(config.ui.n_patterns, sizeof(int));
    pattern_name_height = calloc(config.ui.n_patterns, sizeof(int));
    if(pattern_name_textures == NULL || pattern_name_width == NULL || pattern_name_height == NULL) MEMFAIL();
    glGenTextures(config.ui.n_patterns, pattern_textures);
    if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));
    for(int i = 0; i < config.ui.n_patterns; i++) {
        glBindTexture(GL_TEXTURE_2D, pattern_textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, config.ui.pattern_width, config.ui.pattern_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }

    // Init crossfader texture
    glGenTextures(1, &crossfader_texture);
    glBindTexture(GL_TEXTURE_2D, crossfader_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, config.ui.crossfader_width, config.ui.crossfader_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, crossfader_fb);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, crossfader_texture, 0);

    // Init pattern entry texture
    glGenTextures(1, &pat_entry_texture);
    glBindTexture(GL_TEXTURE_2D, pat_entry_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, config.ui.pat_entry_width, config.ui.pat_entry_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, pat_entry_fb);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, pat_entry_texture, 0);

    // Spectrum data texture
    glGenTextures(1, &tex_spectrum_data);
    glBindTexture(GL_TEXTURE_1D, tex_spectrum_data);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, config.audio.spectrum_bins, 0, GL_RED, GL_FLOAT, NULL);

    // Spectrum UI element
    glGenTextures(1, &spectrum_texture);
    glBindTexture(GL_TEXTURE_2D, spectrum_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, config.ui.spectrum_width, config.ui.spectrum_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, spectrum_fb);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, spectrum_texture, 0);

    // Waveform data texture
    glGenTextures(1, &tex_waveform_data);
    glBindTexture(GL_TEXTURE_1D, tex_waveform_data);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F, config.audio.waveform_length, 0, GL_RGBA, GL_FLOAT, NULL);

    glGenTextures(1, &tex_waveform_beats_data);
    glBindTexture(GL_TEXTURE_1D, tex_waveform_beats_data);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F, config.audio.waveform_length, 0, GL_RGBA, GL_FLOAT, NULL);

    // Waveform UI element
    glGenTextures(1, &waveform_texture);
    glBindTexture(GL_TEXTURE_2D, waveform_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, config.ui.waveform_width, config.ui.waveform_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, waveform_fb);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, waveform_texture, 0);

    // Strip indicators
    glGenTextures(1, &strip_texture);
    glBindTexture(GL_TEXTURE_2D, strip_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, config.pattern.master_width, config.pattern.master_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, strip_fb);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, strip_texture, 0);

    // Done allocating textures & FBOs, unbind and check for errors
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));

    if((blit_shader = load_shader("resources/blit.glsl")) == 0) FAIL("Could not load blit shader!\n%s", load_shader_error);
    if((main_shader = load_shader("resources/ui_main.glsl")) == 0) FAIL("Could not load UI main shader!\n%s", load_shader_error);
    if((pat_shader = load_shader("resources/ui_pat.glsl")) == 0) FAIL("Could not load UI pattern shader!\n%s", load_shader_error);
    if((crossfader_shader = load_shader("resources/ui_crossfader.glsl")) == 0) FAIL("Could not load UI crossfader shader!\n%s", load_shader_error);
    if((text_shader = load_shader("resources/ui_text.glsl")) == 0) FAIL("Could not load UI text shader!\n%s", load_shader_error);
    if((spectrum_shader = load_shader("resources/ui_spectrum.glsl")) == 0) FAIL("Could not load UI spectrum shader!\n%s", load_shader_error);
    if((waveform_shader = load_shader("resources/ui_waveform.glsl")) == 0) FAIL("Could not load UI waveform shader!\n%s", load_shader_error);
    if((strip_shader = load_shader("resources/strip.glsl")) == 0) FAIL("Could not load strip indicator shader!\n%s", load_shader_error);

    // Stop text input
    SDL_StopTextInput();

    // Open the font
    font = TTF_OpenFont(config.ui.font, config.ui.fontsize);
    if(font == NULL) FAIL("Could not open font %s: %s\n", config.ui.font, SDL_GetError());

    // Init statics
    pat_entry = false;

    SDL_Surface * surf;
    surf = TTF_RenderText_Blended(font, "wtf, why is this necessary", font_color);
    if(surf == NULL) FAIL("Could not create surface: %s\n", SDL_GetError());
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);
    if(texture == NULL) FAIL("Could not create texture: %s\n", SDL_GetError());
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(texture);
}

void ui_term() {
    TTF_CloseFont(font);
    for(int i=0; i<config.ui.n_patterns; i++) {
        if(pattern_name_textures[i] != NULL) SDL_DestroyTexture(pattern_name_textures[i]);
    }
    free(pattern_textures);
    free(pattern_name_textures);
    free(pattern_name_width);
    free(pattern_name_height);
    // TODO glDeleteTextures...
    glDeleteObjectARB(blit_shader);
    glDeleteObjectARB(main_shader);
    glDeleteObjectARB(pat_shader);
    glDeleteObjectARB(crossfader_shader);
    glDeleteObjectARB(text_shader);
    glDeleteObjectARB(spectrum_shader);
    glDeleteObjectARB(waveform_shader);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    window = NULL;
    SDL_Quit();
}

static struct pattern * selected_pattern(int s) {
    for(int i=0; i<config.ui.n_patterns; i++) {
        if(map_selection[i] == s) return deck[map_deck[i]].pattern[map_pattern[i]];
    }
    return NULL;
}

static float get_slider(int s) {
    if(s == crossfader_selection_top || s == crossfader_selection_bot)
        return crossfader.position;

    struct pattern * p = selected_pattern(s);
    if(p != NULL)
        return p->intensity;

    return 0.;
}

static void set_slider_to(int s, float v, int snap) {
    if (snap && snap_states[s] != snap) {
        if (ABS(get_slider(s) - v) > params.ui.snap_threshold)
            return;
    }
    v = CLAMP(v, 0., 1.);

    if(s == crossfader_selection_top || s == crossfader_selection_bot) {
        crossfader.position = v;
    } else {
        struct pattern * p = selected_pattern(s);
        if(p != NULL)
            p->intensity = v;
    }

    snap_states[s] = snap;
}

static void increment_slider(int s, float v) {
    set_slider_to(s, v + get_slider(s), 0);
}

static void redraw_pattern_ui(int s) {
    snap_states[s] = 0;
    const struct pattern * p = deck[map_deck[s]].pattern[map_pattern[s]];
    if (p == NULL) return;
    
    if(pattern_name_textures[s] != NULL) SDL_DestroyTexture(pattern_name_textures[s]);
    pattern_name_textures[s] = render_text(p->name, &pattern_name_width[s], &pattern_name_height[s]);
}

static void handle_key(SDL_KeyboardEvent * e) {
    // See SDLKey man page
    bool shift = e->keysym.mod & KMOD_SHIFT;
    //bool ctrl = e->keysym.mod & KMOD_CTRL;
    //bool alt = e->keysym.mod & KMOD_ALT;

    // Currently none of these modifiers are used
    // If they're held down then the key probably isn't for us
    if (e->keysym.mod & ~(KMOD_SHIFT))
        return;

    if(pat_entry) {
        switch(e->keysym.sym) {
            case SDLK_RETURN:
                for(int i=0; i<config.ui.n_patterns; i++) {
                    if(map_selection[i] == selected) {
                        if (deck_load_set(&deck[map_deck[i]], pat_entry_text) == 0) {
                            for (int j = 0; j < config.ui.n_patterns; j++) {
                                if (map_deck[j] == map_deck[selected])
                                    redraw_pattern_ui(j);
                            }
                        } else if(deck_load_pattern(&deck[map_deck[i]], map_pattern[i], pat_entry_text, -1) == 0) {
                            redraw_pattern_ui(i);
                        }
                        break;
                    }
                }
                pat_entry = false;
                SDL_StopTextInput();
                break;
            case SDLK_ESCAPE:
                pat_entry = false;
                SDL_StopTextInput();
                break;
            case SDLK_BACKSPACE:
                if (pat_entry_text[0] != '\0') {
                    pat_entry_text[strlen(pat_entry_text)-1] = '\0';
                    handle_text("\0");
                }
                break;
            default:
                break;
        }
    } else {
        DEBUG("Keysym: %u '%c'", e->keysym.sym, e->keysym.sym);
        switch(e->keysym.sym) {
            case SDLK_h:
            case SDLK_LEFT:
                selected = map_left[selected];
                break;
            case SDLK_l:
            case SDLK_RIGHT:
                selected = map_right[selected];
                break;
            case SDLK_UP:
            case SDLK_k:
                if (shift) increment_slider(selected, +0.1);
                else selected = map_up[selected];
                break;
            case SDLK_DOWN:
            case SDLK_j:
                if (shift) increment_slider(selected, -0.1);
                else selected = map_down[selected];
                break;
            case SDLK_ESCAPE:
                selected = 0;
                break;
            case SDLK_DELETE:
            case SDLK_d:
                for(int i=0; i<config.ui.n_patterns; i++) {
                    if(map_selection[i] == selected) {
                        deck_unload_pattern(&deck[map_deck[i]], map_pattern[i]);
                        break;
                    }
                }
                break;
            case SDLK_BACKQUOTE:
                set_slider_to(selected, 0, 0);
                break;
            case SDLK_1:
                set_slider_to(selected, 0.1, 0);
                break;
            case SDLK_2:
                set_slider_to(selected, 0.2, 0);
                break;
            case SDLK_3:
                set_slider_to(selected, 0.3, 0);
                break;
            case SDLK_4:
                if(shift) {
                    selected = map_end[selected];
                } else {
                    set_slider_to(selected, 0.4, 0);
                }
                break;
            case SDLK_5:
                set_slider_to(selected, 0.5, 0);
                break;
            case SDLK_6:
                if(shift) {
                    selected = map_home[selected];
                } else {
                    set_slider_to(selected, 0.6, 0);
                }
                break;
            case SDLK_7:
                set_slider_to(selected, 0.7, 0);
                break;
            case SDLK_8:
                set_slider_to(selected, 0.8, 0);
                break;
            case SDLK_9:
                set_slider_to(selected, 0.9, 0);
                break;
            case SDLK_0:
                set_slider_to(selected, 1, 0);
                break;
            case SDLK_SEMICOLON: if(!shift) break;
                for(int i=0; i<config.ui.n_patterns; i++) {
                    if(map_selection[i] == selected) {
                        pat_entry = true;
                        pat_entry_text[0] = '\0';
                        SDL_StartTextInput();
                        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, pat_entry_fb);
                        render_textbox(pat_entry_text, config.ui.pat_entry_width, config.ui.pat_entry_height);
                        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
                    }
                }
                break;
            case SDLK_RETURN:
                for(int i=0; i<config.ui.n_patterns; i++) {
                    if(map_selection[i] == selected) {
                        if(i < 4) {
                            left_deck_selector = 0;
                        } else if(i < 8) {
                            right_deck_selector = 1;
                        } else if(i < 12) {
                            left_deck_selector = 2;
                        } else if(i < 16) {
                            right_deck_selector = 3;
                        }
                    }
                }
                break;
            case SDLK_LEFTBRACKET:
                if(left_deck_selector == 0) {
                    left_deck_selector = 2;
                } else {
                    left_deck_selector = 0;
                }
                break;
            case SDLK_RIGHTBRACKET:
                if(right_deck_selector == 1) {
                    right_deck_selector = 3;
                } else {
                    right_deck_selector = 1;
                }
                break;
            case SDLK_SPACE:
                selected = map_space[selected];
                break;
            case SDLK_TAB:
                if(shift) {
                    selected = map_stab[selected];
                } else {
                    selected = map_tab[selected];
                }
                break;
            case SDLK_HOME:
                selected = map_home[selected];
                break;
            case SDLK_END:
                selected = map_end[selected];
                break;
            case SDLK_r:
                params_refresh();
                if (shift) {
                    midi_refresh();
                    output_refresh();
                }
                break;
            case SDLK_w:
                if (shift) {
                    for(int i=0; i<config.ui.n_patterns; i++) {
                        if(map_selection[i] == selected)
                            deck_save(&deck[map_deck[i]], "NAME");
                    }
                }
                break;
            case SDLK_q:
                switch(strip_indicator) {
                    case STRIPS_NONE:
                        strip_indicator = STRIPS_SOLID;
                        break;
                    case STRIPS_SOLID:
                        strip_indicator = STRIPS_COLORED;
                        break;
                    case STRIPS_COLORED:
                    default:
                        strip_indicator = STRIPS_NONE;
                        break;
                }
                break;
            default:
                break;
        }
    }
}

static void blit(float x, float y, float w, float h) {
    GLint location;
    location = glGetUniformLocationARB(blit_shader, "iPosition");
    glUniform2fARB(location, x, y);
    location = glGetUniformLocationARB(blit_shader, "iResolution");
    glUniform2fARB(location, w, h);

    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x, y + h);
    glVertex2f(x + w, y + h);
    glVertex2f(x + w, y);
    glEnd();
}

static void ui_render(bool select) {
    GLint location;
    GLenum e;

    // Render strip indicators
    switch(strip_indicator) {
        case STRIPS_SOLID:
        case STRIPS_COLORED:
            glLoadIdentity();
            glViewport(0, 0, config.pattern.master_width, config.pattern.master_height);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, strip_fb);
            glUseProgramObjectARB(strip_shader);

            location = glGetUniformLocationARB(strip_shader, "iPreview");
            glUniform1iARB(location, 0);
            location = glGetUniformLocationARB(strip_shader, "iResolution");
            glUniform2fARB(location, config.pattern.master_width, config.pattern.master_height);
            location = glGetUniformLocationARB(strip_shader, "iIndicator");
            glUniform1iARB(location, strip_indicator);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, crossfader.tex_output);

            glClear(GL_COLOR_BUFFER_BIT);
            glBegin(GL_QUADS);
            for(struct output_device * d = output_device_head; d != NULL; d = d->next) {
#ifdef SOLID_LINE_INDICATOR
                bool first = true;
                double x;
                double y;
                for(struct output_vertex * v = d->vertex_head; v != NULL; v = v->next) {
                    if(!first) {
                        double dx = v->x - x;
                        double dy = -v->y - y;
                        double dl = hypot(dx, dy);
                        dx = config.ui.strip_thickness * dx / dl;
                        dy = config.ui.strip_thickness * dy / dl;
                        glVertex2d(x + dy, y - dx);
                        glVertex2d(v->x + dy, -v->y - dx);
                        glVertex2d(v->x - dy, -v->y + dx);
                        glVertex2d(x - dy, y + dx);
                    } else {
                        first = false;
                    }
                    x = v->x;
                    y = -v->y;
                }
#else // PIXEL INDICATOR
                // Maybe this is horrendously slow because it has to draw a quad for every output pixel? 
                // It looks cool though
                for (size_t i = 0; i < d->pixels.length; i++) {
                    double x = d->pixels.xs[i];
                    double y = d->pixels.ys[i];
                    double dx = config.ui.point_thickness;
                    double dy = config.ui.point_thickness;
                    glVertex2d(x + dx, y);
                    glVertex2d(x, y + dy);
                    glVertex2d(x - dx, y);
                    glVertex2d(x, y - dy);
                }
#endif
            }
            glEnd();
            break;
        default:
        case STRIPS_NONE:
            break;
    }

    // Render the patterns
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, pat_fb);

    int pw = config.ui.pattern_width;
    int ph = config.ui.pattern_height;
    glUseProgramObjectARB(pat_shader);
    location = glGetUniformLocationARB(pat_shader, "iResolution");
    glUniform2fARB(location, pw, ph);
    glUseProgramObjectARB(pat_shader);
    location = glGetUniformLocationARB(pat_shader, "iSelection");
    glUniform1iARB(location, select);
    location = glGetUniformLocationARB(pat_shader, "iPreview");
    glUniform1iARB(location, 0);
    location = glGetUniformLocationARB(pat_shader, "iName");
    glUniform1iARB(location, 1);
    GLint pattern_index = glGetUniformLocationARB(pat_shader, "iPatternIndex");
    GLint pattern_intensity = glGetUniformLocationARB(pat_shader, "iIntensity");
    GLint name_resolution = glGetUniformLocationARB(pat_shader, "iNameResolution");

    glLoadIdentity();
    gluOrtho2D(0, pw, 0, ph);
    glViewport(0, 0, pw, ph);

    for(int i = 0; i < config.ui.n_patterns; i++) {
        struct pattern * p = deck[map_deck[i]].pattern[map_pattern[i]];
        if(p != NULL) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, p->tex_output);
            glActiveTexture(GL_TEXTURE1);
            SDL_GL_BindTexture(pattern_name_textures[i], NULL, NULL);
            glUniform1iARB(pattern_index, i);
            glUniform1fARB(pattern_intensity, p->intensity);
            glUniform2fARB(name_resolution, pattern_name_width[i], pattern_name_height[i]);
            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, pattern_textures[i], 0);
            glClear(GL_COLOR_BUFFER_BIT);
            fill(pw, ph);
        }
    }

    // Render the crossfader
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, crossfader_fb);

    int cw = config.ui.crossfader_width;
    int ch = config.ui.crossfader_height;
    glUseProgramObjectARB(crossfader_shader);
    location = glGetUniformLocationARB(crossfader_shader, "iResolution");
    glUniform2fARB(location, cw, ch);
    location = glGetUniformLocationARB(crossfader_shader, "iSelection");
    glUniform1iARB(location, select);
    location = glGetUniformLocationARB(crossfader_shader, "iPreview");
    glUniform1iARB(location, 0);
    location = glGetUniformLocationARB(crossfader_shader, "iStrips");
    glUniform1iARB(location, 1);
    location = glGetUniformLocationARB(crossfader_shader, "iIntensity");
    glUniform1fARB(location, crossfader.position);
    location = glGetUniformLocationARB(crossfader_shader, "iIndicator");
    glUniform1iARB(location, strip_indicator);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, crossfader.tex_output);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, strip_texture);

    glLoadIdentity();
    gluOrtho2D(0, cw, 0, ch);
    glViewport(0, 0, cw, ch);
    glClear(GL_COLOR_BUFFER_BIT);
    fill(cw, ch);

    int sw = 0;
    int sh = 0;
    int vw = 0;
    int vh = 0;
    if(!select) {
        analyze_render(tex_spectrum_data, tex_waveform_data, tex_waveform_beats_data);

        // Render the spectrum
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, spectrum_fb);

        sw = config.ui.spectrum_width;
        sh = config.ui.spectrum_height;
        glUseProgramObjectARB(spectrum_shader);
        location = glGetUniformLocationARB(spectrum_shader, "iResolution");
        glUniform2fARB(location, sw, sh);
        location = glGetUniformLocationARB(spectrum_shader, "iBins");
        glUniform1iARB(location, config.audio.spectrum_bins);
        location = glGetUniformLocationARB(spectrum_shader, "iSpectrum");
        glUniform1iARB(location, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_1D, tex_spectrum_data);

        glLoadIdentity();
        gluOrtho2D(0, sw, 0, sh);
        glViewport(0, 0, sw, sh);
        glClear(GL_COLOR_BUFFER_BIT);
        fill(sw, sh);

        // Render the waveform
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, waveform_fb);

        vw = config.ui.waveform_width;
        vh = config.ui.waveform_height;
        glUseProgramObjectARB(waveform_shader);
        location = glGetUniformLocationARB(waveform_shader, "iResolution");
        glUniform2fARB(location, sw, sh);
        location = glGetUniformLocationARB(waveform_shader, "iLength");
        glUniform1iARB(location, config.audio.waveform_length);
        location = glGetUniformLocationARB(waveform_shader, "iWaveform");
        glUniform1iARB(location, 0);
        location = glGetUniformLocationARB(waveform_shader, "iBeats");
        glUniform1iARB(location, 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_1D, tex_waveform_data);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_1D, tex_waveform_beats_data);

        glLoadIdentity();
        gluOrtho2D(0, vw, 0, vh);
        glViewport(0, 0, vw, vh);
        glClear(GL_COLOR_BUFFER_BIT);
        fill(vw, vh);
    }

    // Render to screen (or select fb)
    if(select) {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, select_fb);
    } else {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    }

    glLoadIdentity();
    gluOrtho2D(0, ww, 0, wh);
    glViewport(0, 0, ww, wh);

    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgramObjectARB(main_shader);

    location = glGetUniformLocationARB(main_shader, "iResolution");
    glUniform2fARB(location, ww, wh);
    location = glGetUniformLocationARB(main_shader, "iSelection");
    glUniform1iARB(location, select);
    location = glGetUniformLocationARB(main_shader, "iSelected");
    glUniform1iARB(location, selected);
    location = glGetUniformLocationARB(main_shader, "iLeftDeckSelector");
    glUniform1iARB(location, left_deck_selector);
    location = glGetUniformLocationARB(main_shader, "iRightDeckSelector");
    glUniform1iARB(location, right_deck_selector);

    fill(ww, wh);

    // Blit UI elements on top
    glEnable(GL_BLEND);
    glUseProgramObjectARB(blit_shader);
    glActiveTexture(GL_TEXTURE0);
    location = glGetUniformLocationARB(blit_shader, "iTexture");
    glUniform1iARB(location, 0);

    for(int i = 0; i < config.ui.n_patterns; i++) {
        struct pattern * pattern = deck[map_deck[i]].pattern[map_pattern[i]];
        if(pattern != NULL) {
            glBindTexture(GL_TEXTURE_2D, pattern_textures[i]);
            blit(map_x[i], map_y[i], pw, ph);
        }
    }

    glBindTexture(GL_TEXTURE_2D, crossfader_texture);
    blit(config.ui.crossfader_x, config.ui.crossfader_y, cw, ch);

    if(!select) {
        glBindTexture(GL_TEXTURE_2D, spectrum_texture);
        blit(config.ui.spectrum_x, config.ui.spectrum_y, sw, sh);

        glBindTexture(GL_TEXTURE_2D, waveform_texture);
        blit(config.ui.waveform_x, config.ui.waveform_y, vw, vh);

        if(pat_entry) {
            for(int i = 0; i < config.ui.n_patterns; i++) {
                if(map_selection[i] == selected) {
                    glBindTexture(GL_TEXTURE_2D, pat_entry_texture);
                    blit(map_pe_x[i], map_pe_y[i], config.ui.pat_entry_width, config.ui.pat_entry_height);
                    break;
                }
            }
        }
    }

    glDisable(GL_BLEND);

    if((e = glGetError()) != GL_NO_ERROR) FAIL("OpenGL error: %s\n", GLU_ERROR_STRING(e));
}

struct rgba {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

static struct rgba test_hit(int x, int y) {
    struct rgba data;

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, select_fb);
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &data);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    return data;
}

static void handle_mouse_move() {
    struct pattern * p;
    switch(ma) {
        case MOUSE_NONE:
            break;
        case MOUSE_DRAG_INTENSITY:
            p = deck[map_deck[mp]].pattern[map_pattern[mp]];
            if(p != NULL) {
                p->intensity = mci + (mx - mcx) * config.ui.intensity_gain_x + (my - mcy) * config.ui.intensity_gain_y;
                if(p->intensity > 1) p->intensity = 1;
                if(p->intensity < 0) p->intensity = 0;
            }
            break;
        case MOUSE_DRAG_CROSSFADER:
            crossfader.position = mci + (mx - mcx) * config.ui.crossfader_gain_x + (my - mcy) * config.ui.crossfader_gain_y;
            if(crossfader.position > 1) crossfader.position = 1;
            if(crossfader.position < 0) crossfader.position = 0;
            break;
    }
}

static void handle_mouse_up() {
    ma = MOUSE_NONE;
}

static void handle_text(const char * text) {
    if(pat_entry) {
        if(strlen(pat_entry_text) + strlen(text) < sizeof(pat_entry_text)) {
            strcat(pat_entry_text, text);
        }
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, pat_entry_fb);
        render_textbox(pat_entry_text, config.ui.pat_entry_width, config.ui.pat_entry_height);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    }
}

static void handle_mouse_down() {
    struct rgba hit;
    hit = test_hit(mx, wh - my);
    switch(hit.r) {
        case HIT_NOTHING:
            selected = 0;
            break;
        case HIT_PATTERN:
            if(hit.g < config.ui.n_patterns) selected = map_selection[hit.g];
            break;
        case HIT_INTENSITY:
            if(hit.g < config.ui.n_patterns) {
                struct pattern * p = deck[map_deck[hit.g]].pattern[map_pattern[hit.g]];
                if(p != NULL) {
                    ma = MOUSE_DRAG_INTENSITY;
                    mp = hit.g;
                    mcx = mx;
                    mcy = my;
                    mci = p->intensity;
                }
            }
            break;
        case HIT_CROSSFADER:
            selected = crossfader_selection_top;
            break;
        case HIT_CROSSFADER_POSITION:
            ma = MOUSE_DRAG_CROSSFADER;
            mcx = mx;
            mcy = my;
            mci = crossfader.position;
            break;
    }
}

void ui_run() {
        SDL_Event e;

        quit = false;
        while(!quit) {
            ui_render(true);

            while(SDL_PollEvent(&e) != 0) {
                if (midi_command_event != (Uint32) -1 && 
                    e.type == midi_command_event) {
                    struct midi_event * me = e.user.data1;
                    switch (me->type) {
                    case MIDI_EVENT_SLIDER:
                        set_slider_to(me->slider.index, me->slider.value, me->snap);
                        break;
                    case MIDI_EVENT_KEY:;
                        SDL_KeyboardEvent fakekeyev;
                        memset(&fakekeyev, 0, sizeof fakekeyev);
                        fakekeyev.type = SDL_KEYDOWN;
                        fakekeyev.state = SDL_PRESSED;
                        fakekeyev.keysym.sym = me->key.keycode[0];
                        handle_key(&fakekeyev);
                        break;
                    }
                    free(e.user.data1);
                    free(e.user.data2);
                    continue;
                }
                switch(e.type) {
                    case SDL_QUIT:
                        quit = true;
                        break;
                    case SDL_KEYDOWN:
                        handle_key(&e.key);
                        break;
                    case SDL_MOUSEMOTION:
                        mx = e.motion.x;
                        my = e.motion.y;
                        handle_mouse_move();
                        break;
                    case SDL_MOUSEBUTTONDOWN:
                        mx = e.button.x;
                        my = e.button.y;
                        switch(e.button.button) {
                            case SDL_BUTTON_LEFT:
                                handle_mouse_down();
                                break;
                        }
                        break;
                    case SDL_MOUSEBUTTONUP:
                        mx = e.button.x;
                        my = e.button.y;
                        switch(e.button.button) {
                            case SDL_BUTTON_LEFT:
                                handle_mouse_up();
                                break;
                        }
                        break;
                    case SDL_TEXTINPUT:
                        handle_text(e.text.text);
                        break;
                }
            }

            for(int i=0; i<N_DECKS; i++) {
                deck_render(&deck[i]);
            }
            crossfader_render(&crossfader, deck[left_deck_selector].tex_output, deck[right_deck_selector].tex_output);
            ui_render(false);

            render_readback(&render);

            SDL_GL_SwapWindow(window);

            double cur_t = SDL_GetTicks();
            double dt = cur_t - l_t;
            if(dt > 0) time += dt / 1000;
            l_t = cur_t;
        }
}

