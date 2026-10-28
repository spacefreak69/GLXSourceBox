#ifndef SB_MAIN_SCENE_H
#define SB_MAIN_SCENE_H

// functions that draw the main menu cube, sphere and cone on the fly

#include <stddef.h>

typedef enum {
    SB_OBJ_CUBE = 0,
    SB_OBJ_SPHERE,
    SB_OBJ_CONE
} sb_obj_type;

typedef struct {
    sb_obj_type type;
    char name[16];

    float position[3];
    float base_rotation[3];
    float scale;
    float base_scale;
    float scale_xyz[3];
    float brightness;

    float rotation_angle;
    float hover_timer;
    float hover_animation_duration;
    float hover_scale_amount;

    int is_hovered;
    int is_rotating;
    int was_hovered;

    float bounding_radius;

    unsigned int display_list;
} sb_object;

void sb_object_init(sb_object *object, sb_obj_type type, const char *name,
                    const float position[3], const float rotation[3],
                    float scale, const float scale_xyz[3], float brightness);

// create the display list once as the objects dont have any animations 
// (beside basic transformations)
void sb_object_create_display_list(sb_object *object);

void sb_object_cleanup(sb_object *object);

// update cube rotation
void sb_object_update_rotation(sb_object *object, float dt);

// update the animation, so it looks like they grow a bit when highlighted
void sb_object_update_animation(sb_object *object, float dt);

// finally draw the object using fixed pipeline
void sb_object_draw(const sb_object *object);

// draws the object using red instead of gray as main color, 
// also does highlight animation (update_animation)
void sb_object_draw_flash(const sb_object *object, int is_red);

/* ------------------------------------------------------------- environment -- */

typedef struct {
    float position[3];
    float rotation[3];
} sb_camera;

typedef struct {
    float position[4];
    float ambient[4];
    float diffuse[4];
    float specular[4];
    int setup_done;
} sb_light;

typedef struct {
    int size;
    float position[3];
    float rotation[3];
    float scale[3];
    float dark_color[3];
    float light_color[3];
    float brightness;

    unsigned int display_list;
    unsigned int texture;
} sb_checkerboard;

void sb_camera_init(sb_camera *camera);
void sb_camera_apply(const sb_camera *camera);

void sb_light_init(sb_light *light);
void sb_light_apply(sb_light *light);

void sb_checkerboard_init(sb_checkerboard *board);
void sb_checkerboard_create_display_list(sb_checkerboard *board);
void sb_checkerboard_draw(const sb_checkerboard *board);
void sb_checkerboard_cleanup(sb_checkerboard *board);

// get the object that is getting hovered, and also play the sound
int sb_pick_hovered(sb_object *objects, size_t count,
                    const double origin[3], const double direction[3],
                    int *hover_sound_requested);

// just get the nearest object to mouse when clicking, play selection sound
int sb_pick_clicked(const sb_object *objects, size_t count,
                    const double origin[3], const double direction[3]);

#endif /* SB_MAIN_SCENE_H */