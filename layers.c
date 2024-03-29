#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Include SSE intrinsics
#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#include <immintrin.h>
#include <x86intrin.h>
#endif

// Include OpenMP
#include <omp.h>

#include "layers.h"
#include "volume.h"

conv_layer_t *make_conv_layer(int input_width, int input_height, int input_depth, int filter_width, int num_filters,
                              int stride, int pad) {
    conv_layer_t *l = (conv_layer_t *) malloc(sizeof(conv_layer_t));

    l->output_depth = num_filters;
    l->filter_width = filter_width;
    l->input_depth = input_depth;
    l->input_width = input_width;
    l->input_height = input_height;

    l->filter_height = filter_width;
    l->stride = stride;
    l->pad = pad;

    l->output_width = (input_width + pad * 2 - filter_width) /
                      stride + 1;
    l->output_height = (input_height + pad * 2 - l->filter_height) /
                       stride + 1;

    l->filters = malloc(sizeof(volume_t *) * num_filters);
    for (int i = 0; i < num_filters; i++) {
        l->filters[i] = make_volume(filter_width, l->filter_height,
                                    input_depth, 0.0);
    }

    l->bias = 0.0;
    l->biases = make_volume(1, 1, l->output_depth, l->bias);

    return l;
}

// Performs the forward pass for a convolutional layer by convolving each one
// of the filters with a particular input, and placing the result in the output
// array.
//
// One way to think about convolution in this case is that we have one of the
// layer's filters (a 3D array) that is superimposed on one of the layer's
// inputs (a second 3D array) that has been implicitly padded with zeros. Since
// convolution is a sum of products (described below), we don't actually have
// to add any zeros to the input volume since those terms will not contribute
// to the convolution. Instead, for each position in the filter, we just make
// sure that we are in bounds for the input volume.
//
// Essentially, the filter is "sliding" across the input, in both the x and y
// directions, where we increment our position in each direction by using the
// stride parameter.
//
// At each position, we compute the sum of the elementwise product of the filter
// and the part of the array it's covering. For instance, let's consider a 2D
// case, where the filter (on the left) is superimposed on some part of the
// input (on the right).
//
//   Filter             Input
//  -1  0  1           1  2  3
//  -1  0  1           4  5  6
//  -1  0  1           7  8  9
//
// Here, the sum of the elementwise product is:
//    Filter[0][0] * Input[0][0] + Filter[0][1] * Input[0][1] + ...
//    = -1 * 1 + 0 * 2 + ... + 0 * 8 + 1 * 9
//    = 6
//
// The 3D case is essentially the same, we just have to sum over the other
// dimension as well. Also, since volumes are internally represented as 1D
// arrays, we must use the volume_get and volume_set commands to access elements
// at a coordinate (x, y, d). Finally, we add the corresponding bias for the
// filter to the sum before putting it into the output volume.
void conv_forward(conv_layer_t *l, volume_t **inputs, volume_t **outputs, int start, int end) {
    for (int i = start; i <= end; i++) {
        volume_t *in = inputs[i];
        volume_t *out = outputs[i];
        int out_width = out->width;
        double* out_weights = out->weights;
        int out_depth = out->depth;

        int in_width = in->width;
        double* in_weights = in->weights;
        int in_depth = in->depth;
        int in_height = in->height;

        int stride = l->stride;

        for (int f = 0; f < l->output_depth; f++) {
            volume_t *filter = l->filters[f];
            double* f_weights = filter->weights;
            int f_width = filter->width;
            int f_depth = filter->depth;
            int f_height = filter->height;
            int y = -l->pad;
            for (int out_y = 0; out_y < l->output_height; y += l->stride, out_y++) {
                int x = -l->pad;
                for (int out_x = 0; out_x < l->output_width; x += l->stride, out_x++) {
                    // Take sum of element-wise product
                    double sum = 0.0;

                    double sarray[4];
                    __m256d result = _mm256_setzero_pd();

                    for (int fy = 0; fy < f_height; fy++) {
                        int in_y = y + fy;
                        for (int fx = 0; fx < f_width; fx++) {
                            int in_x = x + fx;
                            if (in_y >= 0 && in_y < in_height && in_x >= 0 && in_x < in_width) {
                                //original
//                                    for (int fd = 0; fd < filter->depth; fd++) {
//                                        sum += volume_get(filter, fx, fy, fd) * volume_get(in, in_x, in_y, fd);
//                                    }

//                                double sarray[4];
//                                __m256d result = _mm256_setzero_pd();

                                if (filter->depth == 3){
                                    __m256d a = _mm256_loadu_pd(in_weights+(((in_width * in_y) + in_x) * in_depth + 0));
                                    __m256d b = _mm256_loadu_pd(f_weights+(((f_width * fy) + fx) * f_depth + 0));
                                    __m256d c = _mm256_mul_pd(a, b);
                                    result = _mm256_add_pd(result, c);

//                                    _mm256_storeu_pd(sarray, result);
//                                    sum += sarray[0] + sarray[1] + sarray[2];

                                }
                                else if (filter->depth == 16){
                                    __m256d a = _mm256_loadu_pd(in_weights+(((in_width * in_y) + in_x) * in_depth + 0));
                                    __m256d b = _mm256_loadu_pd(f_weights+(((f_width * fy) + fx) * f_depth + 0));
                                    __m256d c = _mm256_mul_pd(a, b);
                                    result = _mm256_add_pd(result, c);

                                    a = _mm256_loadu_pd(in_weights+(((in_width * in_y) + in_x) * in_depth + 4));
                                    b = _mm256_loadu_pd(f_weights+(((f_width * fy) + fx) * f_depth + 4));
                                    c = _mm256_mul_pd(a, b);
                                    result = _mm256_add_pd(result, c);

                                    a = _mm256_loadu_pd(in_weights+(((in_width * in_y) + in_x) * in_depth + 8));
                                    b = _mm256_loadu_pd(f_weights+(((f_width * fy) + fx) * f_depth + 8));
                                    c = _mm256_mul_pd(a, b);
                                    result = _mm256_add_pd(result, c);

                                    a = _mm256_loadu_pd(in_weights+(((in_width * in_y) + in_x) * in_depth + 12));
                                    b = _mm256_loadu_pd(f_weights+(((f_width * fy) + fx) * f_depth + 12));
                                    c = _mm256_mul_pd(a, b);
                                    result = _mm256_add_pd(result, c);

//                                    _mm256_storeu_pd(sarray, result);
//                                    sum += sarray[0] + sarray[1] + sarray[2]+sarray[3];
                                }
                                else if (filter->depth == 20){
                                    __m256d a = _mm256_loadu_pd(in_weights+(((in_width * in_y) + in_x) * in_depth + 0));
                                    __m256d b = _mm256_loadu_pd(f_weights+(((f_width * fy) + fx) * f_depth + 0));
                                    __m256d c = _mm256_mul_pd(a, b);
                                    result = _mm256_add_pd(result, c);

                                    a = _mm256_loadu_pd(in_weights+(((in_width * in_y) + in_x) * in_depth + 4));
                                    b = _mm256_loadu_pd(f_weights+(((f_width * fy) + fx) * f_depth + 4));
                                    c = _mm256_mul_pd(a, b);
                                    result = _mm256_add_pd(result, c);

                                    a = _mm256_loadu_pd(in_weights+(((in_width * in_y) + in_x) * in_depth + 8));
                                    b = _mm256_loadu_pd(f_weights+(((f_width * fy) + fx) * f_depth + 8));
                                    c = _mm256_mul_pd(a, b);
                                    result = _mm256_add_pd(result, c);

                                    a = _mm256_loadu_pd(in_weights+(((in_width * in_y) + in_x) * in_depth + 12));
                                    b = _mm256_loadu_pd(f_weights+(((f_width * fy) + fx) * f_depth + 12));
                                    c = _mm256_mul_pd(a, b);
                                    result = _mm256_add_pd(result, c);

                                    a = _mm256_loadu_pd(in_weights+(((in_width * in_y) + in_x) * in_depth + 16));
                                    b = _mm256_loadu_pd(f_weights+(((f_width * fy) + fx) * f_depth + 16));
                                    c = _mm256_mul_pd(a, b);
                                    result = _mm256_add_pd(result, c);

//                                    _mm256_storeu_pd(sarray, result);
//                                    sum += sarray[0] + sarray[1] + sarray[2]+sarray[3];
                                }
                            }
                        }
                    }

                    if (filter->depth == 3) {
                        _mm256_storeu_pd(sarray, result);
                        sum += sarray[0] + sarray[1] + sarray[2];
                    } else {
                        _mm256_storeu_pd(sarray, result);
                        sum += sarray[0] + sarray[1] + sarray[2]+sarray[3];
                    }
//                    double* res = (double*) calloc(4, sizeof(double));
//                    _mm256_storeu_pd(res, result);
//                    sum += res[0] + res[1] + res[2] + res[3];
//                    free(res);

                    sum += l->biases->weights[f];
                    //volume_set(out, out_x, out_y, f, sum);
                    out_weights[((out_width * out_y) + out_x) * out_depth + f] = sum;
                }
            }
        }
    }
}

void conv_load(conv_layer_t *l, const char *file_name) {
    int filter_width, filter_height, depth, filters;

    FILE *fin = fopen(file_name, "r");

    fscanf(fin, "%d %d %d %d", &filter_width, &filter_height, &depth, &filters);
    assert(filter_width == l->filter_width);
    assert(filter_height == l->filter_height);
    assert(depth == l->input_depth);
    assert(filters == l->output_depth);

    for(int f = 0; f < filters; f++) {
        double* weights = l->filters[f]->weights;
        int depth = l->filters[f]->depth;
        int width = l->filters[f]->width;
        for (int x = 0; x < filter_width; x++) {
            for (int y = 0; y < filter_height; y++) {
                for (int d = 0; d < depth; d++) {
                    double val;
                    fscanf(fin, "%lf", &val);
                    //volume_set(l->filters[f], x, y, d, val);
                    weights[((width * y) + x) * depth + d] = val;
                }
            }
        }
    }

    for(int d = 0; d < l->output_depth; d++) {
        double val;
        fscanf(fin, "%lf", &val);
        volume_set(l->biases, 0, 0, d, val);
    }

    fclose(fin);
}

relu_layer_t *make_relu_layer(int input_width, int input_height, int input_depth) {
    relu_layer_t *l = (relu_layer_t *) malloc(sizeof(relu_layer_t));

    l->input_depth = input_depth;
    l->input_width = input_width;
    l->input_height = input_height;

    l->output_width = l->input_width;
    l->output_height = l->input_height;
    l->output_depth = l->input_depth;

    return l;
}

// Applies the Rectifier Linear Unit (ReLU) function to the input, which sets
// output(x, y, d) to max(0.0, input(x, y, d)).
void relu_forward(relu_layer_t *l, volume_t **inputs, volume_t **outputs, int start, int end) {
    int width = l->input_width;
    int height = l->input_height;
    int depth = l->input_depth;
    for (int i = start; i <= end; i++) {
        double * input_weights = inputs[i]->weights;
        int input_width = inputs[i]->width;
        int input_depth = inputs[i]->depth;

        double * output_weights = outputs[i]->weights;
        int output_width = outputs[i]->width;
        int output_depth = outputs[i]->depth;

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                for (int d = 0; d < depth; d++) {
                    double v = input_weights[((input_width * y) + x) * input_depth + d];
                    //double v = volume_get(inputs[i], x, y, d);
                    double value = (v < 0.0) ? 0.0 : v;
                    //volume_set(outputs[i], x, y, d, value);
                    output_weights[((output_width * y) + x) * output_depth + d] = value;
                }
            }
        }
    }
}

pool_layer_t *make_pool_layer(int input_width, int input_height, int input_depth, int pool_width, int stride) {
    pool_layer_t *l = (pool_layer_t *) malloc(sizeof(pool_layer_t));

    l->pool_width = pool_width;
    l->input_depth = input_depth;
    l->input_width = input_width;
    l->input_height = input_height;

    l->pool_height = pool_width;
    l->stride = stride;
    l->pad = 0;

    l->output_depth = input_depth;
    l->output_width = floor((input_width + 0 * 2 - pool_width) / stride + 1);
    l->output_height = floor((input_height + 0 * 2 - l->pool_height) / stride + 1);

    return l;
}

// This is like the convolutional layer in that we are sliding across the input
// volume, but instead of having a filter that we use to find the sum of an
// elementwise product, we instead just output the max value of some part of
// the image. For instance, if we consider a 2D case where the following is the
// part of the input that we are considering:
//
//     1 3 5
//     4 2 1
//     2 2 2
//
// then the value of the corresponding element in the output is 5 (since that
// is the maximum element). This effectively compresses the input.
void pool_forward(pool_layer_t *l, volume_t **inputs, volume_t **outputs, int start, int end) {
    for (int i = start; i <= end; i++) {
        volume_t *in = inputs[i];
        volume_t *out = outputs[i];
        double* in_weights = in->weights;
        int in_height = in->height;
        int in_width = in->width;
        int in_depth = in->depth;
        int output_depth = l->output_depth;
        int output_width = l->output_width;
        int output_height = l->output_height;
        int stride = l->stride;
        int pad = l->pad;
        int pool_width = l->pool_width;
        int pool_height = l->pool_height;

        double* out_weights = out->weights;
        int out_width = out->width;
        int out_depth = out->depth;


        int n = 0;
        for(int d = 0; d < output_depth; d++) {
            int x = -pad;
            for(int out_x = 0; out_x < output_width; x += stride, out_x++) {
                int y = -pad;
                for(int out_y = 0; out_y < output_height; y += stride, out_y++) {

                    double max = -INFINITY;
                    for(int fx = 0; fx < pool_width; fx++) {
                        for(int fy = 0; fy < pool_height; fy++) {
                            int in_y = y + fy;
                            int in_x = x + fx;
                            if(in_x >= 0 && in_x < in_width && in_y >= 0 && in_y < in_height) {
                                double v = in_weights[((in_width * in_y) + in_x) * in_depth + d];
                                //double v = volume_get(in, in_x, in_y, d);
                                if(v > max) {
                                    max = v;
                                }
                            }
                        }
                    }

                    n++;
                    //volume_set(out, out_x, out_y, d, max);
                    out_weights[((out_width * out_y) + out_x) * out_depth + d] = max;

                }
            }
        }
    }
}

fc_layer_t *make_fc_layer(int input_width, int input_height, int input_depth, int num_neurons) {
    fc_layer_t *l = (fc_layer_t *) malloc(sizeof(fc_layer_t));

    l->output_depth = num_neurons;
    l->input_depth = input_depth;
    l->input_width = input_width;
    l->input_height = input_height;

    l->num_inputs = input_width * input_height * input_depth;
    l->output_width = 1;
    l->output_height = 1;

    l->filters = (volume_t **) malloc(sizeof(volume_t *) * num_neurons);
    for (int i = 0; i < l->output_depth; i++) {
        l->filters[i] = make_volume(1, 1, l->num_inputs, 0.0);
    }

    l->bias = 0.0;
    l->biases = make_volume(1, 1, l->output_depth, l->bias);

    return l;
}

// Computes the dot product (i.e. the sum of the elementwise product) of the
// input's weights with each of the filters. Note that these filters are not
// the same as the filters for the convolutional layer.
void fc_forward(fc_layer_t *l, volume_t **inputs, volume_t **outputs, int start, int end) {
    for (int j = start; j <= end; j++) {
        volume_t *in = inputs[j];
        volume_t *out = outputs[j];
        double* in_weights = in->weights;


        for(int i = 0; i < l->output_depth;i++) {
            double dot = 0.0;
            double* fweights = l->filters[i]->weights;
            //original
//            for(int d = 0; d < l->num_inputs; d++) {
//                dot += in->weights[d] * l->filters[i]->weights[d];
//            }
            // Unrolling
            for(int d = 0; d < l->num_inputs/4*4; d+=4){
                dot += in_weights[d] * fweights[d];
                dot += in_weights[d+1] * fweights[d+1];
                dot += in_weights[d+2] * fweights[d+2];
                dot += in_weights[d+3] * fweights[d+3];
            }
            for(int d = l->num_inputs/4*4; d < l->num_inputs; d++){
                dot += in->weights[d] * fweights[d];
            }
            dot += l->biases->weights[i];
            out->weights[i] = dot;
        }
    }
}



void fc_load(fc_layer_t *l, const char *filename) {
    FILE *fin = fopen(filename, "r");

    int num_inputs;
    int output_depth;
    fscanf(fin, "%d %d", &num_inputs, &output_depth);
    assert(output_depth == l->output_depth);
    assert(num_inputs == l->num_inputs);

    for(int i = 0; i < l->output_depth; i++)
        for(int j = 0; j < l->num_inputs; j++) {
            fscanf(fin, "%lf", &(l->filters[i]->weights[j]));
        }

    for(int i = 0; i < l->output_depth; i++) {
        fscanf(fin, "%lf", &(l->biases->weights[i]));
    }

    fclose(fin);
}

softmax_layer_t *make_softmax_layer(int input_width, int input_height, int input_depth) {
    softmax_layer_t *l = (softmax_layer_t*) malloc(sizeof(softmax_layer_t));

    l->input_depth = input_depth;
    l->input_width = input_width;
    l->input_height = input_height;

    l->output_width = 1;
    l->output_height = 1;
    l->output_depth = input_width * input_height * input_depth;

    l->likelihoods = (double*) malloc(sizeof(double) * l->output_depth);

    return l;
}

// This function converts an input's weights array into a probability
// distribution by using the following formula:
//
// likelihood[i] = exp(in->weights[i]) / sum(exp(in->weights))
//
// To increase the numerical stability of taking the exponential of a value, we
// subtract the maximum input weights from each weight before taking the
// exponential. This yields exactly the same results as the expression above,
// but is more resilient to floating point errors.
void softmax_forward(softmax_layer_t *l, volume_t **inputs, volume_t **outputs, int start, int end) {
    double likelihoods[l->output_depth];

    for (int j = start; j <= end; j++) {
        volume_t *in = inputs[j];
        volume_t *out = outputs[j];

        // Compute max activation (used to compute exponentials)
        double amax = in->weights[0];
        for(int i = 1; i < l->output_depth; i++) {
            if (in->weights[i] > amax) {
                amax = in->weights[i];
            }
        }

        // Compute exponentials in a numerically stable way
        double total = 0.0;

        double* in_weights = in->weights;

        for(int i = 0; i < l->output_depth; i++) {
            double e = exp(in_weights[i] - amax);
            total += e;
            likelihoods[i] = e;
        }

        // Normalize and output to sum to one
//        for(int i = 0; i < l->output_depth; i++) {
//            out->weights[i] = likelihoods[i] / total;
//        }
        // Unrolling
        for(int i = 0; i < l->output_depth/4*4; i += 4){
            out->weights[i] = likelihoods[i] / total;
            out->weights[i+1] = likelihoods[i+1] / total;
            out->weights[i+2] = likelihoods[i+2] / total;
            out->weights[i+3] = likelihoods[i+3] / total;
        }
        for (int i = l->output_depth/4*4; i < l->output_depth; i++) {
            out->weights[i] = likelihoods[i] / total;
        }
    }
}