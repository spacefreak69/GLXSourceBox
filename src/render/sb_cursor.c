#include "render/sb_cursor.h"

#include "core/sb_log.h"
#include "core/sb_resources.h"
#include "gl/glx.h"
#include "gl/sb_image.h"

#include <stdio.h>
#include <string.h>

void sb_cursor_load(sb_cursor *cursor, const char *fallback_file)
{
    const char *candidates[3];
    char path[1024];
    sb_image image;

    if (cursor == NULL) {
        return;
    }

    cursor->texture = 0u;
    cursor->width = 0;
    cursor->height = 0;
    cursor->enabled = 0;
    cursor->scale = 1.0f;

    candidates[0] = "assets/images/cursor.png";
    candidates[1] = (fallback_file != NULL) ? fallback_file : "cursor.png";
    candidates[2] = "cursor.png";

    if (!sb_find_resource(candidates, 3u, path, sizeof path)) {
        sb_log_printf("[cursor] no cursor loaded, using the system cursor");
        return;
    }

    // flip the cursor for sdl so opengl renders it correctly
    if (!sb_image_load(path, 1, &image)) {
        sb_log_printf("[cursor] error loading cursor from %s", path);
        return;
    }

    glGenTextures(1, &cursor->texture);
    if (cursor->texture == 0u) {
        sb_image_release(&image);
        sb_log_printf("[cursor] error creating cursor texture");
        return;
    }

    glBindTexture(GL_TEXTURE_2D, cursor->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image.pixels);

    cursor->width = image.width;
    cursor->height = image.height;
    cursor->enabled = 1;

    sb_image_release(&image);
    sb_log_printf("[cursor] loaded %s (%dx%d)", path, cursor->width, cursor->height);
}

void sb_cursor_set_scale(sb_cursor *cursor, float scale)
{
    if (cursor == NULL) {
        return;
    }

    if (scale < 1.0f) {
        scale = 1.0f;
    } else if (scale > 2.0f) {
        scale = 2.0f;
    }
    cursor->scale = scale;
}

void sb_cursor_draw(const sb_cursor *cursor, int mouse_x, int mouse_y,
                    int width, int height)
{
    GLfloat w;
    GLfloat h;

    if (cursor == NULL || !cursor->enabled || cursor->texture == 0u) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, (GLdouble)width, (GLdouble)height, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cursor->texture);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    w = (GLfloat)(cursor->width * cursor->scale);
    h = (GLfloat)(cursor->height * cursor->scale);

    glBegin(GL_QUADS);
    // the cursor is technically a 2d in 3d object for opengl
    glTexCoord2f(0.0f, 1.0f); glVertex2f((GLfloat)mouse_x, (GLfloat)mouse_y);
    glTexCoord2f(1.0f, 1.0f); glVertex2f((GLfloat)mouse_x + w, (GLfloat)mouse_y);
    glTexCoord2f(1.0f, 0.0f); glVertex2f((GLfloat)mouse_x + w, (GLfloat)mouse_y + h);
    glTexCoord2f(0.0f, 0.0f); glVertex2f((GLfloat)mouse_x, (GLfloat)mouse_y + h);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

void sb_cursor_cleanup(sb_cursor *cursor)
{
    if (cursor == NULL) {
        return;
    }

    if (cursor->texture != 0u) {
        glDeleteTextures(1, &cursor->texture);
    }
    cursor->texture = 0u;
    cursor->width = 0;
    cursor->height = 0;
    cursor->enabled = 0;
}