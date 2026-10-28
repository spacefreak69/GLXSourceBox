#include "gl/sb_text.h"

#include "core/sb_log.h"
#include "core/sb_resources.h"
#include "gl/glx.h"

#include <SDL_ttf.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// i am using a cache of text entries, which looking back wasnt a good idea specially when 
// dynamically generating text from rng output, but it kept the memory alloc easier to clear, 
// that is what i want to believe

#define SB_TEXT_CACHE_SLOTS 64
#define SB_TEXT_KEY_MAX 2048
#define SB_TEXT_OPAQUE 255u

typedef struct {
    int used;
    // store how many references there are to this texture, do not delete a texture that has at least 1 ref
    unsigned int refs;
    // monotonic clock to check how long ago was this texture used
    unsigned long stamp;
    char key[SB_TEXT_KEY_MAX];
    unsigned int texture;
    int width;
    int height;
} sb_text_cache_entry;

static sb_text_cache_entry *g_cache = NULL;
static size_t g_cache_slots = 0u;
static unsigned long g_stamp = 0ul;
static int g_ready = 0;

typedef struct {
    char path[512];
    int size;
    TTF_Font *font;
} sb_font_entry;

#define SB_FONT_SLOTS 12
static sb_font_entry g_fonts[SB_FONT_SLOTS];

//candidate font paths, i wish i could use fontconfig here but windows doesnt have that :(
static const char *const SB_DEFAULT_FONT_CANDIDATES[] = {
    "assets/fonts/Cabin-Regular.ttf",
    "fonts/Cabin-Regular.ttf",
    "Cabin-Regular.ttf",
    NULL
};

static int sb_path_readable(const char *path)
{
    FILE *file;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

static int sb_path_is_absolute(const char *path)
{
    size_t length;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    if (path[0] == '/' || path[0] == '\\') {
        return 1;
    }

    length = strlen(path);

    // count windows absolute paths as absolute 
    // as without this check C:\\Program Files is treated as relative because it doesnt start with / but with a letter
    if (length >= 3u && path[1] == ':' &&
        (path[2] == '/' || path[2] == '\\')) {
        return 1;
    }
    return 0;
}

// resolve relative path into absolute so that sdl_ttf eats it up
static int sb_text_resolve_font(const char *font_path, char *out, size_t cap)
{
    size_t i;

    out[0] = '\0';

    if (font_path != NULL && font_path[0] != '\0') {
        if (sb_path_is_absolute(font_path)) {
            if (!sb_path_readable(font_path)) {
                return 0;
            }
            snprintf(out, cap, "%s", font_path);
            return 1;
        }
        if (sb_find_resource_one(font_path, out, cap)) {
            return 1;
        }
        if (sb_path_readable(font_path)) {
            snprintf(out, cap, "%s", font_path);
            return 1;
        }
        return 0;
    }

    for (i = 0; i < sizeof SB_DEFAULT_FONT_CANDIDATES / sizeof SB_DEFAULT_FONT_CANDIDATES[0];
         ++i) {
        if (SB_DEFAULT_FONT_CANDIDATES[i] == NULL) {
            break;
        }
        if (sb_find_resource_one(SB_DEFAULT_FONT_CANDIDATES[i], out, cap)) {
            return 1;
        }
    }

    return 0;
}

static TTF_Font *sb_font_acquire(const char *font_path, int font_size)
{
    char resolved[512];
    size_t i;
    size_t free_slot;
    TTF_Font *font;

    if (!g_ready) {
        memset(g_fonts, 0, sizeof g_fonts);
        g_ready = 1;
    }

    if (!sb_text_resolve_font(font_path, resolved, sizeof resolved)) {
        if (font_path != NULL && font_path[0] != '\0') {
            sb_log_printf("[text] font not found: %s", font_path);
        } else {
            sb_log_printf("[text] no default font available; text will not render");
        }
        return NULL;
    }

    for (i = 0; i < SB_FONT_SLOTS; ++i) {
        if (g_fonts[i].font != NULL &&
            g_fonts[i].size == font_size &&
            strcmp(g_fonts[i].path, resolved) == 0) {
            return g_fonts[i].font;
        }
    }

    font = TTF_OpenFont(resolved, font_size);
    if (font == NULL) {
        sb_log_printf("[text] TTF_OpenFont(%s, %d) failed: %s",
                      resolved, font_size, TTF_GetError());
        return NULL;
    }

    free_slot = SB_FONT_SLOTS;
    for (i = 0; i < SB_FONT_SLOTS; ++i) {
        if (g_fonts[i].font == NULL) {
            free_slot = i;
            break;
        }
    }

    if (free_slot == SB_FONT_SLOTS) {
        // if there arent any free slots, best effort use the first one (ala fifo)
        free_slot = 0u;
        TTF_CloseFont(g_fonts[0].font);
    }

    snprintf(g_fonts[free_slot].path, sizeof g_fonts[free_slot].path, "%s", resolved);
    g_fonts[free_slot].size = font_size;
    g_fonts[free_slot].font = font;
    return font;
}

static void sb_font_close_all(void)
{
    size_t i;

    for (i = 0; i < SB_FONT_SLOTS; ++i) {
        if (g_fonts[i].font != NULL) {
            TTF_CloseFont(g_fonts[i].font);
            memset(&g_fonts[i], 0, sizeof g_fonts[i]);
        }
    }
}

// returns the byte length of utf8 text p
static size_t sb_utf8_seq_len(const char *p)
{
    unsigned char lead = (unsigned char)p[0];

    if (lead < 0x80u) {
        return 1u;
    }
    if ((lead & 0xe0u) == 0xc0u) {
        return 2u;
    }
    if ((lead & 0xf0u) == 0xe0u) {
        return 3u;
    }
    if ((lead & 0xf8u) == 0xf0u) {
        return 4u;
    }
    return 1u;
}

// copies some utf8 character into out
static void sb_utf8_glyph(const char *utf8, size_t index, char *out, size_t out_cap)
{
    size_t i = 0u;
    size_t n = 0u;

    while (utf8[i] != '\0') {
        size_t seq = sb_utf8_seq_len(utf8 + i);
        if (n == index) {
            if (seq + 1u > out_cap) {
                seq = out_cap - 1u;
            }
            memcpy(out, utf8 + i, seq);
            out[seq] = '\0';
            return;
        }
        i += seq;
        n += 1u;
    }

    out[0] = '\0';
}

static size_t sb_utf8_count(const char *utf8)
{
    size_t count = 0u;
    size_t i = 0u;

    while (utf8[i] != '\0') {
        i += sb_utf8_seq_len(utf8 + i);
        count += 1u;
    }
    return count;
}

// sdl_ttf uses its own byte layout for fonts and surfaces, 
// so need to convert to rgba32 which is what opengl accepts
static SDL_Surface *sb_surface_to_rgba32(SDL_Surface *surface)
{
    SDL_Surface *converted;

    if (surface == NULL) {
        return NULL;
    }

    converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    if (converted == NULL) {
        sb_log_printf("[text] SDL_ConvertSurfaceFormat failed: %s", SDL_GetError());
    }
    return converted;
}

static void sb_unpremultiply(SDL_Surface *surface)
{
    int x;
    int y;

    if (surface == NULL) {
        return;
    }
    if (surface->format->BytesPerPixel != 4 ||
        surface->format->Rmask != 0x000000ffu ||
        surface->format->Gmask != 0x0000ff00u ||
        surface->format->Bmask != 0x00ff0000u ||
        surface->format->Amask != 0xff000000u) {
        // the layout is weird, leave it as is and hope to god it works
        return;
    }

    for (y = 0; y < surface->h; ++y) {
        unsigned char *row = (unsigned char *)surface->pixels + (size_t)y * surface->pitch;
        for (x = 0; x < surface->w; ++x) {
            unsigned char *px = row + (size_t)x * 4u;
            unsigned int alpha = px[3];
            if (alpha == 0u) {
                px[0] = 0u;
                px[1] = 0u;
                px[2] = 0u;
            } else if (alpha < SB_TEXT_OPAQUE) {
                px[0] = (unsigned char)(((unsigned int)px[0] * SB_TEXT_OPAQUE + alpha / 2u) / alpha);
                px[1] = (unsigned char)(((unsigned int)px[1] * SB_TEXT_OPAQUE + alpha / 2u) / alpha);
                px[2] = (unsigned char)(((unsigned int)px[2] * SB_TEXT_OPAQUE + alpha / 2u) / alpha);
            }
        }
    }
}

// depending on the way sdl_ttf was compiled it produces the alpha channel premultiplied or left as is
// because this is depending on compilation options, we need to detect it at runtime and adapt
static int sb_surface_is_premultiplied(const SDL_Surface *surface)
{
    static int reported = 0;
    int x;
    int y;
    int premultiplied = 1;

    if (surface == NULL || surface->format == NULL) {
        return 0;
    }
    if (surface->format->BytesPerPixel != 4 ||
        surface->format->Rmask != 0x000000ffu ||
        surface->format->Gmask != 0x0000ff00u ||
        surface->format->Bmask != 0x00ff0000u ||
        surface->format->Amask != 0xff000000u) {
        return 0;
    }

    for (y = 0; y < surface->h && premultiplied; ++y) {
        const unsigned char *row = (const unsigned char *)surface->pixels +
                                   (size_t)y * (size_t)surface->pitch;
        for (x = 0; x < surface->w; ++x) {
            const unsigned char *px = row + (size_t)x * 4u;
            unsigned char alpha = px[3];

            if (alpha == 0u || alpha == 255u) {
                continue;
            }
            if (px[0] > alpha || px[1] > alpha || px[2] > alpha) {
                premultiplied = 0;
                break;
            }
        }
    }

    if (!reported) {
        reported = 1;
        sb_log_printf("[text] glyph pixels are %s",
                      premultiplied ? "premultiplied (unpremultiplying)"
                                    : "straight alpha (left as-is)");
    }
    return premultiplied;
}

// this basically blits a surface to screen without blending 
// by overwriting the pixels of surface dst with the ones from surface src
// very hacky workaround
static void sb_overwrite(SDL_Surface *dst, const SDL_Surface *src, int x, int y)
{
    int row;
    size_t span;

    if (dst == NULL || src == NULL) {
        return;
    }
    if (x + src->w > dst->w || y + src->h > dst->h) {
        // clip a character that is wider than expected
        if (x >= dst->w || y >= dst->h) {
            return;
        }
    }

    span = (size_t)(src->w) * 4u;

    for (row = 0; row < src->h; ++row) {
        int dy = y + row;
        if (dy < 0 || dy >= dst->h) {
            continue;
        }
        memcpy((unsigned char *)dst->pixels + (size_t)dy * dst->pitch + (size_t)x * 4u,
               (const unsigned char *)src->pixels + (size_t)row * src->pitch,
               span);
    }
}

static SDL_Surface *sb_surface_new(int width, int height)
{
    SDL_Surface *surface;

    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }

    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                             SDL_PIXELFORMAT_RGBA32);
    if (surface == NULL) {
        sb_log_printf("[text] SDL_CreateRGBSurfaceWithFormat failed: %s", SDL_GetError());
        return NULL;
    }

    SDL_FillRect(surface, NULL, 0u);
    return surface;
}

// manually do letter spacing because sdl_ttf doesnt do that by itself
int sb_text_render(const char *utf8, const char *font_path, int font_size,
                   const unsigned char rgba[4], int letter_spacing, int bold,
                   int flip, unsigned char **out_pixels,
                   int *out_width, int *out_height)
{
    TTF_Font *font;
    SDL_Color colour;
    SDL_Surface *canvas;
    unsigned char *pixels;
    size_t glyphs;
    int width;
    int height;
    int y;

    if (out_pixels != NULL) {
        *out_pixels = NULL;
    }
    if (out_width != NULL) {
        *out_width = 0;
    }
    if (out_height != NULL) {
        *out_height = 0;
    }

    if (utf8 == NULL || utf8[0] == '\0' || rgba == NULL) {
        return 0;
    }

    font = sb_font_acquire(font_path, font_size);
    if (font == NULL) {
        return 0;
    }

    colour.r = rgba[0];
    colour.g = rgba[1];
    colour.b = rgba[2];
    colour.a = rgba[3];

    glyphs = sb_utf8_count(utf8);

    if (letter_spacing != 0 && glyphs > 1u) {
        int *advances;
        size_t n;
        int cursor = 0;

        advances = (int *)calloc(glyphs, sizeof *advances);
        if (advances == NULL) {
            return 0;
        }

        width = 0;
        for (n = 0; n < glyphs; ++n) {
            char glyph[8];
            int w = 1;
            int h = 0;

            sb_utf8_glyph(utf8, n, glyph, sizeof glyph);
            if (TTF_SizeUTF8(font, glyph, &w, &h) != 0 || w < 1) {
                w = 1;
            }
            advances[n] = w;
            width += w;
        }

        width += letter_spacing * (int)(glyphs - 1u);
        if (bold) {
            width += 1;
        }

        height = TTF_FontHeight(font);

        canvas = sb_surface_new(width, height);
        if (canvas == NULL) {
            free(advances);
            return 0;
        }

        for (n = 0; n < glyphs; ++n) {
            char glyph[8];
            SDL_Surface *piece;
            SDL_Surface *rgba;

            sb_utf8_glyph(utf8, n, glyph, sizeof glyph);
            piece = TTF_RenderUTF8_Blended(font, glyph, colour);
            if (piece == NULL) {
                cursor += advances[n] + letter_spacing;
                continue;
            }

            rgba = sb_surface_to_rgba32(piece);
            SDL_FreeSurface(piece);
            if (rgba == NULL) {
                cursor += advances[n] + letter_spacing;
                continue;
            }

            if (sb_surface_is_premultiplied(rgba)) {
                sb_unpremultiply(rgba);
            }
            sb_overwrite(canvas, rgba, cursor, 0);
            if (bold) {
                sb_overwrite(canvas, rgba, cursor + 1, 0);
            }
            SDL_FreeSurface(rgba);

            cursor += advances[n] + letter_spacing;
        }

        free(advances);
    } else {
        SDL_Surface *line = TTF_RenderUTF8_Blended(font, utf8, colour);
        SDL_Surface *rgba;

        if (line == NULL) {
            sb_log_printf("[text] TTF_RenderUTF8_Blended failed: %s", TTF_GetError());
            return 0;
        }

        rgba = sb_surface_to_rgba32(line);
        SDL_FreeSurface(line);
        if (rgba == NULL) {
            return 0;
        }

        if (sb_surface_is_premultiplied(rgba)) {
            sb_unpremultiply(rgba);
        }

        width = rgba->w + (bold ? 1 : 0);
        height = rgba->h;

        canvas = sb_surface_new(width, height);
        if (canvas == NULL) {
            SDL_FreeSurface(rgba);
            return 0;
        }

        sb_overwrite(canvas, rgba, 0, 0);
        if (bold) {
            sb_overwrite(canvas, rgba, 1, 0);
        }
        SDL_FreeSurface(rgba);
    }

    pixels = (unsigned char *)malloc((size_t)canvas->w * (size_t)canvas->h * 4u);
    if (pixels == NULL) {
        SDL_FreeSurface(canvas);
        return 0;
    }

    // invert the rows, as opengl wants bottom to top
    SDL_LockSurface(canvas);
    for (y = 0; y < canvas->h; ++y) {
        int source_row = flip ? (canvas->h - 1 - y) : y;
        const unsigned char *src = (const unsigned char *)canvas->pixels +
                                   (size_t)source_row * (size_t)canvas->pitch;
        memcpy(pixels + (size_t)y * (size_t)canvas->w * 4u,
               src, (size_t)canvas->w * 4u);
    }
    SDL_UnlockSurface(canvas);

    if (out_width != NULL) {
        *out_width = canvas->w;
    }
    if (out_height != NULL) {
        *out_height = canvas->h;
    }

    SDL_FreeSurface(canvas);

    if (out_pixels != NULL) {
        *out_pixels = pixels;
    } else {
        free(pixels);
    }
    return 1;
}

int sb_text_measure(const char *utf8, const char *font_path, int font_size,
                    const unsigned char rgba[4], int letter_spacing, int bold,
                    int *out_width, int *out_height)
{
    TTF_Font *font;
    int w = 0;
    int h = 0;

    if (out_width != NULL) {
        *out_width = 0;
    }
    if (out_height != NULL) {
        *out_height = 0;
    }
    if (utf8 == NULL || utf8[0] == '\0') {
        return 0;
    }

    font = sb_font_acquire(font_path, font_size);
    if (font == NULL) {
        return 0;
    }

    if (TTF_SizeUTF8(font, utf8, &w, &h) != 0) {
        return 0;
    }

    if (bold) {
        w += 1;
    }
    if (letter_spacing != 0 && sb_utf8_count(utf8) > 1u) {
        w += letter_spacing * ((int)sb_utf8_count(utf8) - 1);
    }

    if (out_width != NULL) {
        *out_width = (w > 0) ? w : 1;
    }
    if (out_height != NULL) {
        *out_height = (h > 0) ? h : 1;
    }
    return 1;
}

static unsigned int sb_text_upload(const unsigned char *pixels, int width, int height,
                                   int use_mipmaps)
{
    unsigned int texture = 0u;
    GLint filter = use_mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;

    if (pixels == NULL || width < 1 || height < 1) {
        return 0u;
    }

    glGenTextures(1, &texture);
    if (texture == 0u) {
        return 0u;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    if (use_mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    return texture;
}

// returns 1 when there is a free cache slot to put the texture into, 0 when there is not
static int sb_text_cache_ensure(void)
{
    if (g_cache != NULL) {
        return 1;
    }

    g_cache = (sb_text_cache_entry *)calloc(SB_TEXT_CACHE_SLOTS, sizeof *g_cache);
    if (g_cache == NULL) {
        sb_log_printf("[text] unable to allocate the texture cache");
        return 0;
    }

    g_cache_slots = (size_t)SB_TEXT_CACHE_SLOTS;
    return 1;
}

// hacky workaround for opengl using the wrong texture ids
// if we need to store a new texture duplicate the size of the array
// i can tell that this gonna have a memory leak in an edge case.... shit
static int sb_text_cache_grow(void)
{
    size_t new_slots = (g_cache_slots == 0u) ? (size_t)SB_TEXT_CACHE_SLOTS
                                             : g_cache_slots * 2u;
    sb_text_cache_entry *grown;

    grown = (sb_text_cache_entry *)realloc(g_cache, new_slots * sizeof *grown);
    if (grown == NULL) {
        sb_log_printf("[text] texture cache stuck at %lu entries",
                      (unsigned long)g_cache_slots);
        return 0;
    }

    memset(grown + g_cache_slots, 0, (new_slots - g_cache_slots) * sizeof *grown);
    g_cache = grown;
    g_cache_slots = new_slots;
    return 1;
}

unsigned int sb_text_texture_flags(const char *utf8, const char *font_path,
                                   int font_size, const unsigned char rgba[4],
                                   int letter_spacing, int bold,
                                   int use_mipmaps, int flip,
                                   int *out_width, int *out_height)
{
    char key[SB_TEXT_KEY_MAX];
    unsigned char *pixels = NULL;
    int width = 0;
    int height = 0;
    unsigned int texture;
    size_t victim;
    size_t i;

    if (out_width != NULL) {
        *out_width = 0;
    }
    if (out_height != NULL) {
        *out_height = 0;
    }
    if (utf8 == NULL || utf8[0] == '\0' || rgba == NULL) {
        return 0u;
    }
    if (!sb_text_cache_ensure()) {
        return 0u;
    }

    {
        int written = snprintf(key, sizeof key, "%d|%d|%d|%d|%d|%u,%u,%u,%u|%s|%s",
                               font_size, letter_spacing, bold ? 1 : 0,
                               use_mipmaps ? 1 : 0, flip ? 1 : 0,
                               (unsigned)rgba[0], (unsigned)rgba[1],
                               (unsigned)rgba[2], (unsigned)rgba[3],
                               (font_path != NULL) ? font_path : "",
                               utf8);

        if (written < 0 || (size_t)written >= sizeof key) {
            static int warned = 0;

            if (!warned) {
                warned = 1;
                sb_log_printf("[text] cache key too long, text may render wrongly");
            }
        }
    }

    for (i = 0; i < g_cache_slots; ++i) {
        if (g_cache[i].used && strcmp(g_cache[i].key, key) == 0) {
            g_cache[i].stamp = ++g_stamp;
            g_cache[i].refs += 1u;
            if (out_width != NULL) {
                *out_width = g_cache[i].width;
            }
            if (out_height != NULL) {
                *out_height = g_cache[i].height;
            }
            return g_cache[i].texture;
        }
    }

    if (!sb_text_render(utf8, font_path, font_size, rgba, letter_spacing, bold,
                       flip, &pixels, &width, &height)) {
        return 0u;
    }

    texture = sb_text_upload(pixels, width, height, use_mipmaps);
    free(pixels);

    if (texture == 0u) {
        return 0u;
    }

    // replace a texture from cache following FIFO logic. 
    // if there is no free slot duplicate the size of the cache as a last resort
    victim = g_cache_slots;
    for (i = 0; i < g_cache_slots; ++i) {
        if (!g_cache[i].used) {
            victim = i;
            break;
        }
    }

    if (victim == g_cache_slots) {
        unsigned long oldest = ~0ul;

        for (i = 0; i < g_cache_slots; ++i) {
            if (g_cache[i].refs == 0u && g_cache[i].stamp < oldest) {
                victim = i;
                oldest = g_cache[i].stamp;
            }
        }
    }

    if (victim == g_cache_slots) {
        size_t old_slots = g_cache_slots;

        if (!sb_text_cache_grow()) {
            glDeleteTextures(1, &texture);
            return 0u;
        }
        victim = old_slots;
    } else if (g_cache[victim].used) {
        glDeleteTextures(1, &g_cache[victim].texture);
    }

    g_cache[victim].used = 1;
    g_cache[victim].refs = 1u;
    g_cache[victim].stamp = ++g_stamp;
    snprintf(g_cache[victim].key, sizeof g_cache[victim].key, "%s", key);
    g_cache[victim].texture = texture;
    g_cache[victim].width = width;
    g_cache[victim].height = height;

    if (out_width != NULL) {
        *out_width = width;
    }
    if (out_height != NULL) {
        *out_height = height;
    }
    return texture;
}

unsigned int sb_text_texture(const char *utf8, const char *font_path, int font_size,
                             const unsigned char rgba[4], int letter_spacing, int bold,
                             int *out_width, int *out_height)
{
    return sb_text_texture_flags(utf8, font_path, font_size, rgba, letter_spacing,
                                 bold, 0, 1, out_width, out_height);
}

void sb_text_release(unsigned int texture)
{
    size_t i;

    if (texture == 0u || g_cache == NULL) {
        return;
    }

    for (i = 0; i < g_cache_slots; ++i) {
        if (g_cache[i].used && g_cache[i].texture == texture) {
            if (g_cache[i].refs > 0u) {
                g_cache[i].refs -= 1u;
            }
            return;
        }
    }
}

void sb_text_purge_cache(void)
{
    size_t i;

    if (g_cache != NULL) {
        for (i = 0; i < g_cache_slots; ++i) {
            if (g_cache[i].used) {
                glDeleteTextures(1, &g_cache[i].texture);
            }
        }
        free(g_cache);
        g_cache = NULL;
        g_cache_slots = 0u;
    }

    sb_font_close_all();
    g_ready = 0;
}