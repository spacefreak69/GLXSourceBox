#ifndef SB_RAYCAST_H
#define SB_RAYCAST_H

#include "core/sb_platform.h"
#include "gl/glx.h"

// take the SDL mouse coordinate and turn it into something OpenGL renders upright
// a cursor loaded by SDL by default renders upside down

typedef struct {
    int ready;
    GLint viewport[4];
    GLdouble modelview[16];
    GLdouble projection[16];

    int cached_valid;
    int last_mouse_x;
    int last_mouse_y;
    double origin[3];
    double direction[3];
} sb_raycast;

// update mouse position and correct orientation
void sb_raycast_update(sb_raycast *raycast);

// the raycast is casted inwards, like the mouse is in front of the objects
int sb_raycast_mouse(sb_raycast *raycast, int mouse_x, int mouse_y,
                     double origin[3], double direction[3]);

// simplified sphere collision for the mouse, dont need to get exact face
int sb_raycast_hits_sphere(const double origin[3], const double direction[3],
                           const double centre[3], double radius);

#endif /* SB_RAYCAST_H */