#ifndef SB_RESOURCES_H
#define SB_RESOURCES_H

#include <stddef.h>

// finds the assets directory using more than one candidate location
// for example, the cwd, the binary's directory or the install location if installed on linux (in /share)

// batch find files from a list of possible routes
int sb_find_resource(const char *const *candidates, size_t count,
                     char *out, size_t cap);

// wrapper to only find 1 asset instead of multiple. again, this is some jank
int sb_find_resource_one(const char *filename, char *out, size_t cap);

// joins two paths together, one absolute and one relative to be exact
int sb_resource_join(const char *root, const char *relative,
                     char *out, size_t cap);

// return the absolute path of the executable, for logging
const char *sb_resource_root(void);

// checks if the search candidates exist, i made it so it would be safe to call more than once
// because there are some loops that use this and it was causing trouble
void sb_resource_init(void);

// check if a file exists, different from resource_init as that checks for if a folder exists
int sb_file_exists(const char *path);

#endif /* SB_RESOURCES_H */