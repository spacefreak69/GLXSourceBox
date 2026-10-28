#ifndef SB_CONE_ENTITIES_H
#define SB_CONE_ENTITIES_H

// voidside grey cone, the one that has the red cone on top
// "grey cone", at least i think it is a grey cone, it would make sense in the ARG

typedef enum {
    SB_CONE_STILL = 0,
    SB_CONE_MOVING,
    SB_CONE_FALLING
} sb_cone_state;

typedef struct {
    float position[3];
    float velocity[3];
    sb_cone_state state;

    float size;
    float rotation[3];
    int rotates;
    float rotation_speed[3];
} sb_grey_cone;

// pick one random state
sb_cone_state sb_cone_pick_state(void);

void sb_grey_cone_init(sb_grey_cone *cone, const float position[3]);

void sb_grey_cone_update(sb_grey_cone *cone, float dt);

void sb_grey_cone_draw(const sb_grey_cone *cone);

#endif /* SB_CONE_ENTITIES_H */