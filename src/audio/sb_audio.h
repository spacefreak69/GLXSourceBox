#ifndef SB_AUDIO_H
#define SB_AUDIO_H

// this file manages the sound effects and music

typedef struct sb_audio sb_audio;

// returns null when the device doesnt wanna be opened
sb_audio *sb_audio_create(void);

void sb_audio_destroy(sb_audio *audio);

// fuckass way of logging. when they succeed they return 1 instead of 0, and if it returns 0 it failed so it logs
// this also makes it so i HAVE to check every damn function for if the result is 1. 
// when i thought of a better way it was too late to change more than 1k lines of code
int sb_audio_load_sound(sb_audio *audio, const char *name, const char *relative_path);

int sb_audio_load_music(sb_audio *audio, const char *relative_path);

void sb_audio_play_sound(sb_audio *audio, const char *name);

void sb_audio_stop_sound(sb_audio *audio, const char *name);

// returns duration of sound in seconds, 0.0 when it failed as a fallback. this only gets used for logging
// and it is also the reason why i cant use the same function for loading the music (not only is it in mp3 while 
// sounds are wav) but there i MUST know the length (even though it is always the same, so i could have hardcoded that...)
// because then if i want to skip to a section and i dont know how long is it i am FUCKED

double sb_audio_sound_duration(const sb_audio *audio, const char *name);

// pass 0 loops to this function to loop forever
void sb_audio_play_music(sb_audio *audio, int loops, float volume, float start);

void sb_audio_stop_music(sb_audio *audio);

#endif /* SB_AUDIO_H */