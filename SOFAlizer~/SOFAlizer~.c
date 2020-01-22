/* RT binaural filter: SOFAlizer~		*/
/* based on: 							*/
/* RT binaural filter: earplug~       */
/* Pei Xiang, summer 2004               */
/* Revised in Fall 2006 by Jorge Castellanos */
/* Revised in Spring 2009 by Hans-Christoph Steiner to compile in the data file */

#include <stdio.h>
#include <math.h>
#include "m_pd.h"
#include "SOFAlizer~.h"
#include "mysofa.h"
#ifdef _MSC_VER /* Thes pragmas only apply to Microsoft's compiler */
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif


static t_class *SOFAlizer_tilde_class;

typedef struct _SOFAlizer_tilde
{
    t_object x_obj; 
    t_outlet *left_channel ; 	  /* left output channel */
    t_outlet *right_channel ;     /* right output channel */

    t_float azimuth ;             /* azimuth inlet from 0 to 360 degrees */
    t_float elevation ;           /* elevation inlet from -90 to 90 (degrees) */
    t_float azi ;				  /* azimuth for processing */
    t_float ele ;				  /* elevation for processing */
    
    int M , N;					  /* M is number of impulse responses, N is length of impulse responses */
    struct MYSOFA_EASY *hrtf;	  /* struct to read in SOFA files */
    t_float values[3];			  /* values of cartesian coordinate system */
    t_float rad;				  /* holds calculated radius */
    t_float leftIR[x->N]; 		  /* holds left impulse response */
	t_float rightIR[x->N];		  /* holds right impulse response */

    t_float convBuffer[128];	  /* convolution buffer */
    t_float f ;                   /* dummy float for dsp */
    t_int bufferPin;			  /* indexing in convolution buffer */ 
} t_SOFAlizer_tilde;

static t_int *SOFAlizer_tilde_perform(t_int *w)
{
    t_SOFAlizer_tilde *x = (t_SOFAlizer_tilde *)(w[1]) ;
    t_float *in = (t_float *)(w[2]);
    t_float *right_out = (t_float *)(w[3]);
    t_float *left_out = (t_float *)(w[4]); 
    int blocksize = (int)(w[5]);

    x->azi = x->azimuth ; 
    x->ele = x->elevation ;
		
	int n;
	
	// Find nearest impulse reponses
	float Pi = 3.1415926536;
	x->values[0] = x->rad * cos(Pi/180 * x->ele) * cos(Pi/180 * x->azi);
	x->values[1] = x->rad * cos(Pi/180 * x->ele) * sin(Pi/180 * x->azi);
	x->values[2] = x->rad * sin(Pi/180 * x->ele);
	
	int nearest;
	int size = x->N * x->hrtf->hrtf->R;
	nearest = mysofa_lookup(x->hrtf->lookup, x->values);
	
	for (i = 0; i < x->N; i++) {
		x->leftIR[i] = x->hrtf->hrtf->DataIR.values[nearest * size + i];
		x->rightIR[i] = x->hrtf->hrtf->DataIR.values[nearest * size + x->N + i];
	}

    float inSample;
	float convSum[2]; // to accumulate the sum during convolution.
	
	// Convolution algorithm
	while (blocksize--) {		
		convSum[0] = 0; 
		convSum[1] = 0;	
		inSample = *(in++);
		x->convBuffer[x->bufferPin] = inSample;
			
		for (n = 0; n < 128; n++) { 
			convSum[0] += x->leftIR[n] * x->convBuffer[(x->bufferPin-n) &127];
			convSum[1] += x->rightIR[n] * x->convBuffer[(x->bufferPin-n) &127];
		}	
		  
        *left_out++ = convSum[0];
     	*right_out++ = convSum[1];
     	
     	x->bufferPin = (x->bufferPin + 1) & 127;
    }
	
    return (w+6);
}

static void SOFAlizer_tilde_dsp(t_SOFAlizer_tilde *x, t_signal **sp)
{
		// callback, params, userdata, in_samples, out_L,		out_R,		blocksize.
    dsp_add(SOFAlizer_tilde_perform, 5, x,  sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
}

void SOFAlizer_tilde_free(t_SOFAlizer_tilde *x)
{
  mysofa_close(x->hrtf);
}

static void *SOFAlizer_tilde_new(t_floatarg azimArg, t_floatarg elevArg)
{
	t_SOFAlizer_tilde *x = (t_SOFAlizer_tilde *)pd_new(SOFAlizer_tilde_class);
    x->left_channel = outlet_new(&x->x_obj, gensym("signal"));
    x->right_channel = outlet_new(&x->x_obj, gensym("signal"));
    floatinlet_new(&x->x_obj, &x->azimuth) ;     /* 0 to 360 degrees */
    floatinlet_new(&x->x_obj, &x->elevation) ;   /* -90 to 90 degrees */

    x->azimuth = azimArg ;
    x->elevation = elevArg ;
    
    int i, filter_length, err; 
    
    // Read in SOFA file
    x->hrtf = mysofa_open("mit_kemar_normal_pinna.sofa", 44100, &filter_length, &err);
    if(x->hrtf == NULL)
    return err;
               
    x->M = x->hrtf->hrtf->M;
	x->N = x->hrtf->hrtf->N;
	
	fprintf(stderr, "mit_kemar_normal_pinna.sofa: %u HRTFs, %u samples @ %u Hz. %s, %s, %s.\n",
							x->M, x->N,
							(unsigned int)(x->hrtf->hrtf->DataSamplingRate.values[0]),
							mysofa_getAttribute(x->hrtf->hrtf->attributes, "DatabaseName"),
							mysofa_getAttribute(x->hrtf->hrtf->attributes, "Title"),
							mysofa_getAttribute(x->hrtf->hrtf->attributes, "ListenerShortName"));
	// Check for IR delays
	float delay = 0;
	for (i = 0; i < x->M; i++) {  
		delay = x->hrtf->hrtf->DataDelay.values[i];
		if (delay > 1.0) {
			post(" Warning: This SOFA file will be processed wrong besause of IR delays!");
			fprintf(stderr, " delay = %f", delay);
		}
	}			 
    
    post("        SOFAlizer~: binaural filter with measured reponses\n") ;
    post("        elevation: -90 to 90 degrees. azimuth: 360") ;
	
    for (i = 0; i < x->N; i++) {
		x->leftIR[i] = 0;				  // initialise fir buffers
		x->rightIR[i] = 0;
	}
        
    for (i = 0; i < 128 ; i++) {
         x->convBuffer[i] = 0; 			  // initialise convolution buffer
    }
	
    x->bufferPin = 0;					  // initialise index of signal for convolution
	
	// Calculate radius for first measurement
	for (i = 0; i < 3; i++) {			
		x->values[i] = x->hrtf->hrtf->SourcePosition.values[i];
	}
	x->rad = sqrtf(powf(x->values[0], 2.f) + powf(x->values[1], 2.f) + powf(x->values[2], 2.f));;
	fprintf(stderr, "radius = %f \n", x->rad);

    return (x);
}

void SOFAlizer_tilde_setup(void)
{
    SOFAlizer_tilde_class = class_new(gensym("SOFAlizer~"), (t_newmethod)SOFAlizer_tilde_new, 0,
    	sizeof(t_SOFAlizer_tilde), CLASS_DEFAULT, A_DEFFLOAT, A_DEFFLOAT, 0);
   
    CLASS_MAINSIGNALIN(SOFAlizer_tilde_class, t_SOFAlizer_tilde, f);
   
    class_addmethod(SOFAlizer_tilde_class, (t_method)SOFAlizer_tilde_dsp, gensym("dsp"), 0);
}
