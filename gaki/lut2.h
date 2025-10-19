#ifndef LUT2_H

#include <stddef.h>

#define LUT2_ITEM_BY_VAL(T)             T
#define LUT2_ITEM_BY_REF(T)             T *
#define LUT2_ITEM(T, M)                 LUT2_ITEM_##M(T)
#define LUT2_EMPTY                      SIZE_MAX
#define LUT2_CAP(width)                 (!!width * (size_t)1ULL << LUT_SHIFT(width))

#define LUT2_H
#endif

#define LUT2_INCLUDE(N, A, TK, MK, TV, MV) \
    typedef struct N##KV { \
        LUT2_ITEM(TK, MK) key; \
        LUT2_ITEM(TV, MV) val; \
        size_t hash; /* !!! IMPORTANT !!! do NOT edit externally */ \
    } N##_KV, *N##_KVs; \
    \
    typedef struct N { \
        N##_KV **buckets; \
        size_t used; \
        uint8_t width; \
    } N; \
    \
    size_t A##_hash(LUT_ITEM(TK, MK) v); \
    int A##_cmp(LUT_ITEM(TK, MK) a, LUT_ITEM(TK, MK) b); \
    void A##_free_k(LUT_ITEM(TK, MK)); \
    void A##_free_v(LUT_ITEM(TV, MV)); \
    void A##_grow(N *lut, uint8_t width_new); \
    /**/

#define LUT2_IMPLEMENT_HASH(N, A, TK, MK, TV, MV, H, C, FK, FV) \
    size_t A##_hash(LUT_ITEM(TK, MK) val) { return H(val) % LUT2_EMPTY; }

#define LUT2_IMPLEMENT_CMP(N, A, TK, MK, TV, MV, H, C, FK, FV) \
    int A##_cmp(LUT_ITEM(TK, MK) a, LUT_ITEM(TK, MK) b) { return C(a, b); }

#define LUT2_IMPLEMENT_FREE_K(N, A, TK, MK, TV, MV, H, C, FK, FV) \
    void A##_free_k(LUT_ITEM(TK, MK) key) { FK(key); }

#define LUT2_IMPLEMENT_FREE_V(N, A, TK, MK, TV, MV, H, C, FK, FV) \
    void A##_free_v(LUT_ITEM(TV, MV) val) { FV(val); }

#define LUT2_IMPLEMENT_COMMON_STATIC_GET_ITEM(N, A, TK, MK, TV, MV, H, C, FK, FV) \
    static N##_KV **A##_static_get_item(N *lut, const LUT_ITEM(TK, MK) key, size_t hash, bool intend_to_set) { \
        LUT_ASSERT_ARG(lut); \
        size_t perturb = hash >> 5; \
        size_t mask = ~(SIZE_MAX << lut->width); \
        size_t i = mask & hash; \
        N##KV **item = &lut->buckets[i]; \
        for(;;) { \
            /*printff("  %zu", i);*/\
            if(!*item) break; \
            if(intend_to_set && (*item)->hash == LUT_EMPTY) break; \
            if((*item)->hash == hash) { \
                if(C != 0) { if(!LUT_TYPE_CMP(C, (*item)->key, key, TK, MK)) return item; } \
                else { if(!memcmp(LUT_REF(MK)(*item)->key, LUT_REF(MK)key, sizeof(*LUT_REF(MK)key))) return item; } \
            } \
            perturb >>= 5; \
            i = mask & (i * 5 + perturb + 1); \
            /* get NEXT item */ \
            item = &lut->buckets[i]; \
        } \
        return item; \
    }

#define LUT2_IMPLEMENT_GROW(N, A, TK, MK, TV, MV, H, C, FK, FV) \
    void A##_grow(N *lut, uint8_t width_new) { \
        ASSERT_ARG(lut); \
        if(width_new <= lut->width) return; \
        N grown = {0}; \
        size_t cap_new = LUT2_CAP(width_new); \
        grown.buckets = malloc(sizeof(*grown.buckets) * cap_new); \
        grown.width = width_new; \
        grown.used = lut->used; \
        memset(grown.buckets, 0, sizeof(*grown.buckets) * cap_new); \
        for(size_t i = 0; i < cap_new; ++i) { \
            N##_KV *src = lut->buckets[i]; \
            if(!src) continue; \
            if(src->hash == LUT2_EMPTY) { \
                free(src); \
                continue; \
            } \
            size_t hash = src->hash; \
            N##_KV **item = A##_static_get_item(&grown, src->key, hash, true); \
            *item = src; \
        } \
        if(lut->buckets) { \
            free(lut->buckets); \
        } \
        *lut = grown; \
        return 0; \
    }

#define LUT2_IMPLEMENT_ONCE(N, A, TK, MK, TV, MV, H, C, FK, FV) \
    N##_KV A##_once(N *lut, LUT_ITEM(TK, MK) key, LUT_ITEM(TV, MV) val) { \
        FV(val); \
    }

#define LUT2_IMPLEMENT(N, A, TK, MK, TV, MV, H, C, FK, FV) \
    LUT2_IMPLEMENT_HASH(N, A, TK, MK, TV, MV, H, C, FK, FV) \
    LUT2_IMPLEMENT_CMP(N, A, TK, MK, TV, MV, H, C, FK, FV) \
    LUT2_IMPLEMENT_FREE_K(N, A, TK, MK, TV, MV, H, C, FK, FV) \
    LUT2_IMPLEMENT_FREE_V(N, A, TK, MK, TV, MV, H, C, FK, FV) \
    LUT2_IMPLEMENT_GROW(N, A, TK, MK, TV, MV, H, C, FK, FV) \
    /**/

