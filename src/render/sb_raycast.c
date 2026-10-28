#include "render/sb_raycast.h"

#include "gl/glglu.h"

#include <math.h>
#include <string.h>

void sb_raycast_update(sb_raycast *raycast)
{
    if (raycast == NULL) {
        return;
    }

    glGetIntegerv(GL_VIEWPORT, raycast->viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, raycast->modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, raycast->projection);

    raycast->ready = 1;
    // recalculate the matrices after updating raycast if something moved
    raycast->cached_valid = 0;
}

int sb_raycast_mouse(sb_raycast *raycast, int mouse_x, int mouse_y,
                     double origin[3], double direction[3])
{
    double near_point[3];
    double far_point[3];
    double dx;
    double dy;
    double dz;
    double length;
    double inv_length;

    if (raycast == NULL || origin == NULL || direction == NULL) {
        return 0;
    }

    if (raycast->cached_valid &&
        raycast->last_mouse_x == mouse_x &&
        raycast->last_mouse_y == mouse_y) {
        memcpy(origin, raycast->origin, sizeof raycast->origin);
        memcpy(direction, raycast->direction, sizeof raycast->direction);
        return 1;
    }

    raycast->cached_valid = 0;
    raycast->last_mouse_x = mouse_x;
    raycast->last_mouse_y = mouse_y;

    if (!raycast->ready) {
        return 0;
    }

    // again, OpenGL has inverted coords to what SDL thinks, so we must check for FALSE
    if (gluUnProject((GLdouble)mouse_x,
                     (GLdouble)(raycast->viewport[3] - mouse_y),
                     0.0,
                     raycast->modelview, raycast->projection, raycast->viewport,
                     &near_point[0], &near_point[1], &near_point[2]) == GL_FALSE) {
        return 0;
    }

    if (gluUnProject((GLdouble)mouse_x,
                     (GLdouble)(raycast->viewport[3] - mouse_y),
                     1.0,
                     raycast->modelview, raycast->projection, raycast->viewport,
                     &far_point[0], &far_point[1], &far_point[2]) == GL_FALSE) {
        return 0;
    }

    dx = far_point[0] - near_point[0];
    dy = far_point[1] - near_point[1];
    dz = far_point[2] - near_point[2];

    length = sqrt(dx * dx + dy * dy + dz * dz);
    if (length < 0.0001) {
        return 0;
    }

    inv_length = 1.0 / length;

    memcpy(raycast->origin, near_point, sizeof raycast->origin);
    raycast->direction[0] = dx * inv_length;
    raycast->direction[1] = dy * inv_length;
    raycast->direction[2] = dz * inv_length;
    raycast->cached_valid = 1;

    memcpy(origin, raycast->origin, sizeof raycast->origin);
    memcpy(direction, raycast->direction, sizeof raycast->direction);
    return 1;
}

int sb_raycast_hits_sphere(const double origin[3], const double direction[3],
                           const double centre[3], double radius)
{
    double oc_x;
    double oc_y;
    double oc_z;
    double a;
    double b;
    double c;
    double discriminant;

    if (origin == NULL || direction == NULL || centre == NULL || radius <= 0.0) {
        return 0;
    }

    oc_x = origin[0] - centre[0];
    oc_y = origin[1] - centre[1];
    oc_z = origin[2] - centre[2];

    a = direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2];
    if (a < 0.0001) {
        return 0;
    }

    b = 2.0 * (oc_x * direction[0] + oc_y * direction[1] + oc_z * direction[2]);
    c = oc_x * oc_x + oc_y * oc_y + oc_z * oc_z - radius * radius;

    discriminant = b * b - 4.0 * a * c;
    return (discriminant >= 0.0) ? 1 : 0;
}