#include "scenes/sb_cone_entities.h"

#include "core/sb_rng.h"
#include "gl/glglu.h"

#include <math.h>

sb_cone_state sb_cone_pick_state(void)
{
    // cone state depends on the rng number we get
    int roll = sb_rng_below(8);

    if (roll < 1) {
        return SB_CONE_STILL;
    }
    if (roll < 5) {
        return SB_CONE_MOVING;
    }
    return SB_CONE_FALLING;
}

void sb_grey_cone_init(sb_grey_cone *cone, const float position[3])
{
    if (cone == NULL) {
        return;
    }

    cone->position[0] = position[0];
    cone->position[1] = position[1];
    cone->position[2] = position[2];

    cone->velocity[0] = 0.0f;
    cone->velocity[1] = 0.0f;
    cone->velocity[2] = 0.0f;

    cone->state = sb_cone_pick_state();

    if (cone->state == SB_CONE_MOVING) {
        cone->velocity[0] = sb_rng_range(-10.0f, 10.0f);
        cone->velocity[1] = sb_rng_range(-10.0f, 10.0f);
        cone->velocity[2] = sb_rng_range(-10.0f, 10.0f);
    } else if (cone->state == SB_CONE_FALLING) {
        cone->velocity[0] = 0.0f;
        cone->velocity[1] = -sb_rng_range(5.0f, 15.0f);
        cone->velocity[2] = 0.0f;
    }

    cone->size = sb_rng_range(0.3f, 3.0f);

    cone->rotation[0] = sb_rng_range(0.0f, 360.0f);
    cone->rotation[1] = sb_rng_range(0.0f, 360.0f);
    cone->rotation[2] = sb_rng_range(0.0f, 360.0f);

    cone->rotates = (sb_rng_next_float() < 0.5f) ? 1 : 0;

    cone->rotation_speed[0] = sb_rng_range(-3.0f, 3.0f);
    cone->rotation_speed[1] = sb_rng_range(-3.0f, 3.0f);
    cone->rotation_speed[2] = sb_rng_range(-3.0f, 3.0f);
}

void sb_grey_cone_update(sb_grey_cone *cone, float dt)
{
    int axis;

    if (cone == NULL) {
        return;
    }

    if (cone->rotates) {
        for (axis = 0; axis < 3; ++axis) {
            float value = cone->rotation[axis] + cone->rotation_speed[axis] * dt;
            // keep sign of divisor so negative numbers exist
            cone->rotation[axis] = (float)fmod((double)value, 360.0);
        }
    }

    if (cone->state == SB_CONE_STILL) {
        return;
    }

    cone->position[0] += cone->velocity[0] * dt;
    cone->position[1] += cone->velocity[1] * dt;
    cone->position[2] += cone->velocity[2] * dt;

    if (cone->state == SB_CONE_FALLING) {
        cone->velocity[1] -= 80.0f * dt;
    }
}

void sb_grey_cone_draw(const sb_grey_cone *cone)
{
    GLfloat axis_length;
    GLUquadric *quadric;

    if (cone == NULL) {
        return;
    }

    glPushMatrix();
    glTranslatef(cone->position[0], cone->position[1], cone->position[2]);
    glRotatef(cone->rotation[0], 1.0f, 0.0f, 0.0f);
    glRotatef(cone->rotation[1], 0.0f, 1.0f, 0.0f);
    glRotatef(cone->rotation[2], 0.0f, 0.0f, 1.0f);

    axis_length = cone->size * 2.5f;
    glLineWidth(2.0f);

    glBegin(GL_LINES);
    glColor3f(1.0f, 0.35f, 0.35f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(axis_length, 0.0f, 0.0f);

    glColor3f(0.35f, 1.0f, 0.35f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, axis_length, 0.0f);

    glColor3f(0.35f, 0.35f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, axis_length);
    glEnd();

    glColor3f(0.5f, 0.5f, 0.5f);
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);

    quadric = gluNewQuadric();
    if (quadric != NULL) {
        gluCylinder(quadric, cone->size * 0.1, 0.0, cone->size * 0.3, 16, 4);
        gluDeleteQuadric(quadric);
    }

    glPopMatrix();
    glPopMatrix();
}