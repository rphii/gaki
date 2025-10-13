#include "gaki.h"

static Gaki *state_global;

Gaki *gaki_global_get(void) {
    return state_global;
}

void gaki_global_set(Gaki *st) {
    state_global = st;
}

void gaki_free(Gaki *gaki) {
}
