#ifndef SB_IMAGE_H
#define SB_IMAGE_H

// image loading. fun fact: opengl reads like a normal person (rgba) but sdl2 exports as bgra, 
// so that was fun to figure out why the damn text was the wrong color :(

typedef struct {
    unsigned char *pixels; //this is rgba8, needs conversion
    int width;
    int height;
} sb_image;

// returns malloc buffer, if flip y is turned on then rows are stored bottom to top 
// instead of top to bottom because opengl wants it bottom to top
int sb_image_load(const char *path, int flip_y, sb_image *out);

void sb_image_release(sb_image *image);

// returns the smallest power of 2 that is greater than or equal to value
int sb_image_next_pow2(int value);

#endif /* SB_IMAGE_H */