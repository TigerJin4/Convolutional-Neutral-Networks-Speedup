#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

// Include SSE intrinsics
#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#include <immintrin.h>
#include <x86intrin.h>
#endif

// Include OpenMP
#include <omp.h>

#include "volume.h"

inline double volume_get(volume_t *v, int x, int y, int d) {
    return v->weights[((v->width * y) + x) * v->depth + d];
}

inline void volume_set(volume_t *v, int x, int y, int d, double value) {
    v->weights[((v->width * y) + x) * v->depth + d] = value;
}

volume_t *make_volume(int width, int height, int depth, double value) {
    volume_t *new_vol = malloc(sizeof(struct volume));
    new_vol->weights = malloc(sizeof(double) * width * height * depth);

    double* newvol_weights = new_vol->weights;

    new_vol->width = width;
    new_vol->height = height;
    new_vol->depth = depth;

    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {

//            for(int d = 0; d < depth / 4 * 4; d += 4) {
//                __m256d curr = _mm256_set1_pd(value);
//                _mm256_storeu_pd(new_vol->weights+(((new_vol->width * y) + x) * new_vol->depth + d), curr);
//            }
//            for (int d = depth / 4 * 4; d < depth; d++) {
//                volume_set(new_vol, x, y, d, value);
//            }

//            original
//            for (int d = 0; d < depth; d++) {
//                newvol_weights[((width * y) + x) * depth + d] = value;
//                //volume_set(new_vol, x, y, d, value);
//
//            }


//            //Unrolling
            for(int d = 0; d < depth/4 * 4; d += 4){
                newvol_weights[((width * y) + x) * depth + d] = value;
                newvol_weights[((width * y) + x) * depth + d+2] = value;
                newvol_weights[((width * y) + x) * depth + d+3] = value;
                newvol_weights[((width * y) + x) * depth + d+4] = value;
            }
            for (int d = depth/4 * 4; d < depth; d ++){
                newvol_weights[((width * y) + x) * depth + d] = value;
            }
        }
    }

    return new_vol;
}

void copy_volume(volume_t *dest, volume_t *src) {
    assert(dest->width == src->width);
    assert(dest->height == src->height);
    assert(dest->depth == src->depth);

    double * s_weights = src->weights;
    int s_width = src->width;
    int s_depth = src->depth;

    double * d_weights = dest->weights;
    int d_width = dest->width;
    int d_depth = dest->depth;




    for (int x = 0; x < dest->width; x++) {
        for (int y = 0; y < dest->height; y++) {

//            for(int d = 0; d < dest->depth / 4 * 4; d += 4) {
//                __m256d value = _mm256_load_pd(src->weights+(((src->width * y) + x) * src->depth + d));
//                _mm256_store_pd(dest->weights+(((dest->width * y) + x) * dest->depth + d), value);
//            }
//            for (int d = dest->depth / 4 * 4; d < dest->depth; d++) {
//                volume_set(dest, x, y, d, volume_get(src, x, y, d));
//            }

//             original
//            for (int d = 0; d < dest->depth; d++) {
//                //volume_set(dest, x, y, d, s_weights[((s_width * y) + x) * s_depth + d]);
//
//                d_weights[((d_width * y) + x) * d_depth + d] = s_weights[((s_width * y) + x) * s_depth + d];
//
//                //volume_set(dest, x, y, d, volume_get(src, x, y, d));
//            }

            // Unrolling
            for(int d = 0; d < dest->depth/4 * 4; d += 4){
                d_weights[((d_width * y) + x) * d_depth + d] = s_weights[((s_width * y) + x) * s_depth + d];
                d_weights[((d_width * y) + x) * d_depth + d+1] = s_weights[((s_width * y) + x) * s_depth + d+1];
                d_weights[((d_width * y) + x) * d_depth + d+2] = s_weights[((s_width * y) + x) * s_depth + d+2];
                d_weights[((d_width * y) + x) * d_depth + d+3] = s_weights[((s_width * y) + x) * s_depth + d+3];
            }
            for (int d = dest->depth/4 * 4; d < dest->depth; d ++) {
                d_weights[((d_width * y) + x) * d_depth + d] = s_weights[((s_width * y) + x) * s_depth + d];
            }
        }
    }
    
}

void free_volume(volume_t *v) {
    free(v->weights);
    free(v);
}