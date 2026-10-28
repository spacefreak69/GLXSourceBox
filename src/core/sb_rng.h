#ifndef SB_RNG_H
#define SB_RNG_H

#include <stdint.h>

// rng algorithm used for voidsite. i decided to use PCG32

// generates seed from system clock and logs the value
void sb_rng_seed_from_clock(void);

// set the seed manually (specified using env var or --seed parameter) and log it as well
void sb_rng_seed(uint64_t seed);

uint32_t sb_rng_next_u32(void);

// random between range of 0 and 1
float sb_rng_next_float(void);

// random between range of low and high. i could just replace the above 
// function with a macro or something but i also need to do some maths to check edge cases, *augh*
float sb_rng_range(float low, float high);

// random int between 0 and bound (including 0 but not bound, so [0, bound) )
int sb_rng_below(int bound);

// get the current seed in use
uint64_t sb_rng_current_seed(void);

// parses the --seed argument or reads the env var and sets that seed
int sb_rng_apply_command_line(int argc, char **argv);

#endif /* SB_RNG_H */