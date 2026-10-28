#include "core/sb_resources.h"

#include "core/sb_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#define SB_PATH_MAX 1024
#define SB_MAX_ROOTS 8

static char g_roots[SB_MAX_ROOTS][SB_PATH_MAX];
static size_t g_root_count = 0;
static int g_roots_ready = 0;

static const char *sb_env(const char *name)
{
    const char *value = getenv(name);
    if (value == NULL || value[0] == '\0') {
        return NULL;
    }
    return value;
}

static void sb_add_root(const char *path)
{
    char normalized[SB_PATH_MAX];
    size_t len;
    size_t i;

    if (path == NULL || path[0] == '\0') {
        return;
    }

    len = strlen(path);
    if (len >= sizeof normalized) {
        return;
    }
    snprintf(normalized, sizeof normalized, "%s", path);

    while (len > 1u && (normalized[len - 1u] == '/' || normalized[len - 1u] == '\\')) {
        normalized[--len] = '\0';
    }

    for (i = 0; i < g_root_count; ++i) {
        if (strcmp(g_roots[i], normalized) == 0) {
            return;
        }
    }

    if (g_root_count >= SB_MAX_ROOTS) {
        return;
    }

    snprintf(g_roots[g_root_count], SB_PATH_MAX, "%s", normalized);
    g_root_count += 1u;
}

// delete the last / or // from the absolute path (a/b/ -> a/b)
static void sb_parent_of(char *path)
{
    size_t len = strlen(path);
    size_t i;

    while (len > 0u && path[len - 1u] != '/' && path[len - 1u] != '\\') {
        --len;
    }
    while (len > 0u && (path[len - 1u] == '/' || path[len - 1u] == '\\')) {
        --len;
    }
    if (len == 0u) {
        return;
    }

    for (i = len; i > 0u; --i) {
        if (path[i - 1u] == '/' || path[i - 1u] == '\\') {
            path[i - 1u] = '\0';
            return;
        }
    }
}

void sb_resource_init(void)
{
    const char *base;

    if (g_roots_ready) {
        return;
    }
    g_roots_ready = 1;

    // if the env var is defined, override everything else
    sb_add_root(sb_env("SOURCEBOX_ROOT"));

    // get the directory of the executable and also check its parent dir because why not
    base = sb_platform_base_dir();
    if (base[0] != '\0') {
        char scratch[SB_PATH_MAX];
        sb_add_root(base);
        snprintf(scratch, sizeof scratch, "%s", base);
        sb_parent_of(scratch);
        sb_add_root(scratch);
    }

    // the cwd of the current tty, as returned by pwd on linux
    {
        char cwd[SB_PATH_MAX];
        if (getcwd(cwd, sizeof cwd) != NULL) {
            sb_add_root(cwd);
        }
    }
}

const char *sb_resource_root(void)
{
    sb_resource_init();
    return (g_root_count > 0u) ? g_roots[0] : "";
}

int sb_resource_join(const char *root, const char *relative,
                     char *out, size_t cap)
{
    const char *prefix;
    size_t prefix_len;
    size_t relative_len;

    if (out == NULL || cap == 0u || relative == NULL) {
        return 0;
    }

    prefix = (root != NULL && root[0] != '\0') ? root : ".";
    prefix_len = strlen(prefix);
    relative_len = strlen(relative);

    //do some maths to check if the path got truncated halfway through, and if so cancel the operation
    if (prefix_len + 1u + relative_len + 1u > cap) {
        out[0] = '\0';
        return 0;
    }

    memcpy(out, prefix, prefix_len);
    out[prefix_len] = '/';
    memcpy(out + prefix_len + 1u, relative, relative_len);
    out[prefix_len + 1u + relative_len] = '\0';
    return 1;
}

int sb_file_exists(const char *path)
{
    struct stat info;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    if (stat(path, &info) != 0) {
        return 0;
    }

    // bitmask to check if the file is a folder or not (in which case it is a file)
    return ((info.st_mode & S_IFMT) != S_IFDIR) ? 1 : 0;
}

int sb_find_resource(const char *const *candidates, size_t count,
                     char *out, size_t cap)
{
    size_t i;
    size_t r;

    if (candidates == NULL || out == NULL || cap == 0u) {
        return 0;
    }

    sb_resource_init();

    // if assets is first found in the executable directoy, then it overrides the cwd of the tty, for example
    for (i = 0; i < count; ++i) {
        char candidate[SB_PATH_MAX];

        if (candidates[i] == NULL) {
            continue;
        }

        // if the path is absolute, use it as is
        if (candidates[i][0] == '/' || candidates[i][0] == '\\' ||
            (candidates[i][0] != '\0' && candidates[i][1] == ':')) {
            snprintf(candidate, sizeof candidate, "%s", candidates[i]);
            if (sb_file_exists(candidate)) {
                snprintf(out, cap, "%s", candidate);
                return 1;
            }
            continue;
        }

        for (r = 0; r < g_root_count; ++r) {
            sb_resource_join(g_roots[r], candidates[i], candidate, sizeof candidate);
            if (sb_file_exists(candidate)) {
                snprintf(out, cap, "%s", candidate);
                return 1;
            }
        }
    }

    out[0] = '\0';
    return 0;
}

int sb_find_resource_one(const char *filename, char *out, size_t cap)
{
    const char *candidates[1];

    candidates[0] = filename;
    return sb_find_resource(candidates, 1u, out, cap);
}