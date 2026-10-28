#ifndef SB_MISSING_TEXTURE_H
#define SB_MISSING_TEXTURE_H

#include "core/sb_rng.h"

// error screen from INTERLOPER using the Source Engine's missing texture... texture

typedef struct {
    unsigned int display_list;
    unsigned int text_texture;

    int text_width;
    int text_height;
    char font_path[512];

    int text_visible;
    float flash_timer;
    float next_flash_interval;
} sb_missing_texture;

void sb_missing_texture_init(sb_missing_texture *scene);

// we can bake the display list here because once the 
// program enters this scene it is "crashed" so it wont go back
void sb_missing_texture_create_display_list(sb_missing_texture *scene);

void sb_missing_texture_update(sb_missing_texture *scene, float dt);

void sb_missing_texture_draw(const sb_missing_texture *scene,
                             int display_width, int display_height);

void sb_missing_texture_cleanup(sb_missing_texture *scene);

#endif /* SB_MISSING_TEXTURE_H */