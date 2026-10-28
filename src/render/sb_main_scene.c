#include "render/sb_main_scene.h"

#include "core/sb_log.h"
#include "gl/glglu.h"
#include "render/sb_raycast.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// just in case define PI. it should be defined but you never know ;)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float sb_absf(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float sb_max3(const float v[3])
{
    float best = v[0];
    if (v[1] > best) {
        best = v[1];
    }
    if (v[2] > best) {
        best = v[2];
    }
    return best;
}

static const char *sb_obj_name(sb_obj_type type)
{
    switch (type) {
    case SB_OBJ_CUBE:
        return "cube";
    case SB_OBJ_SPHERE:
        return "sphere";
    case SB_OBJ_CONE:
        return "cone";
    default:
        return "object";
    }
}

void sb_object_init(sb_object *object, sb_obj_type type, const char *name,
                    const float position[3], const float rotation[3],
                    float scale, const float scale_xyz[3], float brightness)
{
    float largest;
    size_t i;

    if (object == NULL) {
        return;
    }

    memset(object, 0, sizeof *object);

    object->type = type;
    snprintf(object->name, sizeof object->name, "%s",
             (name != NULL) ? name : sb_obj_name(type));

    for (i = 0; i < 3u; ++i) {
        object->position[i] = position[i];
        object->base_rotation[i] = rotation[i];

        object->scale_xyz[i] = sb_absf(scale_xyz[i]);
        if (object->scale_xyz[i] < 0.01f) {
            object->scale_xyz[i] = 0.01f;
        }
    }

    object->scale = sb_absf(scale);
    if (object->scale < 0.01f) {
        object->scale = 0.01f;
    }
    object->base_scale = object->scale;

    object->brightness = brightness;
    if (object->brightness < 0.0f) {
        object->brightness = 0.0f;
    } else if (object->brightness > 1.0f) {
        object->brightness = 1.0f;
    }

    object->hover_animation_duration = 0.1f;
    object->hover_scale_amount = 0.05f;

    largest = sb_max3(object->scale_xyz);

    switch (type) {
    case SB_OBJ_SPHERE:
        object->bounding_radius = 0.5f * object->scale * largest;
        break;
    case SB_OBJ_CUBE:
        object->bounding_radius = 0.866f * object->scale * largest;
        break;
    case SB_OBJ_CONE:
        object->bounding_radius = 0.6f * object->scale * largest;
        break;
    default:
        object->bounding_radius = 0.5f * object->scale * largest;
        break;
    }
}

static void sb_object_draw_cube_geometry(void)
{
    glBegin(GL_QUADS);

    // top face
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);

    // bottom face
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);

    // right face
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);

    // left face
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);

    //front face (facing the camera)
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);

    // back face. should be culled by default but it is needed for 
    // spinning to work correctly. this also saved me from coding a culling algorith
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, -0.5f, 0.5f);

    glEnd();
}

void sb_object_create_display_list(sb_object *object)
{
    GLUquadric *quadric;

    if (object == NULL || object->display_list != 0u) {
        return;
    }

    object->display_list = glGenLists(1);
    if (object->display_list == 0u) {
        return;
    }

    glNewList(object->display_list, GL_COMPILE);

    switch (object->type) {
    case SB_OBJ_CUBE:
        sb_object_draw_cube_geometry();
        break;

    case SB_OBJ_SPHERE:
        quadric = gluNewQuadric();
        if (quadric != NULL) {
            gluQuadricNormals(quadric, GLU_SMOOTH);
            gluSphere(quadric, 0.5, 32, 32);
            gluDeleteQuadric(quadric);
        }
        break;

    case SB_OBJ_CONE:
        quadric = gluNewQuadric();
        if (quadric != NULL) {
            gluQuadricNormals(quadric, GLU_SMOOTH);
            gluCylinder(quadric, 0.5, 0.0, 1.0, 32, 4);
            gluDeleteQuadric(quadric);
        }

        // in theory GLU should make the cylinder of the cone have a base, but just in case...
        glBegin(GL_TRIANGLE_FAN);
        glNormal3f(0.0f, 0.0f, -1.0f);
        glVertex3f(0.0f, 0.0f, 0.0f);
        {
            int i;
            for (i = 0; i < 33; ++i) {
                GLfloat angle = (GLfloat)(((double)i / 32.0) * 2.0 * M_PI);
                glVertex3f((GLfloat)(0.5 * cos(angle)),
                           (GLfloat)(0.5 * sin(angle)),
                           0.0f);
            }
        }
        glEnd();
        break;

    default:
        break;
    }

    glEndList();

    // if somehow the display list is 0, log and abort. this shouldnt happen normally
    if (glIsList(object->display_list) == GL_FALSE) {
        sb_log_printf("[main] glNewList failed for %s", object->name);
        glDeleteLists(object->display_list, 1);
        object->display_list = 0u;
    }
}

void sb_object_cleanup(sb_object *object)
{
    if (object == NULL) {
        return;
    }

    if (object->display_list != 0u) {
        glDeleteLists(object->display_list, 1);
        object->display_list = 0u;
    }
}

void sb_object_update_rotation(sb_object *object, float dt)
{
    if (object == NULL) {
        return;
    }

    if (dt > 0.0f && dt <= 1.0f && object->is_rotating) {
        object->rotation_angle = (float)fmod((double)(object->rotation_angle + 45.0 * dt), 360.0);
    }
}

void sb_object_update_animation(sb_object *object, float dt)
{
    float progress;
    float scale_progress;

    if (object == NULL) {
        return;
    }

    if (dt <= 0.0f || dt > 1.0f) {
        return;
    }

    if (object->is_hovered && !object->was_hovered) {
        object->hover_timer = 0.0f;
        object->was_hovered = 1;
    } else if (!object->is_hovered && object->was_hovered) {
        object->hover_timer = 0.0f;
        object->scale = object->base_scale;
        object->was_hovered = 0;
        return;
    }

    if (!object->is_hovered) {
        return;
    }

    object->hover_timer += dt;

    if (object->hover_timer > object->hover_animation_duration) {
        object->scale = object->base_scale;
        return;
    }

    progress = object->hover_timer / object->hover_animation_duration;

    if (progress <= 0.5f) {
        scale_progress = progress * 5.0f;
        object->scale = object->base_scale * (1.0f + object->hover_scale_amount * scale_progress);
    } else {
        scale_progress = (progress - 0.5f) * 3.0f;
        object->scale = object->base_scale *
                        (1.0f + object->hover_scale_amount * (1.0f - scale_progress));
    }
}

void sb_object_draw(const sb_object *object)
{
    int lighting_disabled = 0;
    float brightness;

    if (object == NULL || object->display_list == 0u) {
        return;
    }

    glPushMatrix();
    glTranslatef(object->position[0], object->position[1], object->position[2]);

    if (object->base_rotation[0] != 0.0f) {
        glRotatef(object->base_rotation[0], 1.0f, 0.0f, 0.0f);
    }
    if (object->base_rotation[1] != 0.0f) {
        glRotatef(object->base_rotation[1], 0.0f, 1.0f, 0.0f);
    }
    if (object->base_rotation[2] != 0.0f) {
        glRotatef(object->base_rotation[2], 0.0f, 0.0f, 1.0f);
    }

    if (object->is_rotating || object->is_hovered) {
        glDisable(GL_LIGHTING);
        lighting_disabled = 1;
        if (object->is_rotating) {
            glRotatef(object->rotation_angle, 0.0f, 0.0f, 1.0f);
        }
    }

    glScalef(object->scale * object->scale_xyz[0],
             object->scale * object->scale_xyz[1],
             object->scale * object->scale_xyz[2]);

    if (object->is_rotating) {
        glColor3f(0.0f, 0.9f, 1.0f);
    } else if (object->is_hovered) {
        glColor3f(0.8f, 0.0f, 0.0f);
    } else {
        switch (object->type) {
        case SB_OBJ_SPHERE:
            brightness = 0.7f;
            break;
        case SB_OBJ_CUBE:
            brightness = 0.6f;
            break;
        case SB_OBJ_CONE:
            brightness = 0.65f;
            break;
        default:
            brightness = 0.6f;
            break;
        }
        brightness *= object->brightness;
        glColor3f(brightness, brightness, brightness);
    }

    glCallList(object->display_list);

    if (lighting_disabled) {
        glEnable(GL_LIGHTING);
    }

    glPopMatrix();
}

void sb_object_draw_flash(const sb_object *object, int is_red)
{
    float brightness;

    if (object == NULL || object->display_list == 0u) {
        return;
    }

    glPushMatrix();
    glTranslatef(object->position[0], object->position[1], object->position[2]);

    if (object->base_rotation[0] != 0.0f) {
        glRotatef(object->base_rotation[0], 1.0f, 0.0f, 0.0f);
    }
    if (object->base_rotation[1] != 0.0f) {
        glRotatef(object->base_rotation[1], 0.0f, 1.0f, 0.0f);
    }
    if (object->base_rotation[2] != 0.0f) {
        glRotatef(object->base_rotation[2], 0.0f, 0.0f, 1.0f);
    }

    glScalef(object->scale * object->scale_xyz[0],
             object->scale * object->scale_xyz[1],
             object->scale * object->scale_xyz[2]);

    if (is_red) {
        glDisable(GL_LIGHTING);
        glColor3f(1.0f, 0.0f, 0.0f);
        glCallList(object->display_list);
        glEnable(GL_LIGHTING);
    } else {
        // render the object with a bit of brightness added on top 
        // to smooth out transition between red and gray
        brightness = 0.65f * object->brightness;
        glColor3f(brightness, brightness, brightness);
        glCallList(object->display_list);
    }

    glPopMatrix();
}

void sb_camera_apply(const sb_camera *camera)
{
    if (camera == NULL) {
        return;
    }

    glLoadIdentity();
    glTranslatef(camera->position[0], camera->position[1], camera->position[2]);
    glRotatef(camera->rotation[0], 1.0f, 0.0f, 0.0f);
    glRotatef(camera->rotation[1], 0.0f, 1.0f, 0.0f);
    glRotatef(camera->rotation[2], 0.0f, 0.0f, 1.0f);
}

void sb_camera_init(sb_camera *camera)
{
    if (camera == NULL) {
        return;
    }

    camera->position[0] = 0.0f;
    camera->position[1] = -1.0f;
    camera->position[2] = -10.0f;
    camera->rotation[0] = 92.97f;
    camera->rotation[1] = -9.00f;
    camera->rotation[2] = -10.38f;
}

void sb_light_init(sb_light *light)
{
    if (light == NULL) {
        return;
    }

    memset(light, 0, sizeof *light);

    light->position[0] = 107.10f;
    light->position[1] = 2.85f;
    light->position[2] = -185.15f;
    light->position[3] = 1.0f;

    light->ambient[0] = 0.1f;
    light->ambient[1] = 0.1f;
    light->ambient[2] = 0.1f;
    light->ambient[3] = 1.0f;

    light->diffuse[0] = 1.0f;
    light->diffuse[1] = 1.0f;
    light->diffuse[2] = 1.0f;
    light->diffuse[3] = 1.0f;

    light->specular[0] = 1.0f;
    light->specular[1] = 1.0f;
    light->specular[2] = 1.0f;
    light->specular[3] = 1.0f;

    light->setup_done = 0;
}

void sb_light_apply(sb_light *light)
{
    if (light == NULL) {
        return;
    }

    if (!light->setup_done) {
        glLightfv(GL_LIGHT0, GL_AMBIENT, light->ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light->diffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, light->specular);
        light->setup_done = 1;
    }
    glLightfv(GL_LIGHT0, GL_POSITION, light->position);
}

void sb_checkerboard_init(sb_checkerboard *board)
{
    if (board == NULL) {
        return;
    }

    memset(board, 0, sizeof *board);

    board->size = 30;
    board->position[0] = -25.87f;
    board->position[1] = 0.53f;
    board->position[2] = 6.68f;
    board->rotation[0] = 0.0f;
    board->rotation[1] = 0.0f;
    board->rotation[2] = 0.0f;
    board->scale[0] = 1.55f;
    board->scale[1] = 0.63f;
    board->scale[2] = 1.22f;
    board->dark_color[0] = 0.2f;
    board->dark_color[1] = 0.2f;
    board->dark_color[2] = 0.2f;
    board->light_color[0] = 0.0f;
    board->light_color[1] = 0.0f;
    board->light_color[2] = 0.0f;
    board->brightness = 2.0f;
    board->display_list = 0u;
    board->texture = 0u;
}

// blur the checkerboard texture at runtime, to accomodate for different resolutions
// if i preblurred it it would be too blurry at high resolutions, for example
static unsigned int sb_checkerboard_build_texture(const sb_checkerboard *board)
{
    const int tex_size = 1024;
    const int tile_count = board->size * 2;
    const double pixels_per_tile = (double)tex_size / (double)tile_count;
    const int radius = 1;

    int dark_val;
    int light_val;
    unsigned char *sharp;
    unsigned char *out;
    unsigned int texture = 0u;
    int x;
    int y;

    dark_val = (int)(board->dark_color[0] * board->brightness * 255.0);
    light_val = (int)(board->light_color[0] * board->brightness * 255.0);
    if (dark_val < 0) {
        dark_val = 0;
    }
    if (dark_val > 255) {
        dark_val = 255;
    }
    if (light_val < 0) {
        light_val = 0;
    }
    if (light_val > 255) {
        light_val = 255;
    }

    sharp = (unsigned char *)malloc((size_t)tex_size * (size_t)tex_size * 3u);
    out = (unsigned char *)malloc((size_t)tex_size * (size_t)tex_size * 3u);
    if (sharp == NULL || out == NULL) {
        sb_log_printf("[main] out of memory building the checkerboard texture");
        free(sharp);
        free(out);
        return 0u;
    }

    for (y = 0; y < tex_size; ++y) {
        int ty = (int)((double)y / pixels_per_tile);
        for (x = 0; x < tex_size; ++x) {
            int tx = (int)((double)x / pixels_per_tile);
            int value = ((tx + ty) & 1) ? light_val : dark_val;
            size_t index = ((size_t)y * (size_t)tex_size + (size_t)x) * 3u;
            sharp[index] = (unsigned char)value;
            sharp[index + 1u] = (unsigned char)value;
            sharp[index + 2u] = (unsigned char)value;
        }
    }

    // blur it just a bit, we dont want it to be too blurry
    for (y = 0; y < tex_size; ++y) {
        for (x = 0; x < tex_size; ++x) {
            int sum = 0;
            int count = 0;
            int dy;
            int dx;
            size_t index;
            int blur_val;
            int value;

            for (dy = -radius; dy <= radius; ++dy) {
                int ny = y + dy;
                if (ny < 0) {
                    ny = 0;
                } else if (ny > tex_size - 1) {
                    ny = tex_size - 1;
                }
                for (dx = -radius; dx <= radius; ++dx) {
                    int nx = x + dx;
                    if (nx < 0) {
                        nx = 0;
                    } else if (nx > tex_size - 1) {
                        nx = tex_size - 1;
                    }
                    sum += sharp[((size_t)ny * (size_t)tex_size + (size_t)nx) * 3u];
                    count += 1;
                }
            }

            index = ((size_t)y * (size_t)tex_size + (size_t)x) * 3u;
            blur_val = sum / count;
            value = (int)((double)sharp[index] * 0.9 + (double)blur_val * 0.1);

            if (value < 0) {
                value = 0;
            } else if (value > 255) {
                value = 255;
            }

            out[index] = (unsigned char)value;
            out[index + 1u] = (unsigned char)value;
            out[index + 2u] = (unsigned char)value;
        }
    }

    glGenTextures(1, &texture);
    if (texture != 0u) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_size, tex_size, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, out);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0u);
    }

    free(sharp);
    free(out);
    return texture;
}

void sb_checkerboard_create_display_list(sb_checkerboard *board)
{
    int size;

    if (board == NULL || board->display_list != 0u) {
        return;
    }

    board->texture = sb_checkerboard_build_texture(board);

    board->display_list = glGenLists(1);
    if (board->display_list == 0u) {
        return;
    }

    glNewList(board->display_list, GL_COMPILE);

    size = board->size;

    if (board->texture != 0u) {
        GLfloat extent = (GLfloat)size;

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, board->texture);
        glNormal3f(0.0f, 1.0f, 0.0f);
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-extent, 0.0f, -extent);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(extent, 0.0f, -extent);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(extent, 0.0f, extent);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-extent, 0.0f, extent);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    } else {
        float dark_r = board->dark_color[0] * board->brightness;
        float dark_g = board->dark_color[1] * board->brightness;
        float dark_b = board->dark_color[2] * board->brightness;
        float light_r = board->light_color[0] * board->brightness;
        float light_g = board->light_color[1] * board->brightness;
        float light_b = board->light_color[2] * board->brightness;
        int x;
        int z;

        glNormal3f(0.0f, 1.0f, 0.0f);
        glBegin(GL_QUADS);
        for (x = -size; x < size; ++x) {
            for (z = -size; z < size; ++z) {
                if ((x + z) & 1) {
                    glColor3f(light_r, light_g, light_b);
                } else {
                    glColor3f(dark_r, dark_g, dark_b);
                }
                glVertex3f((GLfloat)x, 0.0f, (GLfloat)z);
                glVertex3f((GLfloat)x, 0.0f, (GLfloat)(z + 1));
                glVertex3f((GLfloat)(x + 1), 0.0f, (GLfloat)(z + 1));
                glVertex3f((GLfloat)(x + 1), 0.0f, (GLfloat)z);
            }
        }
        glEnd();
    }

    glEndList();

    if (glIsList(board->display_list) == GL_FALSE) {
        sb_log_printf("[main] glNewList failed for the checkerboard");
        glDeleteLists(board->display_list, 1);
        board->display_list = 0u;
    }
}

void sb_checkerboard_draw(const sb_checkerboard *board)
{
    if (board == NULL || board->display_list == 0u) {
        return;
    }

    glPushMatrix();
    glTranslatef(board->position[0], board->position[1], board->position[2]);

    if (board->rotation[0] != 0.0f || board->rotation[1] != 0.0f ||
        board->rotation[2] != 0.0f) {
        glRotatef(board->rotation[0], 1.0f, 0.0f, 0.0f);
        glRotatef(board->rotation[1], 0.0f, 1.0f, 0.0f);
        glRotatef(board->rotation[2], 0.0f, 0.0f, 1.0f);
    }

    glScalef(board->scale[0], board->scale[1], board->scale[2]);
    glCallList(board->display_list);
    glPopMatrix();
}

void sb_checkerboard_cleanup(sb_checkerboard *board)
{
    if (board == NULL) {
        return;
    }

    if (board->display_list != 0u) {
        glDeleteLists(board->display_list, 1);
        board->display_list = 0u;
    }
    if (board->texture != 0u) {
        glDeleteTextures(1, &board->texture);
        board->texture = 0u;
    }
}

static double sb_distance_squared(const double a[3], const float b[3])
{
    double dx = b[0] - a[0];
    double dy = b[1] - a[1];
    double dz = b[2] - a[2];
    return dx * dx + dy * dy + dz * dz;
}

static int sb_pick(const sb_object *objects, size_t count,
                   const double origin[3], const double direction[3],
                   int skip_rotating)
{
    size_t i;
    size_t best = count;
    double best_distance = 0.0;

    if (objects == NULL || count == 0u) {
        return -1;
    }

    for (i = 0; i < count; ++i) {
        double centre[3];
        double distance;

        if (skip_rotating && objects[i].is_rotating) {
            continue;
        }

        centre[0] = (double)objects[i].position[0];
        centre[1] = (double)objects[i].position[1];
        centre[2] = (double)objects[i].position[2];

        if (!sb_raycast_hits_sphere(origin, direction, centre,
                                    (double)objects[i].bounding_radius)) {
            continue;
        }

        distance = sb_distance_squared(origin, objects[i].position);

        if (best == count || distance < best_distance) {
            best = i;
            best_distance = distance;
        }
    }

    return (best == count) ? -1 : (int)best;
}

int sb_pick_hovered(sb_object *objects, size_t count,
                    const double origin[3], const double direction[3],
                    int *hover_sound_requested)
{
    size_t i;
    int hovered;
    int sound_requested = 0;

    if (hover_sound_requested != NULL) {
        *hover_sound_requested = 0;
    }
    if (objects == NULL || count == 0u || origin == NULL || direction == NULL) {
        return -1;
    }

    hovered = sb_pick(objects, count, origin, direction, 1);

    // the rotating cube is also included in the logic despite not being highlighted in red, 
    // saves me from copying a bunch of code into a specific function
    for (i = 0; i < count; ++i) {
        sb_object *object = &objects[i];
        int next = ((int)i == hovered);

        if (next && !object->is_hovered) {
            sound_requested = 1;
        }
        object->is_hovered = next;
    }

    if (hover_sound_requested != NULL) {
        *hover_sound_requested = sound_requested;
    }

    return hovered;
}

int sb_pick_clicked(const sb_object *objects, size_t count,
                    const double origin[3], const double direction[3])
{
    if (objects == NULL || count == 0u || origin == NULL || direction == NULL) {
        return -1;
    }
    return sb_pick(objects, count, origin, direction, 0);
}