#ifndef SB_CONE_SCENE_H
#define SB_CONE_SCENE_H

#include "audio/sb_audio.h"
#include "scenes/sb_cone_entities.h"

// the other half of the renderer.
// i ended up splitting them because 1k lines is already big enough for a file
// and also it already took too long to compile that unit

#define SB_MAX_GREY_CONES 20
#define SB_MAX_PINK_TRIANGLES 1
#define SB_METAL_REG_PHRASES 8

typedef struct {
    float position[3];
    float size;
    float velocity[3];
} sb_pink_triangle;

typedef enum {
    SB_TRIANGLE_NONE = 0,
    SB_TRIANGLE_RED,
    SB_TRIANGLE_GREY
} sb_triangle_flash;

typedef struct {
    int is_linux;
    sb_audio *audio;

    char font_path[512];

    // constant not const grid settings
    int grid_count;
    float grid_spacing;
    unsigned int grid_texture;
    float texture_scale;

    float fog_color[4];
    float fog_start;
    float fog_end;

    float camera_pos[3];
    float camera_rotation[3];

    // sound effect system
    float friend_join_timer;
    int friend_join_active;
    float friend_join_max_duration;
    float friend_join_next_play;
    float friend_join_sound_duration;

    float target_position[3];
    float trace_direction[3];
    float trace_length;

    float box_position[3];
    float box_size[3];

    sb_grey_cone grey_cones[SB_MAX_GREY_CONES];
    int grey_cone_count;

    sb_pink_triangle pink_triangles[SB_MAX_PINK_TRIANGLES];
    int pink_triangle_count;

    int red_object_visible;
    float red_object_position[3];
    float red_object_rotation;
    float red_object_pulse_timer;

    // the triangle detection for the back button is a simple area check, not raycast
    sb_triangle_flash triangle_flash;
    int triangle_hovered;
    float triangle_scale;

    // switching target position
    float target_timer;
    float target_switch_interval;

    // loading setting up, dont switch for 9 seconds
    float startup_timer;
    float startup_duration;
    int startup_complete;

    // random cone flashes
    float cone_flash_timer;
    float cone_flash_interval;
    int cone_visible;

    // camera settings
    float camera_distance;
    float strafe_speed;
    float slow_speed;

    // fake loading screen
    float loading_timer;
    float loading_duration;
    float loading_flash_timer;
    float loading_flash_interval;
    float loading_flash_intervals[3];
    int loading_flash_index;
    int loading_text_visible;
    int loading_effect_active;

    double stored_coords[3];
    int coords_initialized;

    // random white dot movement
    float dot_check_timer;
    float dot_check_interval;
    float dot_offset[3];
    float dot_movement_direction[3];
    int dot_is_moving;
    float dot_move_distance;
    float dot_max_distance;
    float dot_move_speed;

    // coordinate text generation
    const char *metal_reg_phrases[SB_METAL_REG_PHRASES];
    int metal_reg_index;
    float metal_reg_timer;
    float metal_reg_intervals[3];
    int metal_reg_interval_index;
} sb_cone_scene;

void sb_cone_scene_init(sb_cone_scene *scene, sb_audio *audio);

// NO OPs
void sb_cone_scene_create_display_list(sb_cone_scene *scene);
void sb_cone_scene_update(sb_cone_scene *scene, float dt);

// handle generating the correct geometry when loading the voidsite
void sb_cone_scene_generate_dot_direction(sb_cone_scene *scene);
void sb_cone_scene_generate_grey_cones(sb_cone_scene *scene);
void sb_cone_scene_generate_pink_triangles(sb_cone_scene *scene);
void sb_cone_scene_generate_random_target(sb_cone_scene *scene);
void sb_cone_scene_update_stored_coords(sb_cone_scene *scene);

// move the camera into position and center target
void sb_cone_scene_teleport_to_target(sb_cone_scene *scene);

// angle the camera in a way that the target is centered but also the void is visible
void sb_cone_scene_center_target_horizontally(sb_cone_scene *scene);

// yet another NO OP
void sb_cone_scene_handle_event(sb_cone_scene *scene);

int sb_cone_scene_check_triangle_hover(sb_cone_scene *scene,
                                       int mouse_x, int mouse_y,
                                       int display_width, int display_height);

int sb_cone_scene_check_triangle_click(const sb_cone_scene *scene,
                                       int mouse_x, int mouse_y,
                                       int display_width, int display_height);

void sb_cone_scene_cleanup(sb_cone_scene *scene);

#endif /* SB_CONE_SCENE_H */