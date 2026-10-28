#include "scenes/sb_cone_renderer.h"

#include "core/sb_log.h"
#include "core/sb_resources.h"
#include "gl/glx.h"
#include "gl/glglu.h"
#include "gl/sb_image.h"
#include "gl/sb_text.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// behold, the longest file in the project, also takes a bit to compile this one

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SB_MAX_GRID_PLANES 64
#define SB_COORD_TEXT_MAX 512

typedef enum {
    SB_PLANE_XY = 0,
    SB_PLANE_XZ,
    SB_PLANE_YZ
} sb_plane_axis;

typedef struct {
    sb_plane_axis axis;
    float position;
    float distance;
} sb_grid_plane;

static const char *const SB_GRID_TEXTURE_CANDIDATES[] = {
    "img/grid.png",
    "assets/images/grid.png",
    "grid.png",
    NULL
};

// draws some text at the specified coords with the flags passed to it
static int sb_cone_draw_text(sb_cone_scene *scene, const char *text,
                             float x, float y, int font_size,
                             const unsigned char color[4], int bold,
                             int use_mipmaps, int flip, int *out_height)
{
    int width = 0;
    int height = 0;
    unsigned int texture;

    if (out_height != NULL) {
        *out_height = 0;
    }

    if (scene == NULL || text == NULL || text[0] == '\0') {
        return 0;
    }

    texture = sb_text_texture_flags(text, scene->font_path, font_size, color,
                                    0 /* letter_spacing */, bold,
                                    use_mipmaps, flip, &width, &height);
    if (texture == 0u) {
        return 0;
    }
    if (width <= 0 || height <= 0) {
        sb_text_release(texture);
        return 0;
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x + (float)width, y);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x + (float)width, y + (float)height);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x, y + (float)height);
    glEnd();

    sb_text_release(texture);

    if (out_height != NULL) {
        *out_height = height;
    }
    return width;
}

// format the coords like sourcebox does in INTERLOPER
static void sb_cone_format_coord(double value, int decimals, char *out, size_t out_size)
{
    char integer_text[64];
    char fraction_text[64];
    char joined[SB_COORD_TEXT_MAX];
    double integer_part;
    double fraction_part;
    char digits[32];
    size_t digit_index;
    size_t out_length;
    char thousands[64];
    size_t thousands_length;
    int i;

    if (out == NULL || out_size == 0) {
        return;
    }

    integer_part = (value < 0.0) ? ceil(value) : floor(value);
    fraction_part = value - integer_part;

    snprintf(digits, sizeof digits, "%lld", (long long)integer_part);

    // keep the sign and separate every 3 digits
    digit_index = 0u;
    thousands_length = 0u;
    for (i = 0; digits[i] != '\0'; ++i) {
        char c = digits[i];

        if (c == '-') {
            thousands[thousands_length++] = c;
            continue;
        }
        if (digit_index > 0u && (digit_index % 3u) == 0u) {
            thousands[thousands_length++] = ',';
        }
        thousands[thousands_length++] = c;
        ++digit_index;
    }
    thousands[thousands_length] = '\0';
    snprintf(integer_text, sizeof integer_text, "%s", thousands);

    fraction_text[0] = '\0';
    if (decimals > 0) {
        snprintf(fraction_text, sizeof fraction_text, "%.*f", decimals, fraction_part);
        // sometimes signs get duplicated going from int to text string, so make sure that doesnt happen
        memmove(fraction_text, fraction_text + 1, strlen(fraction_text));
    }

    snprintf(joined, sizeof joined, "%s%s", integer_text, fraction_text);

    // make some space before the text so i dont have to fuck around with opengl rendering too much
    out_length = 0u;
    for (i = 0; joined[i] != '\0'; ++i) {
        if (i > 0) {
            if (out_length + 7u >= out_size) {
                break;
            }
            memcpy(out + out_length, "       ", 7u);
            out_length += 7u;
        }
        if (out_length + 1u >= out_size) {
            break;
        }
        out[out_length++] = joined[i];
    }
    out[out_length] = '\0';
}

void sb_cone_renderer_load_grid_texture(sb_cone_scene *scene)
{
    char path[512];
    sb_image image;
    int width;
    int height;
    size_t i;

    if (scene == NULL) {
        return;
    }

    scene->grid_texture = 0u;

    if (!sb_find_resource(SB_GRID_TEXTURE_CANDIDATES,
                          sizeof SB_GRID_TEXTURE_CANDIDATES / sizeof SB_GRID_TEXTURE_CANDIDATES[0],
                          path, sizeof path)) {
        sb_log_printf("[cone] grid texture not found; falling back to flat planes");
        return;
    }
    // flip the grid texture, even then, linux fucks up uploading to gpu?
    if (!sb_image_load(path, 1, &image)) {
        sb_log_printf("[cone] grid texture unreadable: %s", path);
        return;
    }

    // black pixels in the texture are transparent
    for (i = 0; i < (size_t)image.width * (size_t)image.height; ++i) {
        unsigned char *px = image.pixels + i * 4u;
        if (px[0] == 0 && px[1] == 0 && px[2] == 0) {
            px[3] = 0;
        }
    }

    width = image.width;
    height = image.height;

    glGenTextures(1, &scene->grid_texture);
    if (scene->grid_texture == 0u) {
        sb_image_release(&image);
        return;
    }

    glBindTexture(GL_TEXTURE_2D, scene->grid_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image.pixels);

    sb_image_release(&image);

    sb_log_printf("[cone] grid texture %s (%dx%d)", path, width, height);
}

void sb_cone_renderer_draw_cyan_triangle_sprite(sb_cone_scene *scene,
                                                int display_width, int display_height)
{
    float center_x;
    float center_y;
    float size;

    if (scene == NULL) {
        return;
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, (double)display_width, (double)display_height, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (scene->triangle_flash == SB_TRIANGLE_RED) {
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    } else if (scene->triangle_flash == SB_TRIANGLE_GREY) {
        glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
    } else if (scene->triangle_hovered) {
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    } else {
        glColor4f(0.0f, 1.0f, 1.0f, 1.0f);
    }

    center_x = (float)display_width * 0.85f;
    center_y = (float)display_height * 0.60f;
    size = 50.0f * 0.5f * scene->triangle_scale;

    // very slight nudge to the left
    glTranslatef(center_x, center_y, 0.0f);
    glRotatef(10.0f, 0.0f, 0.0f, 1.0f);
    glTranslatef(-center_x, -center_y, 0.0f);

    glBegin(GL_TRIANGLES);
    glVertex2f(center_x, center_y + size);
    glVertex2f(center_x - size, center_y - size);
    glVertex2f(center_x + size, center_y - size);
    glEnd();

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void sb_cone_renderer_draw_loading_text(sb_cone_scene *scene,
                                        int display_width, int display_height)
{
    static const unsigned char colour[4] = { 150, 0, 0, 255 };
    int text_height = 0;
    float x_pos;
    float y_pos;

    if (scene == NULL) {
        return;
    }

    // measure the text first, as the y position depends on the text's height
    {
        int ignored_width = 0;
        int measured_height = 0;
        unsigned int probe = sb_text_texture_flags("NG CONNECTION...", scene->font_path,
                                                  20, colour, 0, 1, 0, 0,
                                                  &ignored_width, &measured_height);
        if (probe == 0u) {
            return;
        }
        text_height = measured_height;
        sb_text_release(probe);
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, (double)display_width, (double)display_height, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    x_pos = -12.0f;
    y_pos = (float)(display_height / 2 - text_height / 2);

    // bold text with no mipmaps and upside down buffer output
    sb_cone_draw_text(scene, "NG CONNECTION...", x_pos, y_pos, 20, colour,
                      1, 0, 0, NULL);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void sb_cone_renderer_draw_coordinate_display(sb_cone_scene *scene,
                                              int display_width, int display_height)
{
    static const unsigned char yellow[4] = { 255, 255, 0, 255 };
    static const unsigned char red[4] = { 255, 0, 0, 255 };
    static const unsigned char orange[4] = { 255, 140, 0, 255 };
    char coord_text[SB_COORD_TEXT_MAX];
    float y_pos;
    float x_pos_start;
    int label_width;

    if (scene == NULL) {
        return;
    }

    if (scene->loading_effect_active) {
        return;
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, (double)display_width, (double)display_height, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    y_pos = 10.0f;

    // located interloper, yellow with no calculated spacing (because it is already spaced)
    sb_cone_draw_text(scene,
                      "L  O  C  A  T  E  D   W  F  A  3  Y  -  A   [  I  N  T  E  R  L  O  P  E  .  D  M  E  ]",
                      10.0f, y_pos, 18, yellow, 0, 1, 0, NULL);
    y_pos += 25.0f;

    // validator text, cuts off at the edge intentionally
    sb_cone_draw_text(scene,
                      "V A L I D A T O R S   H A V E   N O T   B E E N   V E R I F I E D,   P L E A S E",
                      10.0f, y_pos, 18, yellow, 0, 1, 0, NULL);
    y_pos += 23.0f;

    if (!scene->coords_initialized) {
        sb_cone_scene_update_stored_coords(scene);
        scene->coords_initialized = 1;
    }

    x_pos_start = 20.0f;

    // funky render tricks to get the generated text to align with coord display
    label_width = sb_cone_draw_text(scene, "X", x_pos_start, y_pos, 18, yellow, 0, 1, 0, NULL);
    sb_cone_format_coord(scene->stored_coords[0], 3, coord_text, sizeof coord_text);
    sb_cone_draw_text(scene, coord_text, x_pos_start + (float)label_width + 5.0f,
                      y_pos - 2.0f, 23, red, 0, 1, 0, NULL);
    y_pos += 25.0f;

    label_width = sb_cone_draw_text(scene, "Y", x_pos_start, y_pos, 18, yellow, 0, 1, 0, NULL);
    sb_cone_format_coord(scene->stored_coords[1], 2, coord_text, sizeof coord_text);
    sb_cone_draw_text(scene, coord_text, x_pos_start + (float)label_width + 5.0f,
                      y_pos - 2.0f, 23, red, 0, 1, 0, NULL);
    y_pos += 25.0f;

    label_width = sb_cone_draw_text(scene, "Z", x_pos_start, y_pos, 18, yellow, 0, 1, 0, NULL);
    sb_cone_format_coord(scene->stored_coords[2], 2, coord_text, sizeof coord_text);
    sb_cone_draw_text(scene, coord_text, x_pos_start + (float)label_width + 5.0f,
                      y_pos - 2.0f, 23, red, 0, 1, 0, NULL);
    y_pos += 28.0f;

    // metal reg thingy
    {
        const char *phrase = scene->metal_reg_phrases[scene->metal_reg_index];
        label_width = sb_cone_draw_text(scene, "METAL_REG-", 10.0f, y_pos, 16,
                                        yellow, 0, 1, 0, NULL);
        sb_cone_draw_text(scene, phrase, 10.0f + (float)label_width, y_pos + 2.5f,
                          12, orange, 0, 1, 0, NULL);
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void sb_cone_renderer_draw_black_screen(int display_width, int display_height)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, (double)display_width, (double)display_height, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f((float)display_width, 0.0f);
    glVertex2f((float)display_width, (float)display_height);
    glVertex2f(0.0f, (float)display_height);
    glEnd();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void sb_cone_renderer_draw_skybox_sprite(int display_width, int display_height)
{
    static const float colour1[4] = { 0.04f, 0.01f, 0.04f, 0.6f };
    static const float colour2[4] = { 0.01f, 0.01f, 0.01f, 0.6f };
    double scale = 2.0;
    float checker_width;
    float checker_height;
    int cols;
    int rows;
    int i;
    int j;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, (double)display_width, (double)display_height, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    checker_width = (float)(80.0 * scale);
    checker_height = (float)(60.0 * scale);
    cols = (int)((double)display_width / (double)checker_width) + 2;
    rows = (int)((double)display_height / (double)checker_height) + 2;

    glBegin(GL_QUADS);
    for (i = 0; i < rows; ++i) {
        for (j = 0; j < cols; ++j) {
            const float *colour = (((i + j) % 2) != 0) ? colour1 : colour2;
            float x = (float)j * checker_width;
            float y = (float)i * checker_height;

            glColor4f(colour[0], colour[1], colour[2], colour[3]);

            glVertex2f(x, y);
            glVertex2f(x + checker_width, y);
            glVertex2f(x + checker_width, y + checker_height);
            glVertex2f(x, y + checker_height);
        }
    }
    glEnd();

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_FOG);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void sb_cone_renderer_draw_grey_cones(sb_cone_scene *scene)
{
    int i;

    if (scene == NULL) {
        return;
    }

    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);

    for (i = 0; i < scene->grey_cone_count; ++i) {
        sb_grey_cone_draw(&scene->grey_cones[i]);
    }

    glDisable(GL_LIGHTING);
}

void sb_cone_renderer_draw_pink_triangles(sb_cone_scene *scene)
{
    int i;

    if (scene == NULL) {
        return;
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glEnable(GL_DEPTH_TEST);

    for (i = 0; i < scene->pink_triangle_count; ++i) {
        const sb_pink_triangle *triangle = &scene->pink_triangles[i];
        const float *pos = triangle->position;
        float size = triangle->size;
        double dx = (double)scene->camera_pos[0] - (double)pos[0];
        double dy = (double)scene->camera_pos[1] - (double)pos[1];
        double dz = (double)scene->camera_pos[2] - (double)pos[2];
        double angle_y = atan2(dx, dz) * 180.0 / M_PI;
        double horizontal_dist = sqrt(dx * dx + dz * dz);
        double angle_x = -atan2(dy, horizontal_dist) * 180.0 / M_PI;

        // the pink triangles (missing texture) always face the camera, so emulate billboarding
        glPushMatrix();
        glTranslatef(pos[0], pos[1], pos[2]);
        glRotatef((float)angle_y, 0.0f, 1.0f, 0.0f);
        glRotatef((float)angle_x, 1.0f, 0.0f, 0.0f);

        glColor3f(1.0f, 0.6f, 1.0f);

        glBegin(GL_TRIANGLES);
        glVertex3f(0.0f, size * 0.7f, 0.0f);
        glVertex3f(-size * 1.0f, -size * 0.4f, 0.0f);
        glVertex3f(size * 1.0f, -size * 0.4f, 0.0f);
        glEnd();

        glPopMatrix();
    }

    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
}

void sb_cone_renderer_draw_red_object(sb_cone_scene *scene)
{
    static const float verts[6][3] = {
        {  0.0f,  4.8f,  0.0f },
        { -4.0f,  0.0f,  2.8f },
        {  3.2f,  0.0f,  4.0f },
        {  4.0f,  0.0f, -2.4f },
        { -2.8f,  0.0f, -4.0f },
        {  0.0f, -3.2f,  0.0f }
    };
    static const int faces[8][3] = {
        { 0, 1, 2 }, { 0, 2, 3 }, { 0, 3, 4 }, { 0, 4, 1 },
        { 5, 2, 1 }, { 5, 3, 2 }, { 5, 4, 3 }, { 5, 1, 4 }
    };
    double pulse;
    double alpha;
    int f;

    if (scene == NULL || !scene->red_object_visible) {
        return;
    }

    glDisable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    pulse = 0.5 + 0.5 * sin((double)scene->red_object_pulse_timer * 2.0);
    alpha = 0.4 + 0.3 * pulse;

    glPushMatrix();
    glTranslatef(scene->red_object_position[0],
                 scene->red_object_position[1],
                 scene->red_object_position[2]);
    glRotatef(scene->red_object_rotation, 0.0f, 1.0f, 0.0f);
    glRotatef(scene->red_object_pulse_timer * 15.0f, 1.0f, 0.0f, 0.0f);

    glColor4f(0.8f, 0.05f, 0.05f, (float)alpha);

    glBegin(GL_TRIANGLES);
    for (f = 0; f < 8; ++f) {
        const float *v0 = verts[faces[f][0]];
        const float *v1 = verts[faces[f][1]];
        const float *v2 = verts[faces[f][2]];
        double ax = (double)v1[0] - (double)v0[0];
        double ay = (double)v1[1] - (double)v0[1];
        double az = (double)v1[2] - (double)v0[2];
        double bx = (double)v2[0] - (double)v0[0];
        double by = (double)v2[1] - (double)v0[1];
        double bz = (double)v2[2] - (double)v0[2];
        double nx = ay * bz - az * by;
        double ny = az * bx - ax * bz;
        double nz = ax * by - ay * bx;
        double length = sqrt(nx * nx + ny * ny + nz * nz);

        if (length > 0.0) {
            glNormal3f((float)(nx / length), (float)(ny / length), (float)(nz / length));
        }
        glVertex3f(v0[0], v0[1], v0[2]);
        glVertex3f(v1[0], v1[1], v1[2]);
        glVertex3f(v2[0], v2[1], v2[2]);
    }
    glEnd();

    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void sb_cone_renderer_draw_blue_box_and_line(sb_cone_scene *scene)
{
    float dot_pos[3];
    float half_w;
    float half_h;
    float half_d;

    if (scene == NULL) {
        return;
    }

    glDisable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    dot_pos[0] = scene->target_position[0] + scene->dot_offset[0];
    dot_pos[1] = scene->target_position[1] + scene->dot_offset[1];
    dot_pos[2] = scene->target_position[2] + scene->dot_offset[2];

    // line from the cone to target
    glColor4f(0.0f, 0.1f, 0.6f, 0.4f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex3f(dot_pos[0], dot_pos[1], dot_pos[2]);
    glVertex3f(scene->box_position[0], scene->box_position[1], scene->box_position[2]);
    glEnd();
    glLineWidth(1.0f);

    // weird unknown blue box
    glPushMatrix();
    glTranslatef(scene->box_position[0], scene->box_position[1], scene->box_position[2]);

    glColor4f(0.0f, 0.15f, 0.7f, 0.3f);

    half_w = scene->box_size[0] / 2.0f;
    half_h = scene->box_size[1] / 2.0f;
    half_d = scene->box_size[2] / 2.0f;

    glBegin(GL_QUADS);
    // front face
    glVertex3f(-half_w, -half_h, half_d);
    glVertex3f(half_w, -half_h, half_d);
    glVertex3f(half_w, half_h, half_d);
    glVertex3f(-half_w, half_h, half_d);
    // back face
    glVertex3f(half_w, -half_h, -half_d);
    glVertex3f(-half_w, -half_h, -half_d);
    glVertex3f(-half_w, half_h, -half_d);
    glVertex3f(half_w, half_h, -half_d);
    // top face
    glVertex3f(-half_w, half_h, half_d);
    glVertex3f(half_w, half_h, half_d);
    glVertex3f(half_w, half_h, -half_d);
    glVertex3f(-half_w, half_h, -half_d);
    // bottom face
    glVertex3f(-half_w, -half_h, -half_d);
    glVertex3f(half_w, -half_h, -half_d);
    glVertex3f(half_w, -half_h, half_d);
    glVertex3f(-half_w, -half_h, half_d);
    //right face
    glVertex3f(half_w, -half_h, half_d);
    glVertex3f(half_w, -half_h, -half_d);
    glVertex3f(half_w, half_h, -half_d);
    glVertex3f(half_w, half_h, half_d);
    //left face
    glVertex3f(-half_w, -half_h, -half_d);
    glVertex3f(-half_w, -half_h, half_d);
    glVertex3f(-half_w, half_h, half_d);
    glVertex3f(-half_w, half_h, -half_d);
    glEnd();

    glPopMatrix();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void sb_cone_renderer_draw_target(sb_cone_scene *scene)
{
    float dot_pos[3];
    double dx;
    double dy;
    double dz;
    double angle_y;
    double horizontal_dist;
    double angle_x;
    int i;
    const int segments = 12;

    if (scene == NULL) {
        return;
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    // snap target to the center of the screen, with ocassional attempts at going away
    dot_pos[0] = scene->target_position[0] + scene->dot_offset[0];
    dot_pos[1] = scene->target_position[1] + scene->dot_offset[1];
    dot_pos[2] = scene->target_position[2] + scene->dot_offset[2];

    // trace movement with a line
    glPushMatrix();
    glTranslatef(dot_pos[0], dot_pos[1], dot_pos[2]);

    glColor3f(0.6f, 0.3f, 0.0f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(scene->trace_direction[0] * scene->trace_length,
               scene->trace_direction[1] * scene->trace_length,
               scene->trace_direction[2] * scene->trace_length);
    glEnd();
    glPopMatrix();

    // billboard the target and marking cone
    glPushMatrix();
    glTranslatef(dot_pos[0], dot_pos[1], dot_pos[2]);

    dx = (double)scene->camera_pos[0] - (double)dot_pos[0];
    dy = (double)scene->camera_pos[1] - (double)dot_pos[1];
    dz = (double)scene->camera_pos[2] - (double)dot_pos[2];

    angle_y = atan2(dx, dz) * 180.0 / M_PI;
    horizontal_dist = sqrt(dx * dx + dz * dz);
    angle_x = -atan2(dy, horizontal_dist) * 180.0 / M_PI;

    glRotatef((float)angle_y, 0.0f, 1.0f, 0.0f);
    glRotatef((float)angle_x, 1.0f, 0.0f, 0.0f);

    // little white dot
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, 0.0f, 0.0f);
    for (i = 0; i <= segments; ++i) {
        double angle = ((double)i / (double)segments) * 2.0 * M_PI;
        glVertex3f(0.15f * (float)cos(angle), 0.15f * (float)sin(angle), 0.0f);
    }
    glEnd();

    // red cone with circle shaped base
    if (scene->cone_visible) {
        const int base_segments = 16;
        const float radius = 1.3f;

        glTranslatef(0.0f, 5.0f, 0.0f);
        glColor3f(1.0f, 0.0f, 0.0f);

        glBegin(GL_TRIANGLES);
        glVertex3f(0.0f, -3.0f, 0.0f);
        glVertex3f(-1.3f, 0.0f, 0.0f);
        glVertex3f(1.3f, 0.0f, 0.0f);
        glEnd();

        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(0.0f, 0.0f, 0.0f);
        for (i = 0; i <= base_segments; ++i) {
            double angle = ((double)i / (double)base_segments) * 2.0 * M_PI;
            glVertex3f(radius * (float)cos(angle), 0.0f, 0.0f);
        }
        glEnd();
    }

    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

// get the different grid intersect planes, to then apply fog to them (slightly broken on linux)
static int sb_cone_collect_grid_planes(const sb_cone_scene *scene, sb_grid_plane *planes)
{
    int half_count = scene->grid_count / 2;
    float spacing = scene->grid_spacing;
    float max_dist = scene->fog_end * 1.5f;
    int count = 0;
    int i;
    int pass;

    for (pass = 0; pass < 3; ++pass) {
        for (i = -half_count; i <= half_count; ++i) {
            float position = (float)i * spacing;
            float reference = (pass == 0) ? scene->camera_pos[2]
                             : (pass == 1) ? scene->camera_pos[1]
                                           : scene->camera_pos[0];
            float delta = position - reference;

            if (fabs(delta) < max_dist && count < SB_MAX_GRID_PLANES) {
                planes[count].axis = (pass == 0) ? SB_PLANE_XY
                                 : (pass == 1) ? SB_PLANE_XZ
                                               : SB_PLANE_YZ;
                planes[count].position = position;
                planes[count].distance = fabs(delta);
                ++count;
            }
        }
    }

    // sort from farthest to closest
    for (i = 1; i < count; ++i) {
        sb_grid_plane key = planes[i];
        int j = i - 1;
        while (j >= 0 && planes[j].distance < key.distance) {
            planes[j + 1] = planes[j];
            --j;
        }
        planes[j + 1] = key;
    }

    return count;
}

// and here is the jankiest code in the project, again platform dependant code
// except it isnt. some GPUs i have tested display the windows rendering on linux correctly
// while others dont, and it isnt something i can detect on runtime
// for anyone that is interested, the linux renderer works fine on windows
// i didnt test the windows build through wine or proton...
// maybe it isnt hardware dependent but implementation dependent?
// is this something opengl32 does CORRECT?

void sb_cone_renderer_draw_optimized_grids_windows(sb_cone_scene *scene)
{
    sb_grid_plane planes[SB_MAX_GRID_PLANES];
    float tex_scale;
    float half_plane;
    int plane_count;
    int i;

    if (scene == NULL) {
        return;
    }

    tex_scale = scene->texture_scale;
    half_plane = (float)scene->grid_count * scene->grid_spacing / 2.0f;

    plane_count = sb_cone_collect_grid_planes(scene, planes);

    if (scene->grid_texture != 0u) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, scene->grid_texture);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    } else {
        glColor4f(1.0f, 1.0f, 1.0f, 0.4f);
    }

    glBegin(GL_QUADS);
    for (i = 0; i < plane_count; ++i) {
        float pos = planes[i].position;

        switch (planes[i].axis) {
        case SB_PLANE_XY:
            glNormal3f(0.0f, 0.0f, 1.0f);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-half_plane, -half_plane, pos);
            glTexCoord2f(tex_scale, 0.0f); glVertex3f(half_plane, -half_plane, pos);
            glTexCoord2f(tex_scale, tex_scale); glVertex3f(half_plane, half_plane, pos);
            glTexCoord2f(0.0f, tex_scale); glVertex3f(-half_plane, half_plane, pos);
            break;
        case SB_PLANE_XZ:
            glNormal3f(0.0f, 1.0f, 0.0f);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-half_plane, pos, -half_plane);
            glTexCoord2f(tex_scale, 0.0f); glVertex3f(half_plane, pos, -half_plane);
            glTexCoord2f(tex_scale, tex_scale); glVertex3f(half_plane, pos, half_plane);
            glTexCoord2f(0.0f, tex_scale); glVertex3f(-half_plane, pos, half_plane);
            break;
        case SB_PLANE_YZ:
        default:
            glNormal3f(1.0f, 0.0f, 0.0f);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(pos, -half_plane, -half_plane);
            glTexCoord2f(tex_scale, 0.0f); glVertex3f(pos, half_plane, -half_plane);
            glTexCoord2f(tex_scale, tex_scale); glVertex3f(pos, half_plane, half_plane);
            glTexCoord2f(0.0f, tex_scale); glVertex3f(pos, -half_plane, half_plane);
            break;
        }
    }
    glEnd();

    if (scene->grid_texture != 0u) {
        glDisable(GL_TEXTURE_2D);
    }
}

void sb_cone_renderer_draw_optimized_grids_linux(sb_cone_scene *scene)
{
    sb_grid_plane planes[SB_MAX_GRID_PLANES];
    float tex_scale;
    float plane_size;
    float half_plane;
    int plane_count;
    int plane_index;
    const int subdivisions = 8;
    float step;

    if (scene == NULL) {
        return;
    }

    tex_scale = scene->texture_scale;
    plane_size = (float)scene->grid_count * scene->grid_spacing;
    half_plane = plane_size / 2.0f;
    step = plane_size / (float)subdivisions;

    plane_count = sb_cone_collect_grid_planes(scene, planes);

    glDisable(GL_FOG);
    glDisable(GL_LIGHTING);

    if (scene->grid_texture != 0u) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, scene->grid_texture);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }

    for (plane_index = 0; plane_index < plane_count; ++plane_index) {
        const sb_plane_axis axis = planes[plane_index].axis;
        const float pos = planes[plane_index].position;
        int i;
        int j;

        for (i = 0; i < subdivisions; ++i) {
            for (j = 0; j < subdivisions; ++j) {
                float x0 = -half_plane + (float)i * step;
                float x1 = -half_plane + (float)(i + 1) * step;
                float y0 = -half_plane + (float)j * step;
                float y1 = -half_plane + (float)(j + 1) * step;

                float u0 = ((float)i / (float)subdivisions) * tex_scale;
                float u1 = ((float)(i + 1) / (float)subdivisions) * tex_scale;
                float v0 = ((float)j / (float)subdivisions) * tex_scale;
                float v1 = ((float)(j + 1) / (float)subdivisions) * tex_scale;

                float centre_x;
                float centre_y;
                float centre_z;
                double distance;
                double alpha;

                switch (axis) {
                case SB_PLANE_XY:
                    centre_x = (x0 + x1) / 2.0f;
                    centre_y = (y0 + y1) / 2.0f;
                    centre_z = pos;
                    break;
                case SB_PLANE_XZ:
                    centre_x = (x0 + x1) / 2.0f;
                    centre_y = pos;
                    centre_z = (y0 + y1) / 2.0f;
                    break;
                case SB_PLANE_YZ:
                default:
                    centre_x = pos;
                    centre_y = (x0 + x1) / 2.0f;
                    centre_z = (y0 + y1) / 2.0f;
                    break;
                }

                distance = sqrt(pow((double)centre_x - (double)scene->camera_pos[0], 2.0) +
                               pow((double)centre_y - (double)scene->camera_pos[1], 2.0) +
                               pow((double)centre_z - (double)scene->camera_pos[2], 2.0));

                if (distance <= (double)scene->fog_start) {
                    alpha = 1.0;
                } else if (distance >= (double)scene->fog_end) {
                    continue;
                } else {
                    double t = (distance - (double)scene->fog_start) /
                               ((double)scene->fog_end - (double)scene->fog_start);
                    alpha = 1.0 - t;
                }

                if (alpha < 0.01) {
                    continue;
                }

                glColor4f(1.0f, 1.0f, 1.0f, (float)alpha);

                glBegin(GL_QUADS);
                switch (axis) {
                case SB_PLANE_XY:
                    glNormal3f(0.0f, 0.0f, 1.0f);
                    glTexCoord2f(u0, v0); glVertex3f(x0, y0, pos);
                    glTexCoord2f(u1, v0); glVertex3f(x1, y0, pos);
                    glTexCoord2f(u1, v1); glVertex3f(x1, y1, pos);
                    glTexCoord2f(u0, v1); glVertex3f(x0, y1, pos);
                    break;
                case SB_PLANE_XZ:
                    glNormal3f(0.0f, 1.0f, 0.0f);
                    glTexCoord2f(u0, v0); glVertex3f(x0, pos, y0);
                    glTexCoord2f(u1, v0); glVertex3f(x1, pos, y0);
                    glTexCoord2f(u1, v1); glVertex3f(x1, pos, y1);
                    glTexCoord2f(u0, v1); glVertex3f(x0, pos, y1);
                    break;
                case SB_PLANE_YZ:
                default:
                    glNormal3f(1.0f, 0.0f, 0.0f);
                    glTexCoord2f(u0, v0); glVertex3f(pos, x0, y0);
                    glTexCoord2f(u1, v0); glVertex3f(pos, x1, y0);
                    glTexCoord2f(u1, v1); glVertex3f(pos, x1, y1);
                    glTexCoord2f(u0, v1); glVertex3f(pos, x0, y1);
                    break;
                }
                glEnd();
            }
        }
    }

    if (scene->grid_texture != 0u) {
        glDisable(GL_TEXTURE_2D);
    }

    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
}

void sb_cone_renderer_draw_optimized_grids(sb_cone_scene *scene)
{
    if (scene == NULL) {
        return;
    }

    // done like this instead of ifndef because some hardware seems to be able to work correctly?
    // put this so the user can override using env var
    if (scene->is_linux) {
        sb_cone_renderer_draw_optimized_grids_linux(scene);
    } else {
        sb_cone_renderer_draw_optimized_grids_windows(scene);
    }
}

void sb_cone_renderer_draw(sb_cone_scene *scene, int display_width, int display_height)
{
    double aspect_ratio;

    if (scene == NULL) {
        return;
    }

    // dont render the voidsite while on main menu, yes this was an issue
    if (scene->loading_effect_active && scene->loading_text_visible) {
        sb_cone_renderer_draw_black_screen(display_width, display_height);
        sb_cone_renderer_draw_loading_text(scene, display_width, display_height);
        return;
    }

    glClearColor(scene->fog_color[0], scene->fog_color[1],
                 scene->fog_color[2], scene->fog_color[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, scene->fog_color);
    glFogf(GL_FOG_START, scene->fog_start);
    glFogf(GL_FOG_END, scene->fog_end);
    glHint(GL_FOG_HINT, GL_NICEST);

    sb_cone_renderer_draw_skybox_sprite(display_width, display_height);

    // enable 3D projection just in case it got disabled
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    aspect_ratio = (double)display_width / (double)display_height;
    gluPerspective(70.0, aspect_ratio, 1.0, 30000.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // use camera space axis rather than world space for rotating the camera
    glRotatef(scene->camera_rotation[2], 0.0f, 0.0f, 1.0f);
    glRotatef(scene->camera_rotation[0], 1.0f, 0.0f, 0.0f);
    glRotatef(scene->camera_rotation[1], 0.0f, 1.0f, 0.0f);

    glTranslatef(-scene->camera_pos[0], -scene->camera_pos[1], -scene->camera_pos[2]);

    // do some trickery to draw grids transparent
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    sb_cone_renderer_draw_optimized_grids(scene);

    glDepthMask(GL_TRUE);

    sb_cone_renderer_draw_grey_cones(scene);
    sb_cone_renderer_draw_pink_triangles(scene);
    sb_cone_renderer_draw_red_object(scene);
    sb_cone_renderer_draw_blue_box_and_line(scene);
    sb_cone_renderer_draw_target(scene);
    sb_cone_renderer_draw_cyan_triangle_sprite(scene, display_width, display_height);
    sb_cone_renderer_draw_coordinate_display(scene, display_width, display_height);

    glDisable(GL_FOG);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}