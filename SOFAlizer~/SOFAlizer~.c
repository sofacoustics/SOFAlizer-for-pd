/* RT binaural filter: SOFAlizer~		*/
/* based on: 							*/
/* RT binaural filter: earplug~       */
/* Pei Xiang, summer 2004               */
/* Revised in Fall 2006 by Jorge Castellanos */
/* Revised in Spring 2009 by Hans-Christoph Steiner to compile in the data file */

#include <stdio.h>
#include <math.h>
#include <string.h>
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
    
    char filename[1000];          /* argument filename */
    t_int M , N;				  /* M is number of impulse responses, N is length of impulse responses */
    struct MYSOFA_EASY *hrtf;	  /* struct to read in SOFA files */
    t_float values[3];			  /* values of cartesian coordinate system */
    t_float rad;				  /* holds calculated radius */
    t_float leftIR[128]; 		  /* holds left impulse response */
	t_float rightIR[128];		  /* holds right impulse response */

    t_float convBuffer1[128];	  /* convolution buffer */
    t_float convBuffer2[128];	  /* convolution buffer */
    t_float f ;                   /* dummy float for dsp */
    t_int bufferPin;			  /* indexing in convolution buffer */ 
} t_SOFAlizer_tilde;

static t_int *SOFAlizer_tilde_perform(t_int *w)
{
    t_SOFAlizer_tilde *x = (t_SOFAlizer_tilde *)(w[1]) ;
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *right_out = (t_float *)(w[4]);
    t_float *left_out = (t_float *)(w[5]); 
    int blocksize = (int)(w[6]);

    x->azi = x->azimuth ; 
    x->ele = x->elevation ;
		
	int i, n;
	
	// Find nearest impulse reponses
	float Pi = 3.1415926536;
	x->values[0] = x->rad * cos(Pi/180 * x->ele) * cos(Pi/180 * x->azi);
	x->values[1] = x->rad * cos(Pi/180 * x->ele) * sin(Pi/180 * x->azi);
	x->values[2] = x->rad * sin(Pi/180 * x->ele);
	
	int nearest, cnt = 0;
	int size = x->N * x->hrtf->hrtf->R;
	nearest = mysofa_lookup(x->hrtf->lookup, x->values);
	if (cnt > 3000){ post("index = %u", nearest); cnt = 0; };
	cnt+=1;
	
	for (i = 0; i < 128; i++) {
		x->leftIR[i] = x->hrtf->hrtf->DataIR.values[nearest * size + i];
		x->rightIR[i] = x->hrtf->hrtf->DataIR.values[nearest * size + x->N + i];
	}

    float inSample1, inSample2;
	float convSum1[2]; // to accumulate the sum during convolution.
	float convSum2[2];
	
	// Convolution algorithm
	while (blocksize--) {		
		convSum1[0] = 0; 
		convSum1[1] = 0;
		convSum2[0] = 0; 
		convSum2[1] = 0;	
		inSample1 = *(in1++);
		inSample2 = *(in2++);
		x->convBuffer1[x->bufferPin] = inSample1; 
		x->convBuffer2[x->bufferPin] = inSample2;
			
		for (n = 0; n < 128; n++) { 
			convSum1[0] += x->leftIR[n] * x->convBuffer1[(x->bufferPin-n) &127];
			convSum1[1] += x->rightIR[n] * x->convBuffer1[(x->bufferPin-n) &127];
			convSum2[0] += x->leftIR[n] * x->convBuffer2[(x->bufferPin-n) &127];
			convSum2[1] += x->rightIR[n] * x->convBuffer2[(x->bufferPin-n) &127];
		}	
		
		if (inSample2 != 0) {
			*left_out++ = convSum1[0]  + convSum2[0];
			*right_out++ = convSum1[1] + convSum2[1];
		}
		else {
			*left_out++ = convSum1[0];
			*right_out++ = convSum1[1];
		}
     	
     	x->bufferPin = (x->bufferPin + 1) & 127;
    }
	
    return (w+7);
}

static void SOFAlizer_tilde_dsp(t_SOFAlizer_tilde *x, t_signal **sp)
{
		// callback, params, userdata, in_samples, out_L,		out_R,		blocksize.
    dsp_add(SOFAlizer_tilde_perform, 6, x,  sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[0]->s_n);
}

void SOFAlizer_tilde_free(t_SOFAlizer_tilde *x)
{
  mysofa_close(x->hrtf);
}

static void *SOFAlizer_tilde_new(t_symbol *filenameArg, t_floatarg azimArg, t_floatarg elevArg)
{
	t_SOFAlizer_tilde *x = (t_SOFAlizer_tilde *)pd_new(SOFAlizer_tilde_class);
	strcpy(x->filename, filenameArg->s_name);
	if (x->filename[0] == '\0') {
		error("No SOFA file specified");
		return 0;
	}
	else {
		post("SOFA file %s will be opened..", x->filename);
	}
    x->left_channel = outlet_new(&x->x_obj, gensym("signal"));
    x->right_channel = outlet_new(&x->x_obj, gensym("signal"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal,&s_signal); //audio input 2
    floatinlet_new(&x->x_obj, &x->azimuth) ;     /* 0 to 360 degrees */
    floatinlet_new(&x->x_obj, &x->elevation) ;   /* -90 to 90 degrees */
	
    x->azimuth = azimArg ;
    x->elevation = elevArg ;
    
    int i, filter_length, err; 
    
    // Read in SOFA file
    x->hrtf = mysofa_open(x->filename, 44100, &filter_length, &err);
    if(x->hrtf == NULL) {
		error("SOFA file couldn't be opened.");
		return 0;
	}
	       
    x->M = x->hrtf->hrtf->M;
	x->N = x->hrtf->hrtf->N;
	
	post("Metadata: %s %u HRTFs, %u samples @ %u Hz. %s, %s, %s.\n",
							x->filename,(unsigned int)x->M, (unsigned int)x->N,
							(unsigned int)(x->hrtf->hrtf->DataSamplingRate.values[0]),
							mysofa_getAttribute(x->hrtf->hrtf->attributes, "DatabaseName"),
							mysofa_getAttribute(x->hrtf->hrtf->attributes, "Title"),
							mysofa_getAttribute(x->hrtf->hrtf->attributes, "ListenerShortName"));
	// Check for IR delays
	float delay = 0;
	int warn = 0;
	for (i = 0; i < 2*x->M; i++) {  
		delay = x->hrtf->hrtf->DataDelay.values[i];
		if (delay > 1.0) {
			warn = 1;
			fprintf(stderr, " delay = %f", delay);
		}
	}	
	if (warn == 1){ error(" Warning: This SOFA file will be processed wrong besause of IR delays!"); }		 
    
    post("        SOFAlizer~: binaural filter with measured reponses\n") ;
    post("        elevation: -90 to 90 degrees. azimuth: 360") ;
	
    for (i = 0; i < 128; i++) {
		x->leftIR[i] = 0;				  // initialise fir buffers
		x->rightIR[i] = 0;
	}
        
    for (i = 0; i < 128 ; i++) {
         x->convBuffer1[i] = 0; 			  // initialise convolution buffer 1
         x->convBuffer2[i] = 0; 			  // initialise convolution buffer 2
    }
	
    x->bufferPin = 0;					  // initialise index of signal for convolution
	
	// Calculate radius for first measurement
	for (i = 0; i < 3; i++) {			
		x->values[i] = x->hrtf->hrtf->SourcePosition.values[i];
	}
	x->rad = sqrtf(powf(x->values[0], 2.f) + powf(x->values[1], 2.f) + powf(x->values[2], 2.f));;
	post("Radius is %f m. \n", x->rad);

    return (x);
}

void SOFAlizer_tilde_setup(void)
{
    SOFAlizer_tilde_class = class_new(gensym("SOFAlizer~"), (t_newmethod)SOFAlizer_tilde_new,
     	(t_method)SOFAlizer_tilde_free, sizeof(t_SOFAlizer_tilde), CLASS_DEFAULT,
     	 A_DEFSYMBOL, A_DEFFLOAT, A_DEFFLOAT, 0);
   
    CLASS_MAINSIGNALIN(SOFAlizer_tilde_class, t_SOFAlizer_tilde, f);
   
    class_addmethod(SOFAlizer_tilde_class, (t_method)SOFAlizer_tilde_dsp, gensym("dsp"), 0);
}
