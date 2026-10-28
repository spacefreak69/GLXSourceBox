// main entry point of GLXSourceBox

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include "audio/sb_audio.h"
#include "core/sb_log.h"
#include "core/sb_platform.h"
#include "core/sb_resources.h"
#include "core/sb_rng.h"
#include "gl/glx.h"
#include "gl/glglu.h"
#include "gl/sb_text.h"
#include "render/sb_cursor.h"
#include "render/sb_main_scene.h"
#include "render/sb_raycast.h"
#include "scenes/sb_cone_renderer.h"
#include "scenes/sb_cone_scene.h"
#include "scenes/sb_missing_texture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SB_FPS 60
#define SB_FRAME_MS (1000 / SB_FPS)
#define SB_MAX_DT 0.1f

#define SB_CONE_WINDOW_WIDTH 548
#define SB_CONE_WINDOW_HEIGHT 525

typedef enum {
    SB_SCENE_MAIN = 0,
    SB_SCENE_ERROR,
    SB_SCENE_CONE
} sb_scene_id;

static const char *const SB_ICON_CANDIDATES[] = {
    "assets/images/sourcebox.png",
    "assets/images/icon.png",
    "assets/images/icon.ico",
    "sourcebox.png",
    "icon.png",
    NULL
};

typedef struct {
    SDL_Window *window;
    SDL_GLContext context;
    int width;
    int height;
    int drawable_width;
    int drawable_height;
} sb_app;

static void sb_app_log_gl_error(const char *where)
{
    GLenum error = glGetError();

    while (error != GL_NO_ERROR) {
        sb_log_printf("[gl] error 0x%04x after %s", (unsigned)error, where);
        error = glGetError();
    }
}

// function to sync up world space with screen resolution, for mouse raycasting
static void sb_app_sync_drawable_size(sb_app *app)
{
    int drawable_width = 0;
    int drawable_height = 0;

    if (app == NULL || app->window == NULL) {
        return;
    }

    SDL_GetWindowSize(app->window, &app->width, &app->height);
    SDL_GL_GetDrawableSize(app->window, &drawable_width, &drawable_height);

    if (drawable_width > 0 && drawable_height > 0) {
        app->drawable_width = drawable_width;
        app->drawable_height = drawable_height;
        glViewport(0, 0, drawable_width, drawable_height);
    }
}

static void sb_app_center_window(sb_app *app, int width, int height)
{
    sb_size desktop;
    int screen_width = 1920;
    int screen_height = 1080;

    if (sb_platform_desktop_size(&desktop)) {
        screen_width = desktop.width;
        screen_height = desktop.height;
    }

    SDL_SetWindowPosition(app->window, (screen_width - width) / 2,
                          (screen_height - height) / 2);
}

// choose best resolution for the monitor
static void sb_app_choose_size(sb_size desktop, int *out_width, int *out_height)
{
    if (desktop.width <= 1366 || desktop.height <= 768) {
        *out_width = (int)((double)desktop.width * 0.8);
        *out_height = (int)((double)desktop.height * 0.8);
    } else if (desktop.width <= 1920 || desktop.height <= 1080) {
        *out_width = 1280;
        *out_height = 720;
    } else {
        *out_width = 1600;
        *out_height = 900;
    }
}

static int sb_app_set_icon(sb_app *app)
{
    char path[512];
    const char *const *candidate;
    SDL_Surface *surface = NULL;

    for (candidate = SB_ICON_CANDIDATES; *candidate != NULL; ++candidate) {
        if (sb_find_resource_one(*candidate, path, sizeof path)) {
            surface = IMG_Load(path);
            if (surface != NULL) {
                SDL_SetWindowIcon(app->window, surface);
                SDL_FreeSurface(surface);
                sb_log_printf("[icon] loaded %s", path);
                return 1;
            }
        }
    }

    sb_log_printf("[icon] none of the candidates resolved");
    return 0;
}

static int sb_app_init_gl(sb_app *app, int width, int height)
{
    SDL_WindowFlags window_flags;

    if (!SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) ||
        !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24) ||
        !SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8) ||
        !SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8) ||
        !SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8) ||
        !SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8)) {
        sb_log_printf("[gl] SDL_GL_SetAttribute failed: %s", SDL_GetError());
    }

    // we intentionally cause an SDL error to keep the default compatibility profile
    // SDL3 added a way to do this better, but i was too deep to rewrite the SDL code to SDL3
    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                             SDL_GL_CONTEXT_PROFILE_COMPATIBILITY)) {
        sb_log_printf("[gl] compatibility profile unavailable: %s", SDL_GetError());
    }

    window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;

    app->window = SDL_CreateWindow("SourceBox", SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED, width, height,
                                   window_flags);
    if (app->window == NULL) {
        sb_log_printf("[gl] SDL_CreateWindow failed: %s", SDL_GetError());
        return 0;
    }

    app->context = SDL_GL_CreateContext(app->window);
    if (app->context == NULL) {
        sb_log_printf("[gl] SDL_GL_CreateContext failed: %s", SDL_GetError());
        return 0;
    }

    SDL_GL_MakeCurrent(app->window, app->context);

    // try setting VSync, there are fallbacks through the code but it isnt intended to run without VSync
    if (SDL_GL_SetSwapInterval(1) != 0) {
        sb_log_printf("[gl] swap interval unavailable: %s", SDL_GetError());
    }

    glewExperimental = GL_TRUE;
    {
        GLenum glew_status = glewInit();
        if (glew_status != GLEW_OK) {
            sb_log_printf("[gl] glewInit failed: %s",
                          (const char *)glewGetErrorString(glew_status));
            return 0;
        }
    }
    // activating experimental glew features causes warnings on some drivers, so ignore those 
    while (glGetError() != GL_NO_ERROR) {
        // drain the error buffer, ignore those driver dependant errors
    }

    // log the gl driver information, was very useful for debugging :)
    sb_log_printf("[gl] vendor   : %s",
                  (const char *)glGetString(GL_VENDOR));
    sb_log_printf("[gl] renderer : %s",
                  (const char *)glGetString(GL_RENDERER));
    sb_log_printf("[gl] version  : %s",
                  (const char *)glGetString(GL_VERSION));

    sb_app_sync_drawable_size(app);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(53.25, (double)width / (double)height, 0.1, 50.0);
    glMatrixMode(GL_MODELVIEW);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    sb_log_printf("[gl] context ready, %dx%d window / %dx%d drawable",
                  app->width, app->height, app->drawable_width, app->drawable_height);
    return 1;
}

// cone flashing when entering and exiting voidsite
static void sb_app_flash_cone_object(sb_app *app, sb_audio *audio,
                                     sb_cursor *cursor,
                                     sb_camera *camera, sb_light *light,
                                     sb_checkerboard *board,
                                     sb_object *objects, size_t object_count,
                                     sb_object *clicked, int mouse_x, int mouse_y)
{
    static const int flash_pattern[5] = { 1, 0, 1, 0, 1 };
    double cone_duration;
    int remaining;
    int step;
    size_t i;

    sb_audio_play_sound(audio, "cone_click");

    //it isnt affected by hovering so it doesnt break the effect
    clicked->is_hovered = 0;

    for (step = 0; step < 5; ++step) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        sb_camera_apply(camera);
        sb_light_apply(light);
        sb_checkerboard_draw(board);
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        glEnable(GL_LIGHTING);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

        for (i = 0; i < object_count; ++i) {
            if (&objects[i] == clicked) {
                sb_object_draw_flash(clicked, flash_pattern[step]);
            } else {
                sb_object_draw(&objects[i]);
            }
        }

        sb_cursor_draw(cursor, mouse_x, mouse_y, app->width, app->height);
        SDL_GL_SwapWindow(app->window);
        SDL_Delay(80);
    }

    cone_duration = sb_audio_sound_duration(audio, "cone_click");
    remaining = (cone_duration > 0.0)
                    ? (int)(cone_duration * 1000.0) - 400
                    : 100;
    if (remaining > 0) {
        SDL_Delay((Uint32)remaining);
    }
}

// same thing but for voidsite triangle
static void sb_app_flash_triangle(sb_app *app, sb_audio *audio,
                                  sb_cursor *cursor, sb_cone_scene *cone,
                                  int mouse_x, int mouse_y)
{
    static const sb_triangle_flash flash_pattern[5] = {
        SB_TRIANGLE_RED, SB_TRIANGLE_GREY, SB_TRIANGLE_RED,
        SB_TRIANGLE_GREY, SB_TRIANGLE_RED
    };
    int step;

    sb_audio_play_sound(audio, "cone_back");

    //it isnt affected by hovering so it doesnt break the effect
    cone->triangle_hovered = 0;

    for (step = 0; step < 5; ++step) {
        cone->triangle_flash = flash_pattern[step];
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        sb_cone_renderer_draw(cone, app->width, app->height);
        sb_cursor_draw(cursor, mouse_x, mouse_y, app->width, app->height);
        SDL_GL_SwapWindow(app->window);
        SDL_Delay(80);
    }

    cone->triangle_flash = SB_TRIANGLE_NONE;

    // can only enter or exit voidsite every 3 seconds
    SDL_Delay(2600);
}

static void sb_app_enter_cone(sb_app *app, sb_audio *audio)
{
    sb_app_center_window(app, SB_CONE_WINDOW_WIDTH, SB_CONE_WINDOW_HEIGHT);
    SDL_SetWindowSize(app->window, SB_CONE_WINDOW_WIDTH, SB_CONE_WINDOW_HEIGHT);
    sb_app_sync_drawable_size(app);

    sb_audio_play_music(audio, -1, 0.3f, 1.0f);
}

static void sb_app_leave_cone(sb_app *app, sb_audio *audio, sb_cursor *cursor,
                              int original_width, int original_height)
{
    sb_app_center_window(app, original_width, original_height);
    SDL_SetWindowSize(app->window, original_width, original_height);
    sb_app_sync_drawable_size(app);

    // restore the projection mode when exiting out of voidsite
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(53.25, (double)original_width / (double)original_height, 0.1, 50.0);
    glMatrixMode(GL_MODELVIEW);

    // cursor is hidden when switching, so remember to switch it back on after transition
    cursor->enabled = 1;
    SDL_ShowCursor(SDL_DISABLE);

    // restart the music
    sb_audio_stop_music(audio);
    sb_audio_play_music(audio, -1, 0.3f, 1.0f);
}

int main(int argc, char **argv)
{
    sb_app app;
    sb_size desktop;
    sb_audio *audio;
    sb_cursor cursor;
    sb_raycast raycast;
    sb_object objects[3];
    sb_camera camera;
    sb_light light;
    sb_checkerboard board;
    sb_missing_texture missing_texture;
    sb_cone_scene cone_scene;
    sb_scene_id current_scene = SB_SCENE_MAIN;
    int original_width;
    int original_height;
    int running = 1;
    Uint32 previous_tick;
    size_t i;
    int seed_index;

    sb_platform_init();
    sb_log_init();
    sb_log_printf("SourceBox (C) starting");
    sb_log_printf("Running on: %s", sb_platform_name());
    sb_log_printf("Resources root: %s", sb_resource_root());

    // force seed passed to --seed argument or stored in the env var
    sb_rng_seed_from_clock();
    seed_index = sb_rng_apply_command_line(argc, argv);
    sb_log_printf("RNG seed: %llu (%s)",
                  (unsigned long long)sb_rng_current_seed(),
                  (seed_index > 0) ? "from --seed"
                          : (getenv("SOURCEBOX_SEED") != NULL) ? "from $SOURCEBOX_SEED"
                                                               : "from the clock");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        sb_log_printf("[sdl] SDL_Init failed: %s", SDL_GetError());
        sb_log_close();
        return 1;
    }

    if (TTF_Init() != 0) {
        sb_log_printf("[ttf] TTF_Init failed: %s", TTF_GetError());
    }

    if (IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) == 0) {
        sb_log_printf("[image] IMG_Init incomplete: %s", IMG_GetError());
    }

    memset(&app, 0, sizeof app);

    desktop.width = 1920;
    desktop.height = 1080;
    if (!sb_platform_desktop_size(&desktop)) {
        sb_log_printf("[platform] desktop size unknown; assuming 1920x1080");
    }
    sb_log_printf("Detected screen resolution: %dx%d", desktop.width, desktop.height);

    sb_app_choose_size(desktop, &app.width, &app.height);
    sb_log_printf("Using display resolution: %dx%d", app.width, app.height);

    if (!sb_app_init_gl(&app, app.width, app.height)) {
        SDL_Quit();
        sb_log_close();
        return 1;
    }
    (void)sb_app_set_icon(&app);

    original_width = app.width;
    original_height = app.height;
    sb_log_printf("Display scale factor: %.2f",
                  (double)sb_platform_display_scale(app.width, app.height));

    audio = sb_audio_create();
    sb_audio_load_sound(audio, "hover", "assets/sounds/click.wav");
    sb_audio_load_sound(audio, "cube_click", "assets/sounds/friend_join.wav");
    sb_audio_load_sound(audio, "cone_click", "assets/sounds/cone.wav");
    sb_audio_load_sound(audio, "cone_back", "assets/sounds/coneback.wav");
    sb_audio_load_music(audio, "assets/sounds/sourcebox.dll.mp3");
    // the official OST version of sourcebox.dll starts 2 seconds in
    // i could skip the first 2 seconds, but it takes so long that after 
    // doing that it starts with 5 seconds of delay or more, so i will gladly take the 2 seconds :)
    
    sb_audio_play_music(audio, -1, 0.3f, 0.0f);

    memset(&cursor, 0, sizeof cursor);
    sb_cursor_load(&cursor, "assets/images/cursor.png");
    sb_cursor_set_scale(&cursor, sb_platform_display_scale(app.width, app.height));
    if (cursor.enabled) {
        SDL_ShowCursor(SDL_DISABLE);
    } else {
        SDL_ShowCursor(SDL_ENABLE);
    }

    {
        static const float cube_position[3] = { -1.21f, 2.11f, -1.14f };
        static const float cube_rotation[3] = { 1467.99f, -1441.71f, 27.87f };
        static const float cube_scale_xyz[3] = { 1.09f, 0.99f, 1.04f };
        static const float sphere_position[3] = { 0.03f, 2.68f, -1.03f };
        static const float sphere_rotation[3] = { -269.60f, -18.40f, 0.00f };
        static const float sphere_scale_xyz[3] = { 0.96f, 0.97f, 0.98f };
        static const float cone_position[3] = { 6.29f, 2.49f, 1.49f };
        static const float cone_rotation[3] = { 157.67f, 19.35f, 335.96f };
        static const float cone_scale_xyz[3] = { 0.73f, 1.51f, 1.11f };

        sb_object_init(&objects[0], SB_OBJ_CUBE, "cube", cube_position,
                       cube_rotation, 1.22f, cube_scale_xyz, 0.7f);
        sb_object_init(&objects[1], SB_OBJ_SPHERE, "sphere", sphere_position,
                       sphere_rotation, 1.69f, sphere_scale_xyz, 0.7f);
        sb_object_init(&objects[2], SB_OBJ_CONE, "cone", cone_position,
                       cone_rotation, 1.06f, cone_scale_xyz, 0.7f);
    }

    sb_camera_init(&camera);
    sb_light_init(&light);
    sb_checkerboard_init(&board);
    memset(&raycast, 0, sizeof raycast);

    sb_missing_texture_init(&missing_texture);
    sb_cone_scene_init(&cone_scene, audio);

    sb_checkerboard_create_display_list(&board);
    for (i = 0; i < 3u; ++i) {
        sb_object_create_display_list(&objects[i]);
    }
    sb_missing_texture_create_display_list(&missing_texture);
    sb_cone_scene_create_display_list(&cone_scene);

    previous_tick = SDL_GetTicks();

    while (running) {
        Uint32 frame_start = SDL_GetTicks();
        float dt = (float)(frame_start - previous_tick) / 1000.0f;
        int mouse_x = 0;
        int mouse_y = 0;
        SDL_Event event;

        previous_tick = frame_start;
        if (dt > SB_MAX_DT) {
            dt = SB_MAX_DT;
        }

        SDL_GetMouseState(&mouse_x, &mouse_y);

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = 0;
                }
                if (current_scene == SB_SCENE_CONE) {
                    sb_cone_scene_handle_event(&cone_scene);
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                int click_x;
                int click_y;

                if (event.button.button != SDL_BUTTON_LEFT) {
                    continue;
                }

                // use the click positions from last SDL event, makes the triangle in voidside more responsive
                click_x = event.button.x;
                click_y = event.button.y;

                if (current_scene == SB_SCENE_MAIN) {
                    double origin[3];
                    double direction[3];
                    int clicked_index;

                    if (!sb_raycast_mouse(&raycast, click_x, click_y, origin, direction)) {
                        continue;
                    }

                    clicked_index = sb_pick_clicked(objects, 3u, origin, direction);
                    if (clicked_index < 0) {
                        continue;
                    }

                    if (objects[clicked_index].type == SB_OBJ_SPHERE) {
                        current_scene = SB_SCENE_ERROR;
                        sb_audio_stop_music(audio);
                        SDL_ShowCursor(SDL_DISABLE);
                        cursor.enabled = 0;
                        sb_log_printf("[scene] sphere clicked -> missing texture");
                    } else if (objects[clicked_index].type == SB_OBJ_CONE) {
                        sb_app_flash_cone_object(&app, audio, &cursor, &camera, &light,
                                                 &board, objects, 3u,
                                                 &objects[clicked_index],
                                                 click_x, click_y);
                        current_scene = SB_SCENE_CONE;
                        sb_app_enter_cone(&app, audio);
                        sb_log_printf("[scene] cone clicked -> voidside");
                    } else if (objects[clicked_index].type == SB_OBJ_CUBE) {
                        sb_audio_play_sound(audio, "cube_click");

                        objects[clicked_index].rotation_angle = 0.0f;
                        if (!objects[clicked_index].is_rotating) {
                            objects[clicked_index].is_rotating = 1;
                            objects[clicked_index].position[0] = -0.69f;
                            objects[clicked_index].position[1] = 1.43f;
                            objects[clicked_index].position[2] = -1.61f;
                            objects[clicked_index].base_rotation[0] = 1422.99f;
                            objects[clicked_index].base_rotation[1] = -1461.21f;
                            objects[clicked_index].base_rotation[2] = 24.37f;
                            objects[clicked_index].scale = 1.22f;
                            objects[clicked_index].scale_xyz[0] = 1.15f;
                            objects[clicked_index].scale_xyz[1] = 1.19f;
                            objects[clicked_index].scale_xyz[2] = 1.19f;
                        }
                    }
                } else if (current_scene == SB_SCENE_CONE) {
                    // check the triangle in voidside
                    if (sb_cone_scene_check_triangle_click(&cone_scene, click_x, click_y,
                                                           app.width, app.height)) {
                        sb_app_flash_triangle(&app, audio, &cursor, &cone_scene,
                                              click_x, click_y);

                        // if clicked, return to menu
                        current_scene = SB_SCENE_MAIN;
                        sb_app_leave_cone(&app, audio, &cursor,
                                          original_width, original_height);
                        sb_log_printf("[scene] triangle clicked -> main menu");
                    }
                }
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                    event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    int width = 0;
                    int height = 0;

                    SDL_GetWindowSize(app.window, &width, &height);

                    // resize the window using SDL instead of OpenGL, as SDL doesnt delete and recreate the context
                    if (width != app.width || height != app.height) {
                        SDL_SetWindowSize(app.window, width, height);
                    }
                    sb_app_sync_drawable_size(&app);

                    sb_log_printf("[resize] %dx%d window / %dx%d drawable",
                                  app.width, app.height,
                                  app.drawable_width, app.drawable_height);
                }
            }
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (current_scene == SB_SCENE_MAIN) {
            double origin[3];
            double direction[3];
            int hover_sound;

            for (i = 0; i < 3u; ++i) {
                sb_object_update_rotation(&objects[i], dt);
                sb_object_update_animation(&objects[i], dt);
            }

            sb_camera_apply(&camera);
            sb_light_apply(&light);

            sb_raycast_update(&raycast);

            if (sb_raycast_mouse(&raycast, mouse_x, mouse_y, origin, direction)) {
                // play the hover sound
                sb_pick_hovered(objects, 3u, origin, direction, &hover_sound);
                if (hover_sound) {
                    sb_audio_play_sound(audio, "hover");
                }
            }

            sb_checkerboard_draw(&board);
            glDisable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);

            for (i = 0; i < 3u; ++i) {
                sb_object_draw(&objects[i]);
            }

            if (cursor.enabled) {
                sb_cursor_draw(&cursor, mouse_x, mouse_y, app.width, app.height);
            }
        } else if (current_scene == SB_SCENE_ERROR) {
            sb_missing_texture_update(&missing_texture, dt);
            sb_missing_texture_draw(&missing_texture, app.width, app.height);
        } else if (current_scene == SB_SCENE_CONE) {
            sb_cone_scene_update(&cone_scene, dt);
            sb_cone_scene_check_triangle_hover(&cone_scene, mouse_x, mouse_y,
                                               app.width, app.height);
            sb_cone_renderer_draw(&cone_scene, app.width, app.height);
            if (cursor.enabled) {
                sb_cursor_draw(&cursor, mouse_x, mouse_y, app.width, app.height);
            }
        }

        SDL_GL_SwapWindow(app.window);

        {
            Uint32 elapsed = SDL_GetTicks() - frame_start;
            if (elapsed < SB_FRAME_MS) {
                SDL_Delay(SB_FRAME_MS - elapsed);
            }
        }
    }

    sb_log_printf("Cleaning up");

    sb_cursor_cleanup(&cursor);
    sb_audio_stop_music(audio);

    sb_checkerboard_cleanup(&board);
    for (i = 0; i < 3u; ++i) {
        sb_object_cleanup(&objects[i]);
    }
    sb_missing_texture_cleanup(&missing_texture);
    sb_cone_scene_cleanup(&cone_scene);

    sb_text_purge_cache();
    sb_app_log_gl_error("shutdown");

    if (app.context != NULL) {
        SDL_GL_DeleteContext(app.context);
    }
    if (app.window != NULL) {
        SDL_DestroyWindow(app.window);
    }

    sb_audio_destroy(audio);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    sb_log_printf("Goodbye!");
    sb_log_close();
    return 0;
}