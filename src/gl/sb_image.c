#include "gl/sb_image.h"

#include "core/sb_log.h"

#include <SDL.h>
#include <SDL_image.h>

#include <stdlib.h>
#include <string.h>

int sb_image_next_pow2(int value)
{
    int result = 1;

    if (value < 1) {
        return 1;
    }

    while (result < value) {
        result <<= 1;
    }
    return result;
}

int sb_image_load(const char *path, int flip_y, sb_image *out)
{
    SDL_Surface *raw;
    SDL_Surface *converted = NULL;
    SDL_Surface *rgba;
    unsigned char *pixels;
    int width;
    int height;
    int y;

    if (path == NULL || out == NULL) {
        return 0;
    }

    out->pixels = NULL;
    out->width = 0;
    out->height = 0;

    raw = IMG_Load(path);
    if (raw == NULL) {
        sb_log_printf("[image] failed to load %s: %s", path, IMG_GetError());
        return 0;
    }

    rgba = raw;
    if (raw->format->BytesPerPixel != 4 ||
        raw->format->Rmask != 0x000000ffu ||
        raw->format->Gmask != 0x0000ff00u ||
        raw->format->Bmask != 0x00ff0000u ||
        raw->format->Amask != 0xff000000u) {
        converted = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
        if (converted == NULL) {
            sb_log_printf("[image] failed to convert %s to RGBA: %s",
                          path, SDL_GetError());
            SDL_FreeSurface(raw);
            return 0;
        }
        rgba = converted;
    }

    width = rgba->w;
    height = rgba->h;

    pixels = (unsigned char *)malloc((size_t)width * (size_t)height * 4u);
    if (pixels == NULL) {
        sb_log_printf("[image] out of memory loading %s (%dx%d)", path, width, height);
        if (converted != NULL) {
            SDL_FreeSurface(converted);
        }
        SDL_FreeSurface(raw);
        return 0;
    }

    SDL_LockSurface(rgba);
    for (y = 0; y < height; ++y) {
        const unsigned char *src = (const unsigned char *)rgba->pixels + y * rgba->pitch;
        unsigned char *dst = pixels + (size_t)(flip_y ? (height - 1 - y) : y) * (size_t)width * 4u;
        memcpy(dst, src, (size_t)width * 4u);
    }
    SDL_UnlockSurface(rgba);

    if (converted != NULL) {
        SDL_FreeSurface(converted);
    }
    SDL_FreeSurface(raw);

    out->pixels = pixels;
    out->width = width;
    out->height = height;
    return 1;
}

void sb_image_release(sb_image *image)
{
    if (image == NULL) {
        return;
    }

    free(image->pixels);
    image->pixels = NULL;
    image->width = 0;
    image->height = 0;
}