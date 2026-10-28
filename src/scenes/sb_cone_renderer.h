#ifndef SB_CONE_RENDERER_H
#define SB_CONE_RENDERER_H

#include "scenes/sb_cone_scene.h"

// horrible voidsite render logic

void sb_cone_renderer_load_grid_texture(sb_cone_scene *scene);

void sb_cone_renderer_draw_cyan_triangle_sprite(sb_cone_scene *scene,
                                                int display_width, int display_height);

void sb_cone_renderer_draw_loading_text(sb_cone_scene *scene,
                                        int display_width, int display_height);

void sb_cone_renderer_draw_coordinate_display(sb_cone_scene *scene,
                                              int display_width, int display_height);

void sb_cone_renderer_draw_black_screen(int display_width, int display_height);

void sb_cone_renderer_draw_skybox_sprite(int display_width, int display_height);

void sb_cone_renderer_draw_grey_cones(sb_cone_scene *scene);

void sb_cone_renderer_draw_pink_triangles(sb_cone_scene *scene);

void sb_cone_renderer_draw_red_object(sb_cone_scene *scene);

void sb_cone_renderer_draw_blue_box_and_line(sb_cone_scene *scene);

void sb_cone_renderer_draw_target(sb_cone_scene *scene);

// really akward workaround. linux doesnt render the grid correctly, applying too much fog
// for some reason. so platform dependant code i guess
// windows' implementation is really close to INTERLOPER, but i couldnt get linux to have the same accuracy
void sb_cone_renderer_draw_optimized_grids(sb_cone_scene *scene);
void sb_cone_renderer_draw_optimized_grids_windows(sb_cone_scene *scene);
void sb_cone_renderer_draw_optimized_grids_linux(sb_cone_scene *scene);

void sb_cone_renderer_draw(sb_cone_scene *scene, int display_width, int display_height);

#endif /* SB_CONE_RENDERER_H */