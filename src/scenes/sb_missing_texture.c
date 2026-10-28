#include "scenes/sb_missing_texture.h"

#include "core/sb_log.h"
#include "core/sb_platform.h"
#include "core/sb_resources.h"
#include "gl/glx.h"
#include "gl/sb_text.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SB_MISSING_TEXT \
    "WARNING: NO GRAPHICS DRIVER DETECTED. PLEASE ENABLE A VALID GRAPHICS DRIVER."

#define SB_TEXT_SIZE 36
#define SB_TEXT_SPACING 2

static const char *const SB_FONT_CANDIDATES[] = {
    "assets/fonts/Cabin-Regular.ttf",
    "fonts/Cabin-Regular.ttf",
    "Cabin-Regular.ttf",
    NULL
};

// first of all set up fonts, and if all of them fail then fallback to system default

// on windows, we have Trebuchet, Arial and Verdana
static const char *const SB_SYSTEM_FONT_FILES_WINDOWS[] = {
    "trebuc.ttf",
    "arial.ttf",
    "verdana.ttf",
    NULL
};

// and on linux, DejaVuSans, LiberationSans-Regular, FreeSans and Arial
// different paths because some distros save them elsewhere
// if windows wasnt a bitch i could potentially use fontconfig to auto resolve them :(
static const char *const SB_SYSTEM_FONT_FILES_POSIX[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/TTF/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    "/usr/share/fonts/TTF/freefont/FreeSans.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
    NULL
};

// puts the first font found into out
static int sb_missing_texture_find_system_font(char *out, size_t cap)
{
    size_t i;

    if (sb_platform_is_windows()) {
        const char *windir = getenv("WINDIR");
        char directory[256];

        if (windir == NULL || windir[0] == '\0') {
            windir = "C:\\Windows";
        }

        // truncate the absolute path to %WINDIR% so it fits into the 512 buffer
        snprintf(directory, sizeof directory, "%.200s\\Fonts", windir);

        for (i = 0; SB_SYSTEM_FONT_FILES_WINDOWS[i] != NULL; ++i) {
            char candidate[sizeof directory + 32];
            int written = snprintf(candidate, sizeof candidate, "%.255s\\%.31s",
                                   directory, SB_SYSTEM_FONT_FILES_WINDOWS[i]);

            if (written <= 0 || (size_t)written >= sizeof candidate) {
                continue;
            }
            if (!sb_file_exists(candidate)) {
                continue;
            }
            if ((size_t)written >= cap) {
                return 0;
            }

            memcpy(out, candidate, (size_t)written + 1u);
            return 1;
        }

        return 0;
    }

    for (i = 0; SB_SYSTEM_FONT_FILES_POSIX[i] != NULL; ++i) {
        const char *candidate = SB_SYSTEM_FONT_FILES_POSIX[i];
        size_t len = strlen(candidate);

        if (len + 1u > cap) {
            continue;
        }
        if (!sb_file_exists(candidate)) {
            continue;
        }

        memcpy(out, candidate, len + 1u);
        return 1;
    }

    return 0;
}

static void sb_missing_texture_build_text(sb_missing_texture *scene)
{
    static const unsigned char red[4] = { 255, 0, 0, 255 };
    size_t i;
    int system_font;

    system_font = sb_missing_texture_find_system_font(scene->font_path,
                                                      sizeof scene->font_path);

    for (i = 0; !system_font; ++i) {
        if (SB_FONT_CANDIDATES[i] == NULL) {
            break;
        }
        if (sb_find_resource_one(SB_FONT_CANDIDATES[i], scene->font_path,
                                 sizeof scene->font_path)) {
            break;
        }
    }

    scene->text_texture = sb_text_texture(SB_MISSING_TEXT, scene->font_path,
                                          SB_TEXT_SIZE, red, SB_TEXT_SPACING, 0,
                                          &scene->text_width, &scene->text_height);

    if (scene->text_texture == 0u) {
        sb_log_printf("[missing-texture] warning text failed to render");
        return;
    }

    sb_log_printf("[missing-texture] warning text %dx%d from %s",
                  scene->text_width, scene->text_height, scene->font_path);
}

void sb_missing_texture_init(sb_missing_texture *scene)
{
    if (scene == NULL) {
        return;
    }

    memset(scene, 0, sizeof *scene);
    scene->text_visible = 1;

    // random interval for text flashing
    scene->next_flash_interval = (float)(0.1 + 0.2 * sb_rng_below(3));

    sb_missing_texture_build_text(scene);
}

void sb_missing_texture_create_display_list(sb_missing_texture *scene)
{
    const int size = 50;
    const double block_width = 0.7;
    const double block_height = 0.5;
    int x;
    int z;

    if (scene == NULL || scene->display_list != 0u) {
        return;
    }

    scene->display_list = glGenLists(1);
    if (scene->display_list == 0u) {
        return;
    }

    glNewList(scene->display_list, GL_COMPILE);

    glBegin(GL_QUADS);
    for (x = -size; x < size; ++x) {
        for (z = -size; z < size; ++z) {
            const int corners[4][2] = {
                { x,     z     },
                { x,     z + 1 },
                { x + 1, z + 1 },
                { x + 1, z     }
            };
            int i;

            // apply two's complement to the texture
            if ((int)((unsigned)(x + z) & 1u) != 0) {
                glColor3f(1.0f, 0.0f, 1.0f);
            } else {
                glColor3f(0.0f, 0.0f, 0.0f);
            }

            for (i = 0; i < 4; ++i) {
                double dx = (double)(corners[i][0] - 0);
                double dz = (double)(corners[i][1] - 0);
                double dist = sqrt(dx * dx + dz * dz);
                double push_x = 0.0;
                double push_z = 0.0;

                // keep looping the texture to cover the screen
                if (dist > 0.0) {
                    double push_amount = dist * 0.03;
                    push_x = (dx / dist) * push_amount;
                    push_z = (dz / dist) * push_amount;
                }

                glVertex3f((GLfloat)((double)corners[i][0] * block_width + push_x),
                           (GLfloat)((double)corners[i][1] * block_height + push_z),
                           0.0f);
            }
        }
    }
    glEnd();

    glEndList();

    if (glIsList(scene->display_list) == GL_FALSE) {
        sb_log_printf("[missing-texture] glNewList failed");
        glDeleteLists(scene->display_list, 1);
        scene->display_list = 0u;
    }
}

void sb_missing_texture_update(sb_missing_texture *scene, float dt)
{
    if (scene == NULL) {
        return;
    }

    scene->flash_timer += dt;
    if (scene->flash_timer >= scene->next_flash_interval) {
        scene->text_visible = !scene->text_visible;
        scene->flash_timer = 0.0f;
        // random flash interval
        scene->next_flash_interval = (float)(0.01 + 0.04 * sb_rng_below(3));
    }
}

void sb_missing_texture_draw(const sb_missing_texture *scene,
                             int display_width, int display_height)
{
    if (scene == NULL) {
        return;
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-display_width / 200.0, display_width / 200.0,
            -display_height / 200.0, display_height / 200.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    if (scene->display_list != 0u) {
        glCallList(scene->display_list);
    }

    if (scene->text_visible && scene->text_texture != 0u && scene->text_width > 0) {
        const double ortho_width = display_width / 100.0;
        const double target_width = ortho_width * 0.8;
        const double scale_factor = target_width / (double)scene->text_width;
        double w = (double)scene->text_width * scale_factor;
        double h = (double)scene->text_height * scale_factor;
        double x = -w / 2.0;
        double y = -h / 24.0;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, scene->text_texture);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f((GLfloat)x, (GLfloat)y);
        glTexCoord2f(1.0f, 0.0f); glVertex2f((GLfloat)(x + w), (GLfloat)y);
        glTexCoord2f(1.0f, 1.0f); glVertex2f((GLfloat)(x + w), (GLfloat)(y + h));
        glTexCoord2f(0.0f, 1.0f); glVertex2f((GLfloat)x, (GLfloat)(y + h));
        glEnd();

        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

void sb_missing_texture_cleanup(sb_missing_texture *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->display_list != 0u) {
        glDeleteLists(scene->display_list, 1);
        scene->display_list = 0u;
    }

    // release the texture from the cache so another one can use this slot
    sb_text_release(scene->text_texture);
    scene->text_texture = 0u;
    scene->text_width = 0;
    scene->text_height = 0;
}