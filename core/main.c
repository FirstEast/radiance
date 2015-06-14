#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL/SDL_thread.h>
#include <SDL/SDL_framerate.h>

#include "core/err.h"
#include "core/config.h"
#include "core/slot.h"
#include "core/state.h"
#include "core/audio.h"
#include "filters/filter.h"
#include "output/output.h"
#include "output/slice.h"
#include "midi/midi.h"
#include "patterns/pattern.h"
#include "patterns/static.h"
#include "signals/signal.h"
#include "timebase/timebase.h"
#include "util/color.h"
#include "ui/ui.h"
#include "ui/layout.h"

int main()
{
    colormap_test_all();
    colormap_set_global(&cm_rainbow_edged);
    colormap_set_mono(0.5);

    pattern_init();
    patterns_updating = SDL_CreateMutex();

    config_load(&config, "config.ini");
    layout_load(&layout, config.path.layout);
    
    config_dump(&config, "config.ini");
    layout_dump(&layout, config.path.layout);

    int ui_loaded = 0;
    if(config.ui.enabled){
        ui_init();
        ui_loaded = 1;
    }

    pat_load(&slots[0], &pat_rainbow);
    pat_load(&slots[1], &pat_fade);
    pat_load(&slots[2], &pat_strobe);
    pat_load(&slots[3], &pat_bubble);
    pat_load(&slots[4], &pat_strobe);
    pat_load(&slots[5], &pat_bubble);
    pat_load(&slots[6], &pat_swipe);
    pat_load(&slots[7], &pat_strobe);

    for(int i = 0; i < 8; i++){
        param_state_setq(&slots[i].param_states[0], (rand() % 1000) / 1000.);
    }

    filters_load();

    audio_start();
    midi_start();
    signal_start();
    output_start();

    FPSmanager fps_manager;
    SDL_initFramerate(&fps_manager);
    SDL_setFramerate(&fps_manager, 100);
    if(ui_loaded){
        while(ui_poll())
        {
            ui_render();
            stat_fps = 1000. / SDL_framerateDelay(&fps_manager);
        }
    }else{
        state_load("state_0.ini");
        while(1){
            int i = 0;
            printf("Load configuration [0-%d]: ", config.state.n_states - 1);
            if(scanf("%d", &i)){
                if(i < config.state.n_states && i > 0){
                    char filename[1024];
                    snprintf(filename, 1023, config.state.path_format, i);
                    state_load(filename);
                }
            }
        }
    }

    signal_stop();
    midi_stop();
    audio_stop();
    output_stop();

    filters_unload();

    SDL_DestroyMutex(patterns_updating);

    if(ui_loaded){
        ui_quit();
    }
    pattern_del();

    return 0;
}

