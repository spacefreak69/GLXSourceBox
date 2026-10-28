#include "core/sb_platform.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#define SB_PATH_MAX 1024

// dont look to hard at this code, it is shit multi platform code, what do you expect?

static char g_base_dir[SB_PATH_MAX] = {0};
static int g_base_dir_ready = 0;

const char *sb_platform_name(void)
{
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "Darwin";
#elif defined(__linux__)
    return "Linux";
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    return "BSD";
#else
    return "Unknown";
#endif
}

int sb_platform_is_windows(void)
{
#if defined(_WIN32)
    return 1;
#else
    return 0;
#endif
}

int sb_platform_is_linux(void)
{
#if defined(__linux__)
    return 1;
#else
    return 0;
#endif
}

void sb_platform_init(void)
{
    if (g_base_dir_ready) {
        return;
    }
    g_base_dir_ready = 1;

    {
        char *base = SDL_GetBasePath();
        if (base != NULL) {
            size_t len = strlen(base);
            while (len > 0u && (base[len - 1u] == '/' || base[len - 1u] == '\\')) {
                base[--len] = '\0';
            }
            snprintf(g_base_dir, sizeof g_base_dir, "%s", base);
            SDL_free(base);
            if (g_base_dir[0] != '\0') {
                return;
            }
        }
    }

#if defined(__unix__) || defined(__APPLE__)
    {
        char raw[SB_PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", raw, sizeof raw - 1u);
        if (len <= 0) {
            len = readlink("/proc/curproc/file", raw, sizeof raw - 1u);
        }
        if (len > 0) {
            size_t i;
            raw[len] = '\0';
            for (i = (size_t)len; i > 0u; --i) {
                if (raw[i - 1u] == '/') {
                    raw[i - 1u] = '\0';
                    break;
                }
            }
            snprintf(g_base_dir, sizeof g_base_dir, "%s", raw);
        }
    }
#elif defined(_WIN32)
    // if sdl fails to find the cwd, then we ask the OS directly and pray that works. 
    // again, thanks windows for being vibecoded as fuck
#endif
}

//me when i think of renaming a function to a shorter name and then have to fix some issues last minute

const char *sb_platform_base_dir(void)
{
    sb_platform_init();
    return g_base_dir;
}

int sb_platform_desktop_size(sb_size *out)
{
    SDL_Rect bounds;
    int displays;

    if (out == NULL) {
        return 0;
    }

    displays = SDL_GetNumVideoDisplays();
    if (displays < 1) {
        displays = 1;
    }

    SDL_GetDisplayBounds(0, &bounds);
    if (bounds.w <= 0 || bounds.h <= 0) {
        // assume common resolution if all else fails
        out->width = 1920;
        out->height = 1080;
        return 0;
    }

    out->width = bounds.w;
    out->height = bounds.h;
    return 1;
}

float sb_platform_display_scale(int width, int height)
{
    const float base_width = 1920.0f;
    const float base_height = 1080.0f;
    float width_scale;
    float height_scale;

    if (width <= 0 || height <= 0) {
        return 1.0f;
    }

    width_scale = (float)width / base_width;
    height_scale = (float)height / base_height;

    return (width_scale < height_scale) ? width_scale : height_scale;
}

double sb_platform_time(void)
{
    Uint64 freq = SDL_GetPerformanceFrequency();
    if (freq != 0u) {
        return (double)SDL_GetPerformanceCounter() / (double)freq;
    }
    return (double)clock() / (double)CLOCKS_PER_SEC;
}