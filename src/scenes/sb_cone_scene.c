#include "scenes/sb_cone_scene.h"

#include "core/sb_log.h"
#include "core/sb_platform.h"
#include "core/sb_resources.h"
#include "core/sb_rng.h"
#include "gl/glx.h"
#include "scenes/sb_cone_renderer.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *const SB_METAL_REG_WORDS[SB_METAL_REG_PHRASES] = {
    "PLAT", "SETREG", "CPU_POP", "NAN_CREG",
    "STOPREG_DIRTY", "WAIT", "THINK", "CPU_PUSH"
};

static const char *const SB_FONT_CANDIDATES[] = {
    "assets/fonts/Cabin-Regular.ttf",
    "fonts/Cabin-Regular.ttf",
    "Cabin-Regular.ttf",
    NULL
};

static float sb_random_range(float low, float high)
{
    return sb_rng_range(low, high);
}

void sb_cone_scene_init(sb_cone_scene *scene, sb_audio *audio)
{
    double yaw_rad;
    double pitch_rad;
    double distance;
    double dx;
    double dy;
    double dz;
    double length;
    size_t i;

    if (scene == NULL) {
        return;
    }

    memset(scene, 0, sizeof *scene);

    scene->is_linux = sb_platform_is_linux();
    {
        // manually set the renderer type if the user defines the env var
        const char *grid = getenv("SOURCEBOX_GRID");

        if (grid != NULL && grid[0] != '\0') {
            if (strcmp(grid, "windows") == 0) {
                scene->is_linux = 0;
            } else if (strcmp(grid, "linux") == 0) {
                scene->is_linux = 1;
            } else {
                sb_log_printf("[cone] ignoring unknown SOURCEBOX_GRID=%s", grid);
            }
        }
    }
    scene->audio = audio;

    for (i = 0; i < sizeof SB_FONT_CANDIDATES / sizeof SB_FONT_CANDIDATES[0]; ++i) {
        if (SB_FONT_CANDIDATES[i] == NULL) {
            break;
        }
        if (sb_find_resource_one(SB_FONT_CANDIDATES[i], scene->font_path,
                                 sizeof scene->font_path)) {
            break;
        }
    }

    // constant grid settings, but they themselves cant be const because it varies depending on resolution
    scene->grid_count = 15;
    scene->grid_spacing = 500.0f;
    scene->grid_texture = 0u;
    scene->texture_scale = 10.0f;

    scene->fog_color[0] = 0.0f;
    scene->fog_color[1] = 0.0f;
    scene->fog_color[2] = 0.0f;
    scene->fog_color[3] = 1.0f;
    scene->fog_start = 10.0f;
    scene->fog_end = 700.0f;

    scene->camera_pos[0] = -62.78f;
    scene->camera_pos[1] = 28.76f;
    scene->camera_pos[2] = -38.61f;
    scene->camera_rotation[0] = 17.48f;
    scene->camera_rotation[1] = 117.90f;
    scene->camera_rotation[2] = 8.88f;

    // sound effects
    scene->friend_join_timer = 0.0f;
    scene->friend_join_active = 1;
    scene->friend_join_max_duration = 8.0f;
    scene->friend_join_next_play = sb_random_range(1.0f, 1.5f);
    scene->friend_join_sound_duration = 0.0f;

    if (scene->audio != NULL) {
        scene->friend_join_sound_duration =
            (float)sb_audio_sound_duration(scene->audio, "cube_click");
        if (scene->friend_join_sound_duration == 0.0f) {
            scene->friend_join_sound_duration = 1.0f;
        }
    }

    // position target in the center and move from there
    yaw_rad = (117.90 * M_PI) / 180.0;
    pitch_rad = (17.48 * M_PI) / 180.0;

    distance = 50.0;
    scene->target_position[0] =
        (float)((double)scene->camera_pos[0] + sin(yaw_rad) * cos(pitch_rad) * distance + 10.0);
    scene->target_position[1] = scene->camera_pos[1] - 30.0f;
    scene->target_position[2] =
        (float)((double)scene->camera_pos[2] - cos(yaw_rad) * cos(pitch_rad) * distance + 10.0);

    // trace a line to the direction the target wants to move
    dx = (double)sb_random_range(-1.0f, 1.0f);
    dy = (double)sb_random_range(-1.0f, 1.0f);
    dz = (double)sb_random_range(-1.0f, 1.0f);
    length = sqrt(dx * dx + dy * dy + dz * dz);
    if (length > 0.0) {
        scene->trace_direction[0] = (float)(dx / length);
        scene->trace_direction[1] = (float)(dy / length);
        scene->trace_direction[2] = (float)(dz / length);
    } else {
        scene->trace_direction[0] = 0.0f;
        scene->trace_direction[1] = 1.0f;
        scene->trace_direction[2] = 0.0f;
    }
    scene->trace_length = sb_random_range(800.0f, 1500.0f);

    // sometimes have a blue box thingy in view
    {
        double box_distance = (double)sb_random_range(20.0f, 100.0f);
        double box_angle_h = (double)sb_random_range(0.0f, 360.0f) * M_PI / 180.0;
        double box_angle_v = (double)sb_random_range(-45.0f, 45.0f) * M_PI / 180.0;

        scene->box_position[0] = (float)((double)scene->target_position[0] +
                                         cos(box_angle_h) * cos(box_angle_v) * box_distance);
        scene->box_position[1] = (float)((double)scene->target_position[1] +
                                         sin(box_angle_v) * box_distance);
        scene->box_position[2] = (float)((double)scene->target_position[2] +
                                         sin(box_angle_h) * cos(box_angle_v) * box_distance);
    }

    scene->box_size[0] = sb_random_range(2.0f, 25.0f);
    scene->box_size[1] = sb_random_range(2.0f, 25.0f);
    scene->box_size[2] = sb_random_range(2.0f, 25.0f);

    sb_cone_scene_generate_grey_cones(scene);
    sb_cone_scene_generate_pink_triangles(scene);

    // weird red object that appears every blue moon
    scene->red_object_visible = (sb_rng_next_float() < 0.15f) ? 1 : 0;
    scene->red_object_position[0] = sb_random_range(-300.0f, 300.0f);
    scene->red_object_position[1] = sb_random_range(-50.0f, 150.0f);
    scene->red_object_position[2] = sb_random_range(-300.0f, 300.0f);
    scene->red_object_rotation = sb_random_range(0.0f, 360.0f);
    scene->red_object_pulse_timer = 0.0f;

    // triangle back button
    scene->triangle_flash = SB_TRIANGLE_NONE;
    scene->triangle_hovered = 0;
    scene->triangle_scale = 1.0f;

    // switch targets immediately after connecting
    scene->target_timer = 0.0f;
    // ... but skip the first switch to not cause jitter
    scene->target_switch_interval = 1.0f;

    // switch faster the first 9 seconds and then slow down
    scene->startup_timer = 0.0f;
    scene->startup_duration = 9.0f;
    scene->startup_complete = 0;

    // the cone randomly flashes sometimes
    scene->cone_flash_timer = 0.0f;
    scene->cone_flash_interval = sb_random_range(0.3f, 0.6f);
    scene->cone_visible = 1;

    // the camera follows the target with a bit of delay
    // the delay is because the camera moves through the void itself
    // albeit at fast speeds
    scene->camera_distance = 80.0f;
    scene->strafe_speed = 5.0f;
    scene->slow_speed = 0.8f;

    // fake loading screen
    scene->loading_timer = 0.0f;
    scene->loading_duration = 5.0f;
    scene->loading_flash_timer = 0.0f;
    scene->loading_flash_intervals[0] = 0.01f;
    scene->loading_flash_intervals[1] = 0.05f;
    scene->loading_flash_intervals[2] = 0.09f;
    scene->loading_flash_interval = scene->loading_flash_intervals[0];
    scene->loading_flash_index = 0;
    scene->loading_text_visible = 1;
    scene->loading_effect_active = 1;
    scene->stored_coords[0] = 0.0;
    scene->stored_coords[1] = 0.0;
    scene->stored_coords[2] = 0.0;
    scene->coords_initialized = 0;

    // random ass white dot movement
    scene->dot_check_timer = 0.0f;
    scene->dot_check_interval = 0.05f;
    scene->dot_offset[0] = 0.0f;
    scene->dot_offset[1] = 0.0f;
    scene->dot_offset[2] = 0.0f;
    scene->dot_is_moving = 0;
    scene->dot_move_distance = 0.0f;
    scene->dot_max_distance = 0.0f;
    scene->dot_move_speed = 50.0f;

    sb_cone_scene_generate_dot_direction(scene);

    // coordinate text generation
    for (i = 0; i < SB_METAL_REG_PHRASES; ++i) {
        scene->metal_reg_phrases[i] = SB_METAL_REG_WORDS[i];
    }
    scene->metal_reg_index = 0;
    scene->metal_reg_timer = 0.0f;
    scene->metal_reg_intervals[0] = 0.01f;
    scene->metal_reg_intervals[1] = 0.03f;
    scene->metal_reg_intervals[2] = 0.09f;
    scene->metal_reg_interval_index = 0;

    sb_cone_renderer_load_grid_texture(scene);

    sb_log_printf("[cone] scene ready (linux=%d, %d cones, %d triangles, red=%d)",
                  scene->is_linux, scene->grey_cone_count,
                  scene->pink_triangle_count, scene->red_object_visible);
}

// cover your eyes for these 2 functions. they are there because i used to do things in a different way
// and then i changed and i was having timing issues, so these 2 are basically no-ops for gcc
// as you can tell i am very good programmer /j

void sb_cone_scene_create_display_list(sb_cone_scene *scene)
{
    (void)scene;
}

void sb_cone_scene_handle_event(sb_cone_scene *scene)
{
    (void)scene;
}

void sb_cone_scene_generate_dot_direction(sb_cone_scene *scene)
{
    double direction[3];
    double length;
    int axis;

    if (scene == NULL) {
        return;
    }

    for (axis = 0; axis < 3; ++axis) {
        direction[axis] = (double)sb_random_range(-1.0f, 1.0f);
    }

    length = sqrt(direction[0] * direction[0] + direction[1] * direction[1] +
                  direction[2] * direction[2]);

    if (length > 0.0) {
        for (axis = 0; axis < 3; ++axis) {
            scene->dot_movement_direction[axis] = (float)(direction[axis] / length);
        }
    } else {
        scene->dot_movement_direction[0] = 0.0f;
        scene->dot_movement_direction[1] = 1.0f;
        scene->dot_movement_direction[2] = 0.0f;
    }
}

void sb_cone_scene_generate_grey_cones(sb_cone_scene *scene)
{
    int count;
    int i;

    if (scene == NULL) {
        return;
    }

    scene->grey_cone_count = 0;

    // how many cones are there is rng dependant
    count = 8 + sb_rng_below(13);
    if (count > SB_MAX_GREY_CONES) {
        count = SB_MAX_GREY_CONES;
    }

    for (i = 0; i < count; ++i) {
        float position[3];
        position[0] = sb_random_range(-400.0f, 400.0f);
        position[1] = sb_random_range(-100.0f, 200.0f);
        position[2] = sb_random_range(-400.0f, 400.0f);
        sb_grey_cone_init(&scene->grey_cones[i], position);
    }

    scene->grey_cone_count = count;
}

void sb_cone_scene_generate_pink_triangles(sb_cone_scene *scene)
{
    double yaw_rad;
    double forward_x;
    double forward_z;
    double dist;
    float position[3];
    float size;
    double dx;
    double dy;
    double dz;
    double length;
    double speed;
    float velocity[3];
    sb_pink_triangle *triangle;

    if (scene == NULL) {
        return;
    }

    scene->pink_triangle_count = 0;

    // very fucking rare pink triangles that totally werent a pain to test
    if (sb_rng_next_float() > 0.20f) {
        return;
    }

    yaw_rad = ((double)scene->camera_rotation[1] * M_PI) / 180.0;
    forward_x = sin(yaw_rad);
    forward_z = -cos(yaw_rad);
    dist = (double)sb_random_range(30.0f, 60.0f);

    position[0] = (float)((double)scene->camera_pos[0] + forward_x * dist +
                          (double)sb_random_range(-20.0f, 20.0f));
    position[1] = (float)((double)scene->camera_pos[1] +
                          (double)sb_random_range(-10.0f, 10.0f));
    position[2] = (float)((double)scene->camera_pos[2] + forward_z * dist +
                          (double)sb_random_range(-20.0f, 20.0f));

    size = sb_random_range(2.0f, 4.0f);

    dx = (double)sb_random_range(-1.0f, 1.0f);
    dy = (double)sb_random_range(-0.3f, 0.3f);
    dz = (double)sb_random_range(-1.0f, 1.0f);
    length = sqrt(dx * dx + dy * dy + dz * dz);

    if (length > 0.0) {
        speed = (double)sb_random_range(3.0f, 6.0f);
        velocity[0] = (float)(dx / length * speed);
        velocity[1] = (float)(dy / length * speed);
        velocity[2] = (float)(dz / length * speed);
    } else {
        velocity[0] = 3.0f;
        velocity[1] = 0.0f;
        velocity[2] = 0.0f;
    }

    if (scene->pink_triangle_count >= SB_MAX_PINK_TRIANGLES) {
        return;
    }

    triangle = &scene->pink_triangles[scene->pink_triangle_count];
    triangle->position[0] = position[0];
    triangle->position[1] = position[1];
    triangle->position[2] = position[2];
    triangle->size = size;
    triangle->velocity[0] = velocity[0];
    triangle->velocity[1] = velocity[1];
    triangle->velocity[2] = velocity[2];

    scene->pink_triangle_count += 1;
}

// sometimes hold the same position instead of constantly switching
static float sb_cone_next_teleport_interval(void)
{
    if (sb_rng_next_float() < 0.02f) {
        return sb_random_range(0.08f, 0.4f);
    }
    return sb_random_range(0.7f, 4.0f);
}

void sb_cone_scene_generate_random_target(sb_cone_scene *scene)
{
    double dx;
    double dy;
    double dz;
    double length;
    double box_distance;
    double box_angle_h;
    double box_angle_v;

    if (scene == NULL) {
        return;
    }

    scene->target_position[0] = sb_random_range(-500.0f, 500.0f);
    scene->target_position[1] = sb_random_range(-200.0f, 200.0f);
    scene->target_position[2] = sb_random_range(-500.0f, 500.0f);

    dx = (double)sb_random_range(-1.0f, 1.0f);
    dy = (double)sb_random_range(-1.0f, 1.0f);
    dz = (double)sb_random_range(-1.0f, 1.0f);
    length = sqrt(dx * dx + dy * dy + dz * dz);

    if (length > 0.0) {
        scene->trace_direction[0] = (float)(dx / length);
        scene->trace_direction[1] = (float)(dy / length);
        scene->trace_direction[2] = (float)(dz / length);
    } else {
        scene->trace_direction[0] = 0.0f;
        scene->trace_direction[1] = 1.0f;
        scene->trace_direction[2] = 0.0f;
    }

    scene->trace_length = sb_random_range(800.0f, 1500.0f);

    // the blue box should be near the target so it is also in camera view
    box_distance = (double)sb_random_range(20.0f, 100.0f);
    box_angle_h = (double)sb_random_range(0.0f, 360.0f) * M_PI / 180.0;
    box_angle_v = (double)sb_random_range(-45.0f, 45.0f) * M_PI / 180.0;

    scene->box_position[0] = (float)((double)scene->target_position[0] +
                                     cos(box_angle_h) * cos(box_angle_v) * box_distance);
    scene->box_position[1] = (float)((double)scene->target_position[1] +
                                     sin(box_angle_v) * box_distance);
    scene->box_position[2] = (float)((double)scene->target_position[2] +
                                     sin(box_angle_h) * cos(box_angle_v) * box_distance);

    scene->box_size[0] = sb_random_range(2.0f, 25.0f);
    scene->box_size[1] = sb_random_range(2.0f, 25.0f);
    scene->box_size[2] = sb_random_range(2.0f, 25.0f);

    sb_cone_scene_generate_grey_cones(scene);
    sb_cone_scene_generate_dot_direction(scene);

    scene->dot_offset[0] = 0.0f;
    scene->dot_offset[1] = 0.0f;
    scene->dot_offset[2] = 0.0f;
    scene->dot_is_moving = 0;
    scene->dot_move_distance = 0.0f;

    sb_cone_scene_update_stored_coords(scene);
}

void sb_cone_scene_update_stored_coords(sb_cone_scene *scene)
{
    double dot_pos[3];
    const double scale_factor = 15000.0;
    const double base_offset = 50000.0;
    int axis;

    if (scene == NULL) {
        return;
    }

    for (axis = 0; axis < 3; ++axis) {
        dot_pos[axis] = (double)scene->target_position[axis] + (double)scene->dot_offset[axis];
    }

    scene->stored_coords[0] = fabs(dot_pos[0] * scale_factor) + base_offset;
    scene->stored_coords[1] = fabs(dot_pos[1] * scale_factor) + base_offset * 2.0;
    scene->stored_coords[2] = fabs(dot_pos[2] * scale_factor) + base_offset * 1.5;
}

void sb_cone_scene_teleport_to_target(sb_cone_scene *scene)
{
    float roll;
    double angle_around_rad;
    double offset_x;
    double offset_z;
    double dx;
    double dy;
    double dz;
    double yaw;
    double horizontal_dist;
    double pitch;

    if (scene == NULL) {
        return;
    }

    roll = sb_random_range(-15.0f, 15.0f);
    angle_around_rad = (double)sb_random_range(0.0f, 360.0f) * M_PI / 180.0;

    offset_x = sin(angle_around_rad) * (double)scene->camera_distance;
    offset_z = cos(angle_around_rad) * (double)scene->camera_distance;

    scene->camera_pos[0] = (float)((double)scene->target_position[0] + offset_x);
    scene->camera_pos[1] = scene->target_position[1];
    scene->camera_pos[2] = (float)((double)scene->target_position[2] + offset_z);

    dx = (double)scene->target_position[0] - (double)scene->camera_pos[0];
    dy = (double)scene->target_position[1] - (double)scene->camera_pos[1];
    dz = (double)scene->target_position[2] - (double)scene->camera_pos[2];

    yaw = atan2(dx, -dz) * 180.0 / M_PI;
    horizontal_dist = sqrt(dx * dx + dz * dz);
    pitch = atan2(dy, horizontal_dist) * 180.0 / M_PI;

    scene->camera_rotation[0] = (float)pitch;
    scene->camera_rotation[1] = (float)yaw;
    scene->camera_rotation[2] = roll;
}

// keep the target centered on screen
void sb_cone_scene_center_target_horizontally(sb_cone_scene *scene)
{
    double dx;
    double dz;

    if (scene == NULL) {
        return;
    }

    dx = (double)scene->target_position[0] - (double)scene->camera_pos[0];
    dz = (double)scene->target_position[2] - (double)scene->camera_pos[2];

    if (fabs(dx) > 0.01 || fabs(dz) > 0.01) {
        scene->camera_rotation[1] = (float)(atan2(dx, -dz) * 180.0 / M_PI);
    }
}

void sb_cone_scene_update(sb_cone_scene *scene, float dt)
{
    double yaw_rad;
    double right_x;
    double right_z;
    int i;

    if (scene == NULL) {
        return;
    }

    // random sound effects playing in the void
    if (scene->friend_join_active && scene->audio != NULL) {
        scene->friend_join_timer += dt;

        if (scene->friend_join_timer >= scene->friend_join_max_duration) {
            scene->friend_join_active = 0;
        } else if (scene->friend_join_timer >= scene->friend_join_next_play) {
            sb_audio_play_sound(scene->audio, "cube_click");
            {
                float delay = sb_random_range(3.0f, 5.0f);
                scene->friend_join_next_play =
                    scene->friend_join_timer + scene->friend_join_sound_duration + delay;
            }
        }
    }

    // while the fake loading screen is up we can actually set up some things
    if (scene->loading_effect_active) {
        scene->loading_timer += dt;
        if (scene->loading_timer >= scene->loading_duration) {
            scene->loading_effect_active = 0;
            if (!scene->coords_initialized) {
                sb_cone_scene_update_stored_coords(scene);
                scene->coords_initialized = 1;
            }
        } else {
            scene->loading_flash_timer += dt;
            if (scene->loading_flash_timer >= scene->loading_flash_interval) {
                scene->loading_flash_timer = 0.0f;
                scene->loading_text_visible = !scene->loading_text_visible;
                scene->loading_flash_index =
                    (scene->loading_flash_index + 1) % 3;
                scene->loading_flash_interval =
                    scene->loading_flash_intervals[scene->loading_flash_index];
            }
        }
    }

    // if somehow we reached the integer limit snap back to safe coords
    // this took like an hour to test T-T
    if (scene->dot_is_moving) {
        float move_amount = scene->dot_move_speed * dt;

        scene->dot_offset[0] += scene->dot_movement_direction[0] * move_amount;
        scene->dot_offset[1] += scene->dot_movement_direction[1] * move_amount;
        scene->dot_offset[2] += scene->dot_movement_direction[2] * move_amount;
        scene->dot_move_distance += move_amount;

        if (scene->dot_move_distance >= scene->dot_max_distance) {
            scene->dot_offset[0] = 0.0f;
            scene->dot_offset[1] = 0.0f;
            scene->dot_offset[2] = 0.0f;
            scene->dot_is_moving = 0;
            scene->dot_move_distance = 0.0f;
        }
    } else {
        scene->dot_check_timer += dt;
        if (scene->dot_check_timer >= scene->dot_check_interval) {
            scene->dot_check_timer = 0.0f;

            if (sb_rng_next_float() < 0.015f) {
                scene->dot_is_moving = 1;
                scene->dot_move_distance = 0.0f;
                scene->dot_max_distance = sb_random_range(2.0f, 15.0f);
                scene->dot_offset[0] = 0.0f;
                scene->dot_offset[1] = 0.0f;
                scene->dot_offset[2] = 0.0f;
            }
        }
    }

    // cycle through the metal_reg text
    if (!scene->loading_effect_active) {
        scene->metal_reg_timer += dt;
        if (scene->metal_reg_timer >= scene->metal_reg_intervals[scene->metal_reg_interval_index]) {
            scene->metal_reg_timer = 0.0f;
            scene->metal_reg_index =
                (scene->metal_reg_index + 1) % SB_METAL_REG_PHRASES;
            scene->metal_reg_interval_index =
                (scene->metal_reg_interval_index + 1) % 3;
        }
    }

    // set up the teleporting after loading complete
    if (!scene->startup_complete) {
        scene->startup_timer += dt;
        if (scene->startup_timer >= scene->startup_duration) {
            scene->startup_complete = 1;
            scene->strafe_speed = scene->slow_speed;
        }
    }

    // random cone flashes
    scene->cone_flash_timer += dt;
    if (scene->cone_flash_timer >= scene->cone_flash_interval) {
        scene->cone_flash_timer = 0.0f;
        scene->cone_flash_interval = sb_random_range(0.3f, 0.6f);
        scene->cone_visible = !scene->cone_visible;
    }

    for (i = 0; i < scene->grey_cone_count; ++i) {
        sb_grey_cone_update(&scene->grey_cones[i], dt);
    }

    // the pink triangles move independently from the camera
    for (i = 0; i < scene->pink_triangle_count; ++i) {
        sb_pink_triangle *triangle = &scene->pink_triangles[i];
        triangle->position[0] += triangle->velocity[0] * dt;
        triangle->position[1] += triangle->velocity[1] * dt;
        triangle->position[2] += triangle->velocity[2] * dt;
    }

    // pulse from the red object
    if (scene->red_object_visible) {
        scene->red_object_pulse_timer += dt;
    }

    // delay when switching target positions or angle
    scene->target_timer += dt;
    if (scene->target_timer >= scene->target_switch_interval) {
        scene->target_timer = 0.0f;
        scene->target_switch_interval = sb_cone_next_teleport_interval();

        sb_cone_scene_generate_random_target(scene);
        sb_cone_scene_teleport_to_target(scene);
    }

    // the camera moves through the void like you would strafe in doom engine games or the like, aka fast as fuck
    yaw_rad = ((double)scene->camera_rotation[1] * M_PI) / 180.0;
    right_x = cos(yaw_rad);
    right_z = sin(yaw_rad);

    // do not bob the camera up and down
    // can you tell i based this off of another game's code?
    scene->camera_pos[0] -= (float)(right_x * (double)scene->strafe_speed * (double)dt);
    scene->camera_pos[2] -= (float)(right_z * (double)scene->strafe_speed * (double)dt);
    sb_cone_scene_center_target_horizontally(scene);
}

int sb_cone_scene_check_triangle_hover(sb_cone_scene *scene,
                                       int mouse_x, int mouse_y,
                                       int display_width, int display_height)
{
    double center_x = (double)display_width * 0.85;
    double center_y = (double)display_height * 0.60;
    double size = 50.0 * 0.5;
    double dx = (double)mouse_x - center_x;
    double dy = (double)mouse_y - center_y;
    double distance;

    if (scene == NULL) {
        return 0;
    }

    distance = sqrt(dx * dx + dy * dy);

    if (distance < size * 2.0) {
        if (!scene->triangle_hovered) {
            if (scene->audio != NULL) {
                sb_audio_play_sound(scene->audio, "hover");
            }
        }
        scene->triangle_hovered = 1;
        return 1;
    }

    scene->triangle_hovered = 0;
    return 0;
}

int sb_cone_scene_check_triangle_click(const sb_cone_scene *scene,
                                       int mouse_x, int mouse_y,
                                       int display_width, int display_height)
{
    double center_x;
    double center_y;
    double size;
    double dx;
    double dy;
    double distance;

    if (scene == NULL) {
        return 0;
    }

    center_x = (double)display_width * 0.85;
    center_y = (double)display_height * 0.60;
    size = 50.0 * 0.5 * (double)scene->triangle_scale;

    dx = (double)mouse_x - center_x;
    dy = (double)mouse_y - center_y;
    distance = sqrt(dx * dx + dy * dy);

    return (distance < size * 2.0) ? 1 : 0;
}

void sb_cone_scene_cleanup(sb_cone_scene *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->grid_texture != 0u) {
        glDeleteTextures(1, &scene->grid_texture);
        scene->grid_texture = 0u;
    }
}