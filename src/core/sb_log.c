#include "core/sb_log.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#define SB_PATH_MAX 1024

static FILE *g_log = NULL;
static char g_path[SB_PATH_MAX] = {0};
static int g_echo = 0;
static int g_opened_once = 0;

static const char *sb_env(const char *name)
{
    const char *value = getenv(name);
    if (value == NULL || value[0] == '\0') {
        return NULL;
    }
    return value;
}

static int sb_mkdir_one(const char *path)
{
#ifdef _WIN32
    // mingw and (i think) msvc use _mkdir instead of mkdir *sigh*
    if (_mkdir(path) == 0) {
        return 0;
    }
#else
    if (mkdir(path, 0777) == 0) {
        return 0;
    }
#endif
    return (errno == EEXIST) ? 0 : -1;
}

// if only i could call the mkdir from the system, but no. \\ are the devils call and they suck
static int sb_mkdirs(const char *path)
{
    char buf[SB_PATH_MAX];
    size_t len;
    size_t start;
    size_t i;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    len = strlen(path);
    if (len >= sizeof buf) {
        return -1;
    }
    memcpy(buf, path, len + 1);

    while (len > 1u && (buf[len - 1] == '/' || buf[len - 1] == '\\')) {
        buf[--len] = '\0';
    }

    // skip the first couple characters to not try to create / or the drive letter on windows
    // now that i think about it i dont know what that would do. linux would probably treat it as absolute and error out
    // but windows would prob create a folder called C: in the cwd, knowing how fucking stupid it is
    start = 1u;
    if (len >= 3u && buf[1] == ':' && (buf[2] == '/' || buf[2] == '\\')) {
        start = 3u;
    }

    for (i = start; i < len; ++i) {
        char saved;
        if (buf[i] != '/' && buf[i] != '\\') {
            continue;
        }
        saved = buf[i];
        buf[i] = '\0';
        if (sb_mkdir_one(buf) != 0) {
            buf[i] = saved;
            return -1;
        }
        buf[i] = saved;
    }

    return sb_mkdir_one(buf);
}

static void sb_mkdir_parent(const char *path)
{
    char parent[SB_PATH_MAX];
    const char *slash = NULL;
    const char *cursor;

    for (cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') {
            slash = cursor;
        }
    }
    if (slash == NULL) {
        return;
    }

    {
        size_t len = (size_t)(slash - path);
        if (len == 0u || len >= sizeof parent) {
            return;
        }
        memcpy(parent, path, len);
        parent[len] = '\0';
    }

    sb_mkdirs(parent);
}

static void sb_build_log_path(char *out, size_t cap)
{
    const char *home;
    const char *base;
    // behold, the apple logic i didnt test!!11!!
#if defined(__APPLE__)
    home = sb_env("HOME");
    if (home != NULL) {
        snprintf(out, cap, "%s/Library/Logs/SourceBox/sourcebox.log", home);
        return;
    }
#elif defined(_WIN32)
    base = sb_env("LOCALAPPDATA");
    if (base != NULL) {
        snprintf(out, cap, "%s/SourceBox/sourcebox.log", base);
        return;
    }
    home = sb_env("USERPROFILE");
    if (home == NULL) {
        home = sb_env("HOME");
    }
    if (home != NULL) {
        snprintf(out, cap, "%s/AppData/Local/SourceBox/sourcebox.log", home);
        return;
    }
#else
    base = sb_env("XDG_STATE_HOME");
    if (base != NULL) {
        snprintf(out, cap, "%s/sourcebox/sourcebox.log", base);
        return;
    }
    home = sb_env("HOME");
    if (home != NULL) {
        snprintf(out, cap, "%s/.local/state/sourcebox/sourcebox.log", home);
        return;
    }
#endif

    // if all else fails just use the cwd and cry
    snprintf(out, cap, "sourcebox.log");
}

// i know localtime is not thread safe, but i aint threading this shit
// also why the fuck does windows use localtime_s unlike everything else using localtime_r?
static void sb_timestamp(char *out, size_t cap)
{
    time_t now = time(NULL);
    struct tm parts;
    char stamp[32];

#if defined(_WIN32)
    if (localtime_s(&parts, &now) != 0) {
        snprintf(out, cap, "----------");
        return;
    }
#else
    if (localtime_r(&now, &parts) == NULL) {
        snprintf(out, cap, "----------");
        return;
    }
#endif

    if (strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%S", &parts) == 0u) {
        snprintf(out, cap, "----------");
        return;
    }
    snprintf(out, cap, "%s", stamp);
}

int sb_log_init(void)
{
    char quiet[8];

    if (g_opened_once) {
        return (g_log != NULL);
    }
    g_opened_once = 1;

    // output to stdout on linux, but not on windows because why would we have conditional stdout logic...
    quiet[0] = '\0';
    {
        const char *value = sb_env("SOURCEBOX_LOG_QUIET");
        if (value != NULL) {
            snprintf(quiet, sizeof quiet, "1");
        }
    }
    g_echo = (quiet[0] == '\0');

    sb_build_log_path(g_path, sizeof g_path);

    sb_mkdir_parent(g_path);
    g_log = fopen(g_path, "a");
    if (g_log == NULL) {
        g_path[0] = '\0';
        return 0;
    }

    sb_log_raw("");
    sb_log_printf("SourceBox starting");
    sb_log_printf("[diagnostics] %s", g_path);

    return 1;
}

int sb_log_is_open(void)
{
    return g_log != NULL;
}

const char *sb_log_path(void)
{
    return g_path;
}

static void sb_emit(const char *prefix, const char *fmt, va_list args)
{
    va_list copy;
    char message[2048];

    va_copy(copy, args);
    vsnprintf(message, sizeof message, fmt, copy);
    va_end(copy);

    if (g_log != NULL) {
        if (prefix != NULL) {
            fprintf(g_log, "[%s] %s\n", prefix, message);
        } else {
            fprintf(g_log, "%s\n", message);
        }
        fflush(g_log);
    }

    if (g_echo) {
        FILE *sink = (prefix != NULL) ? stderr : stdout;
        if (prefix != NULL) {
            fprintf(sink, "[%s] %s\n", prefix, message);
        } else {
            fprintf(sink, "%s\n", message);
        }
        fflush(sink);
    }
}

void sb_log_printf(const char *fmt, ...)
{
    char stamp[32];
    va_list args;

    if (fmt == NULL) {
        return;
    }

    sb_timestamp(stamp, sizeof stamp);
    va_start(args, fmt);
    sb_emit(stamp, fmt, args);
    va_end(args);
}

void sb_log_raw(const char *fmt, ...)
{
    va_list args;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    sb_emit(NULL, fmt, args);
    va_end(args);
}

void sb_log_close(void)
{
    sb_log_printf("SourceBox exiting");

    if (g_log != NULL) {
        fclose(g_log);
        g_log = NULL;
    }
    g_opened_once = 0;
}