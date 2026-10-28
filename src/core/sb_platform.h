#ifndef SB_PLATFORM_H
#define SB_PLATFORM_H

#include <stddef.h>

// os detection and platform specific code. translated to english: horrible spaghetti code

typedef struct {
    int width;
    int height;
} sb_size;

const char *sb_platform_name(void);
int sb_platform_is_windows(void);
int sb_platform_is_linux(void);

// fallback to 1920x1080, if it doesnt fail it returns resolution of first monitor
int sb_platform_desktop_size(sb_size *out);

// returns the display scale (DPi more or less)
float sb_platform_display_scale(int width, int height);

// returns the directory where the executable is, empty string if it failed
const char *sb_platform_base_dir(void);

// simple clock, monotonic clock, just in case of funny clock shenanigans
double sb_platform_time(void);

// i love when i decide to rename a function halfway through 
// development and having to wrap some calls through a tiny ass function. 
// poor planning indeed. if that doesnt scream that i coded this myself and 
// not an LLM or some other bullshit i dont know what will...
void sb_platform_init(void);

#endif /* SB_PLATFORM_H */