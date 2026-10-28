#include "core/sb_rng.h"

#include "core/sb_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// rng coding time. i decided to use the PCG32 algorithm. if you are scared of magic numbers do not proceed!!!!

#define PCG32_MULT 6364136223846793005ULL

static uint64_t g_state = 0x853c49e6748fea9bULL;
static uint64_t g_inc = 0xda3e39cb94b95bdbULL;
static uint64_t g_seed = 0u;
static int g_seeded = 0;

static uint32_t sb_rng_step(void)
{
    uint64_t old = g_state;
    uint32_t xorshifted;
    uint32_t rot;

    g_state = old * PCG32_MULT + g_inc;
    xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
}

void sb_rng_seed(uint64_t seed)
{
    g_seed = seed;
    g_state = 0u;
    g_inc = ((seed << 1u) | 1u);
    (void)sb_rng_step();
    g_state += seed ^ 0x9e3779b97f4a7c15ULL;
    (void)sb_rng_step();
    g_seeded = 1;

    sb_log_printf("[rng] seed=%llu (pcg32)",
                  (unsigned long long)g_seed);
}

void sb_rng_seed_from_clock(void)
{
    uint64_t seed = (uint64_t)time(NULL);

    // evil math to make sure that if somehow you launch the program on the 
    // same second twice it doesnt have the same seed
    seed ^= (uint64_t)(uintptr_t)&seed;
    seed <<= 17;
    seed ^= (uint64_t)time(NULL);

    sb_rng_seed(seed);
}

uint64_t sb_rng_current_seed(void)
{
    return g_seed;
}

uint32_t sb_rng_next_u32(void)
{
    if (!g_seeded) {
        sb_rng_seed_from_clock();
    }
    return sb_rng_step();
}

float sb_rng_next_float(void)
{
    // i can use a float to represent this because it is 24 bits exact (the mantissa is 24 bits). Lucky!
    return (float)(sb_rng_next_u32() >> 8) / 16777216.0f;
}

float sb_rng_range(float low, float high)
{
    if (high <= low) {
        return low;
    }
    return low + sb_rng_next_float() * (high - low);
}

int sb_rng_below(int bound)
{
    uint32_t limit;
    uint32_t threshold;
    uint32_t value;

    if (bound <= 1) {
        return 0;
    }

    // some calculation to make the rng not have random spikes
    limit = (uint32_t)bound;
    threshold = (0u - limit) % limit;

    do {
        value = sb_rng_next_u32();
    } while (value < threshold);

    return (int)(value % limit);
}

static int sb_rng_parse_u64(const char *text, uint64_t *out)
{
    char *end = NULL;
    unsigned long long value;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    value = strtoull(text, &end, 0);
    if (end == text || end == NULL || *end != '\0') {
        return 0;
    }

    *out = (uint64_t)value;
    return 1;
}

int sb_rng_apply_command_line(int argc, char **argv)
{
    const char *env;
    uint64_t seed;
    int i;

    if (argc > 0 && argv != NULL) {
        for (i = 0; i + 1 < argc; ++i) {
            if (strcmp(argv[i], "--seed") == 0) {
                if (sb_rng_parse_u64(argv[i + 1], &seed)) {
                    sb_rng_seed(seed);
                    return i + 1;
                }
                sb_log_printf("[rng] ignoring malformed --seed value '%s'", argv[i + 1]);
                return i + 1;
            }
        }
    }

    env = getenv("SOURCEBOX_SEED");
    if (env != NULL && sb_rng_parse_u64(env, &seed)) {
        sb_rng_seed(seed);
        return -1;
    }

    return -1;
}