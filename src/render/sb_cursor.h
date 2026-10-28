#ifndef SB_CURSOR_H
#define SB_CURSOR_H

// load an image over the cursor. for opengl it is a quad (2d in 3d object kind of thing)

typedef struct {
    unsigned int texture;
    int width;
    int height;
    int enabled;
    float scale;
} sb_cursor;

// loads the cursor.png file, if it fails use the system cursor as fallback
void sb_cursor_load(sb_cursor *cursor, const char *fallback_file);

// fix scale issues so that minimum scale is 1 and maximum is 2 (clamping)
void sb_cursor_set_scale(sb_cursor *cursor, float scale);

// draw the cursor relative to the resolution
void sb_cursor_draw(const sb_cursor *cursor, int mouse_x, int mouse_y,
                    int width, int height);

void sb_cursor_cleanup(sb_cursor *cursor);

#endif /* SB_CURSOR_H */