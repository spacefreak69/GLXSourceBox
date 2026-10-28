#ifndef SB_TEXT_H
#define SB_TEXT_H

#include <stddef.h>

// we use a cached list of 64 entries of text by default
// and then duplicate the array as a last resort when none can be cleared
// this is my own rasterizer btw

// this returns the size of an rgba8 text string as a texture
int sb_text_measure(const char *utf8, const char *font_path, int font_size,
                    const unsigned char rgba[4], int letter_spacing, int bold,
                    int *out_width, int *out_height);

// render the text into a memory buffer
int sb_text_render(const char *utf8, const char *font_path, int font_size,
                   const unsigned char rgba[4], int letter_spacing, int bold,
                   int flip, unsigned char **out_pixels,
                   int *out_width, int *out_height);

// upload texture to gpu or fetch from the cache. if mipmaps is on then also generate those
unsigned int sb_text_texture_flags(const char *utf8, const char *font_path,
                                   int font_size, const unsigned char rgba[4],
                                   int letter_spacing, int bold,
                                   int use_mipmaps, int flip,
                                   int *out_width, int *out_height);

// most used set of options of above function as a short handle, which is flipped and no mipmaps
// flipped because opengl format requirements and why would you have mipmaps on text?
// (that actually caused issues when i didnt realize i was doing that, it caused the text to be hard to read)
unsigned int sb_text_texture(const char *utf8, const char *font_path, int font_size,
                             const unsigned char rgba[4], int letter_spacing, int bold,
                             int *out_width, int *out_height);

// releases a text string from cache, next time we try to upload it will be regenerated if needed
void sb_text_release(unsigned int texture);

// cleanup called when closing the program, deletes the entire texture cache
void sb_text_purge_cache(void);

#endif /* SB_TEXT_H */