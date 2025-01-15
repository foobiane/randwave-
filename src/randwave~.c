/*
 * randwave~: Random single-period waveform generator
 * Created by Ian Doherty, December 2024 - January 2025
 *
 * This Pd external creates random single-period waveforms and plays them back
 * for the user. A random waveform, in this context, is defined as a sequence
 * of randomly-generated sample and amplitude pairs that are connected via
 * trigonometric interpolation. Each random value is generated using a
 * uniform random distribution.
 * 
 * The creation of randwave~ was heavily inspired by Iannis Xenakis' dynamic
 * stochastic synthesis (DSS) procedure, and attempts to emulate the process
 * with more traditional waveforms. Additionally, the code for this project
 * was influenced by Eric Lyon's dynstoch~ external, which implements DSS in
 * Pd.
**/

#include "m_pd.h"

#include <math.h>
#include <stdlib.h>

#define DEFAULT_WAVETABLE_SIZE 2048
#define DEFAULT_NUM_INTERP_POINTS 4

typedef struct _sample_amplitude_pair {
    int sample;
    float amplitude;
} sample_amplitude_pair;

// randwave~ struct
typedef struct _randwave {
    t_object obj;

    // Interpolation points
    sample_amplitude_pair* points; 
    int num_points;

    // Wavetable
    float* wavetable;
    int wavetable_size;
    int phase; // index of wavetable saved between dsp ops

    // Other params
    short generated;
    t_float fsig; // converts signals to float values
} t_randwave;

// Pd method prototypes
void randwave_dsp(t_randwave* x, t_signal** sp);
void randwave_free(t_randwave* x);
void randwave_generate(t_randwave* x, t_floatarg wavetable_size, t_floatarg num_interp_points);
void* randwave_new(t_symbol* s, short argc, t_atom* argv);
t_int* randwave_perform(t_int* w);
void randwave_tilde_setup(void);

// Utility prototypes
void swap(int* a, int* b);
int* fill(int* arr, int n);
int* shuffle(int* arr, int n);
int int_qsort_compar(const void* l, const void* r);
float trig_cardinal(float x, int N);
void trig_interp(float* wavetable, int wavetable_size, sample_amplitude_pair* points, int num_points);

// Class declaration
static t_class* randwave_class;

/////////////////////////////////////////////////////////////////////
// PD FUNCTIONS                                                    //
/////////////////////////////////////////////////////////////////////

void randwave_tilde_setup(void) {
    // Initializing the class structure
    randwave_class = class_new(
        gensym("randwave~"),        // Name
        (t_newmethod) randwave_new, // Constructor
        (t_method) randwave_free,   // Destructor
        sizeof(t_randwave),         // Size
        CLASS_DEFAULT,              // Graphical representation
        A_GIMME,                    // Data input format
        0                           
    );

    // First inlet will control the frequency of the waveform, enabling
    // frequency modulation
    CLASS_MAINSIGNALIN(randwave_class, t_randwave, fsig);

    // Defining class methods
    class_addmethod(randwave_class, (t_method) randwave_dsp, gensym("dsp"), A_CANT, 0);	
    class_addmethod(randwave_class, (t_method) randwave_generate, gensym("generate"), A_FLOAT, A_FLOAT, 0);
}

void* randwave_new(t_symbol* s, short argc, t_atom* argv) {
    t_randwave* x = (t_randwave*) pd_new(randwave_class);
    outlet_new(&x->obj, gensym("signal"));
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("generate"), gensym("generate")); // inlet for generate messages only
    
    x->points = NULL;
    x->num_points = 0;

    x->wavetable = NULL;
    x->wavetable_size = 2048;
    x->phase = 0;

    x->generated = 0;

    return x;
}

void randwave_generate(t_randwave* x, t_floatarg wavetable_size, t_floatarg num_interp_points) {
    post("Generating...");

    // Saving old wavetable size in case it needs to be freed later
    int old_wavetable_size = x->wavetable_size;

    if (wavetable_size < 0) {
        pd_error(x, "randwave~: Invalid sample count for wavetable. Using default (%d) instead.", DEFAULT_WAVETABLE_SIZE);
        x->wavetable_size = DEFAULT_WAVETABLE_SIZE;
    }
    else
        x->wavetable_size = (int) wavetable_size;

    // Initializing wavetable and freeing the old one if present
    if (x->wavetable) { freebytes(x->wavetable, old_wavetable_size * sizeof(float)); }
    x->wavetable = getbytes(x->wavetable_size * sizeof(float));

    // Saving old number of points in case it needs to be freed later
    int old_num_points = x->num_points;

    // User inputs the number of interpolation points, plus 2 for each end
    if (num_interp_points <= 0 || num_interp_points > wavetable_size - 2) {
        pd_error(x, "randwave~: Invalid number of interpolation points. Using default (%d) instead.", DEFAULT_NUM_INTERP_POINTS);
        x->num_points = DEFAULT_NUM_INTERP_POINTS + 2;
    }
    x->num_points = num_interp_points + 2;

    // Initializing points array and freeing old one if present
    if (x->points) { freebytes(x->points, old_num_points * sizeof(sample_amplitude_pair)); }
    x->points = getbytes(x->num_points * sizeof(sample_amplitude_pair));

    // Creating zero crossings for the first and last point
    x->points[0].sample = 0;
    x->points[0].amplitude = 0;
    x->points[x->num_points - 1].sample = x->wavetable_size - 1;
    x->points[x->num_points - 1].amplitude = 0;

    // Generating random sample values by shuffling an array of labelled
    // samples and taking the first num_points ones.

    int all_samples[x->wavetable_size - 2]; // array of all samples in wavetable (not including ends)
    fill(all_samples, x->wavetable_size - 2); // filling array with values 1 thru the wavetable size - 1
    shuffle(all_samples, x->wavetable_size - 2); // shuffling the array
    qsort(all_samples, x->num_points - 2, sizeof(int), int_qsort_compar); // ordering first num_points - 2 random sample vals

    for (int i = 1; i < x->num_points - 1; i++) {
        x->points[i].sample = all_samples[i - 1];
        x->points[i].amplitude = ((float) rand() / (float) RAND_MAX) * 2 - 1; // val between -1 and 1
    }

    // debug
    for (int i = 0; i < x->num_points; i++) {
        post("Point %d: (%d, %f)", i, x->points[i].sample, x->points[i].amplitude);
    }

    // Constructing the waveform by trigonometrically interpolating it and
    // storing it in the wavetable
    trig_interp(x->wavetable, x->wavetable_size, x->points, x->num_points);

    // Marking the waveform as generated
    post("Waveform generated!");
    x->generated = 1;
}

void randwave_dsp(t_randwave* x, t_signal** sp) {
    if (sp[1]->s_sr > 0) {
        // Always ensure that the waveform is generated first before DSP.
        if (!x->generated)
            randwave_generate(x, DEFAULT_WAVETABLE_SIZE, DEFAULT_NUM_INTERP_POINTS);

        dsp_add(randwave_perform, 4, x, sp[0]->s_vec /* Left inlet */, sp[1]->s_vec /* Outlet */, sp[1]->s_n /* Signal vector size */);
    }
}

t_int* randwave_perform(t_int* w) {
    // Obtaining values from signal vector
    t_randwave* x = (t_randwave*) w[1];
    float* frequency = (t_float*) w[2];
    float* output = (t_float*) w[3];
    int n = w[4];

    int i = x->phase; // wavetable index
    int base_sample_increment = ceil((float) x->wavetable_size / (float) n);
    int sample_increment = 0;

    while (n--) {
        // Reading from wavetable
        *output++ = x->wavetable[i];

        // Creating our sample increment by reading from the frequency inlet
        // (if it is being used)
        // TODO: Reading from the frequency inlet gives us very odd values.
        // More testing is needed.
        sample_increment = /*(frequency ? *frequency++)*/ 1 * base_sample_increment;
        i += sample_increment;

        // Adjusting for out-of-bounds values
        while (i >= x->wavetable_size)
            i -= x->wavetable_size;
        while (i < 0)
            i += x->wavetable_size;
    }

    // Saving the index we landed on back into the object
    x->phase = i;

    return w + 5;
}

void randwave_free(t_randwave* x) {
    if (x->points) { freebytes(x->points, x->num_points * sizeof(sample_amplitude_pair)); }
    if (x->wavetable) { freebytes(x->wavetable, x->wavetable_size * sizeof(float)); }
}

/////////////////////////////////////////////////////////////////////
// UTILITY FUNCTIONS                                               //
/////////////////////////////////////////////////////////////////////

// Swaps two int values.
void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

// Fills an int array of size n such that for all i < n, arr[i] = i+1.
int* fill(int* arr, int n) {
    for (int i = 0; i < n; i++)
        arr[i] = i+1;
    
    return arr;
}

// Shuffles an int array of size n.
// Taken from here: https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle
int* shuffle(int* arr, int n) {
    int j = 0;
    for (int i = n - 1; i >= 1; i--) {
        j = rand() % (i + 1);
        swap(&arr[j], &arr[i]);
    }

    return arr;
}

// Comparison function for sorting an int array using qsort.
// See here: https://www.man7.org/linux/man-pages/man3/qsort.3.html
int int_qsort_compar(const void* l, const void* r) {
    return (*(int*) l) - (*(int*) r);
}

// This algorithm was designed based on the one found here:
// https://fncbook.github.io/fnc/trig#cardinal-functions

float trig_cardinal(float x, int N) {
    if (N == 0)
        return 1;
    else if (N % 2 == 1)
        return sin(N * M_PI * x / 2) / (N * sin(M_PI * x / 2));
    else
        return sin(N * M_PI * x / 2) / (N * tan(M_PI * x / 2));
}

void trig_interp(float* wavetable, int wavetable_size, sample_amplitude_pair* points, int num_points) {
    for (int i = 0; i < wavetable_size; i++) {
        float sum = 0;
        float xi = ((float) i / (float) wavetable_size) * (2 * M_PI);

        for (int k = 0; k < num_points; k++) {
            float xk = ((float) points[k].sample / (float) wavetable_size) * (2 * M_PI);
            sum += (points[k].amplitude * trig_cardinal(xi - xk, num_points));
        }

        // Hard limiting any amplitudes that exceed 1 or -1
        if (sum > 1)
            sum = 1;
        else if (sum < -1)
            sum = -1;
        
        wavetable[i] = sum;
    }
}