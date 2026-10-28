#include "audio/sb_audio.h"

#include "core/sb_log.h"
#include "core/sb_resources.h"

#include <SDL.h>
#include <SDL_mixer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SB_PATH_MAX 1024
#define SB_MAX_SOUNDS 16

typedef struct {
    char name[32];
    Mix_Chunk *chunk;
    double duration;
} sb_sound_entry;

struct sb_audio {
    sb_sound_entry sounds[SB_MAX_SOUNDS];
    size_t sound_count;
    Mix_Music *music;
    int initialized;
};

static uint32_t sb_read_u32le(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t sb_read_u16le(const unsigned char *p)
{
    return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

// SDL_mixer cannot tell me the duration of a wav file. 
// i could convert the sounds into mp3, but i am a masochist so this is how i do it T-T
// information gathered from online WAVE and RIFF documentation

static double sb_wav_duration_seconds(const char *path)
{
    unsigned char header[12];
    FILE *file;
    double duration = 0.0;
    uint32_t byte_rate = 0u;

    file = fopen(path, "rb");
    if (file == NULL) {
        return 0.0;
    }

    if (fread(header, 1u, sizeof header, file) != sizeof header) {
        fclose(file);
        return 0.0;
    }
    if (memcmp(header, "RIFF", 4u) != 0 || memcmp(header + 8u, "WAVE", 4u) != 0) {
        fclose(file);
        return 0.0;
    }

    for (;;) {
        unsigned char chunk[8];
        uint32_t chunk_size;
        long payload;

        if (fread(chunk, 1u, sizeof chunk, file) != sizeof chunk) {
            break;
        }
        chunk_size = sb_read_u32le(chunk + 4u);
        payload = ftell(file);

        if (memcmp(chunk, "fmt ", 4u) == 0) {
            unsigned char fmt[16];
            if (chunk_size < sizeof fmt || fread(fmt, 1u, sizeof fmt, file) != sizeof fmt) {
                break;
            }
            // if the format tag is not 1, then we are fucked and cannot compute length
            if (sb_read_u16le(fmt) == 1u) {
                byte_rate = sb_read_u32le(fmt + 8u);
            }
        } else if (memcmp(chunk, "data", 4u) == 0) {
            if (byte_rate != 0u) {
                duration = (double)chunk_size / (double)byte_rate;
            }
            break;
        }

        if (fseek(file, payload + (long)chunk_size + (chunk_size & 1u), SEEK_SET) != 0) {
            break;
        }
    }

    fclose(file);
    return duration;
}

sb_audio *sb_audio_create(void)
{
    sb_audio *audio = (sb_audio *)calloc(1u, sizeof *audio);

    if (audio == NULL) {
        sb_log_printf("[audio] out of memory");
        return NULL;
    }

    if (Mix_OpenAudio(44100, AUDIO_S16LSB, 2, 512) != 0) {
        sb_log_printf("[audio] failed to initialize audio: %s", Mix_GetError());
        free(audio);
        return NULL;
    }

    audio->initialized = 1;
    Mix_AllocateChannels(16);
    sb_log_printf("[audio] opened 44100 Hz, 16-bit stereo, 512 frame buffer");
    return audio;
}

void sb_audio_destroy(sb_audio *audio)
{
    size_t i;

    if (audio == NULL) {
        return;
    }

    sb_audio_stop_music(audio);

    for (i = 0; i < audio->sound_count; ++i) {
        if (audio->sounds[i].chunk != NULL) {
            Mix_FreeChunk(audio->sounds[i].chunk);
            audio->sounds[i].chunk = NULL;
        }
    }

    if (audio->initialized) {
        Mix_CloseAudio();
        audio->initialized = 0;
    }

    free(audio);
}

static sb_sound_entry *sb_audio_find(sb_audio *audio, const char *name)
{
    size_t i;

    if (audio == NULL || name == NULL) {
        return NULL;
    }

    for (i = 0; i < audio->sound_count; ++i) {
        if (strcmp(audio->sounds[i].name, name) == 0) {
            return &audio->sounds[i];
        }
    }
    return NULL;
}

// used during cleanup on exit and to free up some memory in some cases
static void sb_audio_forget(sb_audio *audio, const char *name)
{
    size_t i;

    for (i = 0; i < audio->sound_count; ++i) {
        if (strcmp(audio->sounds[i].name, name) != 0) {
            continue;
        }
        if (audio->sounds[i].chunk != NULL) {
            Mix_FreeChunk(audio->sounds[i].chunk);
        }
        if (i + 1u < audio->sound_count) {
            memmove(&audio->sounds[i], &audio->sounds[i + 1u],
                    (audio->sound_count - i - 1u) * sizeof audio->sounds[0]);
        }
        audio->sound_count -= 1u;
        return;
    }
}

int sb_audio_load_sound(sb_audio *audio, const char *name, const char *relative_path)
{
    char path[SB_PATH_MAX];
    sb_sound_entry *slot;

    if (audio == NULL || !audio->initialized || name == NULL) {
        return 0;
    }

    sb_audio_forget(audio, name);

    if (!sb_find_resource_one(relative_path, path, sizeof path)) {
        sb_log_printf("[audio] sound '%s' not found: %s", name, relative_path);
        return 0;
    }

    if (audio->sound_count >= SB_MAX_SOUNDS) {
        sb_log_printf("[audio] sound table full, dropping '%s'", name);
        return 0;
    }

    slot = &audio->sounds[audio->sound_count];
    memset(slot, 0, sizeof *slot);
    snprintf(slot->name, sizeof slot->name, "%s", name);

    slot->chunk = Mix_LoadWAV(path);
    if (slot->chunk == NULL) {
        sb_log_printf("[audio] error loading sound %s: %s", name, Mix_GetError());
        memset(slot, 0, sizeof *slot);
        return 0;
    }

    slot->duration = sb_wav_duration_seconds(path);
    audio->sound_count += 1u;
    sb_log_printf("[audio] loaded sound '%s' (%.3f s)", name, slot->duration);
    return 1;
}

int sb_audio_load_music(sb_audio *audio, const char *relative_path)
{
    char path[SB_PATH_MAX];

    if (audio == NULL || !audio->initialized) {
        return 0;
    }

    if (!sb_find_resource_one(relative_path, path, sizeof path)) {
        sb_log_printf("[audio] music not found: %s", relative_path);
        return 0;
    }

    if (audio->music != NULL) {
        Mix_HaltMusic();
        Mix_FreeMusic(audio->music);
        audio->music = NULL;
    }

    audio->music = Mix_LoadMUS(path);
    if (audio->music == NULL) {
        sb_log_printf("[audio] error loading music: %s", Mix_GetError());
        return 0;
    }

    sb_log_printf("[audio] loaded music (%.3f s)", Mix_MusicDuration(audio->music));
    return 1;
}

// sdl cannot stop an individual chunk, only a channel, so here goes the channel per music file, woohoo
// maybe sdl3 remedied this? i havent looked into it
static int sb_audio_channel_of(const sb_audio *audio, const sb_sound_entry *slot)
{
    if (slot == NULL) {
        return -1;
    }
    return (int)(slot - audio->sounds);
}

void sb_audio_play_sound(sb_audio *audio, const char *name)
{
    sb_sound_entry *slot;
    int channel;

    if (audio == NULL || !audio->initialized) {
        return;
    }

    slot = sb_audio_find(audio, name);
    if (slot == NULL || slot->chunk == NULL) {
        return;
    }

    channel = sb_audio_channel_of(audio, slot);
    if (Mix_PlayChannel(channel, slot->chunk, 0) < 0) {
        sb_log_printf("[audio] failed to start '%s' on channel %d", name, channel);
    }
}

void sb_audio_stop_sound(sb_audio *audio, const char *name)
{
    sb_sound_entry *slot;
    int channel;

    if (audio == NULL || !audio->initialized) {
        return;
    }

    slot = sb_audio_find(audio, name);
    if (slot == NULL) {
        return;
    }

    channel = sb_audio_channel_of(audio, slot);
    if (channel >= 0) {
        Mix_HaltChannel(channel);
    }
}

double sb_audio_sound_duration(const sb_audio *audio, const char *name)
{
    size_t i;

    if (audio == NULL || name == NULL) {
        return 0.0;
    }

    for (i = 0; i < audio->sound_count; ++i) {
        if (strcmp(audio->sounds[i].name, name) == 0) {
            return audio->sounds[i].duration;
        }
    }
    return 0.0;
}

void sb_audio_play_music(sb_audio *audio, int loops, float volume, float start)
{
    if (audio == NULL || !audio->initialized || audio->music == NULL) {
        return;
    }

    if (volume < 0.0f) {
        volume = 0.0f;
    } else if (volume > 1.0f) {
        volume = 1.0f;
    }
    if (start < 0.0f) {
        start = 0.0f;
    }

    Mix_VolumeMusic((int)(volume * 128.0f));

    if (start > 0.0f) {
        // you cant just skip to a position in the music file. i have to stop, seek, and then start the music channel again. 
        // Mix_SetMusicPosition doesnt work while the music isnt halted
        Mix_HaltMusic();
        if (Mix_SetMusicPosition(start) != 0) {
            sb_log_printf("[audio] music seek to %.3f s failed: %s", start, Mix_GetError());
        }
    }

    if (Mix_PlayMusic(audio->music, loops) != 0) {
        sb_log_printf("[audio] Mix_PlayMusic failed: %s", Mix_GetError());
    }
}

void sb_audio_stop_music(sb_audio *audio)
{
    if (audio == NULL || !audio->initialized || audio->music == NULL) {
        return;
    }
    Mix_HaltMusic();
}