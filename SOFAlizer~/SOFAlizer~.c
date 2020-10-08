/* ****************************************************************************************************************** */
/* RT binaural filter: SOFAlizer~                                                                                     */
/* based on earplug~ by Pei Xiang (summer 2004), Jorge Castellanos (Fall 2006), Hans-Christoph Steiner (Spring 2009). */
/* ****************************************************************************************************************** */
/* Author: Christian Klemenschitz                                                                                     */
/* For: Acoustic Research Institute, Wohllebengasse 12-14, A-1040, Vienna, Austria                                    */
/* Contact: Piotr Majdak, piotr@majdak.com                                                                            */
/* ****************************************************************************************************************** */

#include <stdio.h>
#include <math.h>
#include <string.h>
//#include <libpd/z_libpd.h>
#include "m_pd.h"
#include "SOFAlizer~.h"
#include "mysofa.h"
#ifdef _MSC_VER		/* Thes pragmas only apply to Microsoft's compiler */
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

#define maxlen 512		/* define constant filter length to process */ 
#define PI 4*atan(1) 	/* define constant PI */
	
static t_class *SOFAlizer_tilde_class; 	/* main handle for class SOFAlizer_tilde_class */

/* pd external data space */
typedef struct _SOFAlizer_tilde
{
    t_object x_obj; 		      /* pure data convention */
    t_outlet *left_channel; 	  /* left output channel */
    t_outlet *right_channel;      /* right output channel */
    t_inlet *filenameIn;		  /* inlet to hold sofa filename */
    t_int len;					  /* filter length */
    t_float azimuth;    		  /* azimuth inlet from 0 to 360 degrees */
    t_float elevation;            /* elevation inlet from -90 to 90 (degrees) */
    t_float azi_old;
    t_float ele_old;
    struct MYSOFA_EASY *sofa;	  /* struct to read in SOFA files */
    char filename[1000];          /* argument filename */
    t_int block;			  /* to print out index */
    t_int M;					  /* M is number of impulse responses */
    t_int N; 					  /* N is length of impulse responses */
    t_int R;					  /* R is number of receivers */
    t_float values[3];			  /* values of cartesian coordinate system */
    t_float radius;				  /* holds calculated radius */
    t_float leftIR[maxlen]; 		  /* holds left impulse response */
	t_float rightIR[maxlen];		  /* holds right impulse response */
    t_float convBuffer[maxlen];	  /* convolution buffer */
    t_int bufferPin;			  /* indexing in convolution buffer */
    t_float f ;                   /* dummy float needed for signal inlet in dsp */
} t_SOFAlizer_tilde;

/*void SOFAlizer_dspState(t_float onoff) {
	t_pd *pdSendObj = gensym("pd")->s_thing;
	if (!pdSendObj) {
		post("dspState: unable to find pd to change dsp state to %d", onoff);
	}
	else {
		t_atom args[2];
		SETSYMBOL(&args[0], gensym("dsp"));
		SETFLOAT(&args[1], onoff);
		pd_forwardmess(pdSendObj, 2, args);
	}
}*/

static void SOFAlizer_tilde_open (t_SOFAlizer_tilde *x, t_symbol *filenameArg) 
{
	//outlet_float(x->out_dsp, 0);
	//SOFAlizer_dspState(0);
	strcpy(x->filename, filenameArg->s_name);   /* copy string from first argument */
	if (x->filename[0] == '\0') {				/* if no argument is passed -> error */
		error("No SOFA file specified");
		return;
	}
	else {
		post("SOFA file %s will be opened..", x->filename); /* else file is opened */
	}
	
	/* read in sofa file */
    int filter_length, err;												/* variables to open sofa file */ 
    x->sofa = mysofa_open(x->filename, 44100, &filter_length, &err);	/* function call from sofa api to open sofa file */ 
    if(x->sofa == NULL) {												/* error if file can't be opened */
		error("SOFA file %s couldn't be opened.", x->filename);
		return;
	}
	
	x->M = x->sofa->hrtf->M;	/* number of filters stored in sofa file */
	x->N = x->sofa->hrtf->N;	/* numer of sample points per stored filter */
	x->R = x->sofa->hrtf->R;	/* number of receivers */
	
	post("Metadata: %s %u HRTFs, %u samples @ %u Hz. %s, %s, %s.\n",					/* print out some metadata */
							x->filename,(unsigned int)x->M, (unsigned int)x->N,				
							(unsigned int)(x->sofa->hrtf->DataSamplingRate.values[0]),  	
							mysofa_getAttribute(x->sofa->hrtf->attributes, "DatabaseName"),	
							mysofa_getAttribute(x->sofa->hrtf->attributes, "Title"),
							mysofa_getAttribute(x->sofa->hrtf->attributes, "ListenerShortName"));
	
	/* Calculate radius of first measurement */
	int i;
	for (i = 0; i < 3; i++) {				/* read in first three coordinates */		
		x->values[i] = x->sofa->hrtf->SourcePosition.values[i];
	}
	x->radius = sqrtf(powf(x->values[0], 2.f) + powf(x->values[1], 2.f) + powf(x->values[2], 2.f)); /* calculate radius */
	post("Radius is %f m. \n", x->radius);
	
	/* Check for IR delays */
	float delay = 0;    								/* holds delays */
	int warn = 0;										/* to print out error message only once */
	for (i = 0; i < x->R; i++) {  
		delay = x->sofa->hrtf->DataDelay.values[i];		/* read in possible delays */
		if (delay != 0.0) {								/* if there are delays print a warning */
			warn = 1;
		}
	}	
	if (warn == 1){ 
		error(" Warning: This SOFA file will be incorrectly processed besause of non zero IR delays!"); 
	}
	//SOFAlizer_dspState(1);
	//outlet_float(x->out_dsp, 1);
}

/* constructor: initializer for class */ 
static void *SOFAlizer_tilde_new(t_symbol *filenameArg, t_float lenArg)
{
	/* pointer to class data space */
	t_SOFAlizer_tilde *x = (t_SOFAlizer_tilde *)pd_new(SOFAlizer_tilde_class); 
    
    /* create inlets and outlets */
    x->filenameIn = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_symbol, gensym("open"));
    floatinlet_new(&x->x_obj, &x->azimuth);  	/* create inlet for azimuth */   
    floatinlet_new(&x->x_obj, &x->elevation);   /* create inlet for elevation */	
    x->left_channel = outlet_new(&x->x_obj, gensym("signal"));  /* create outlet for left signal channel */
    x->right_channel = outlet_new(&x->x_obj, gensym("signal")); /* create outlet for right signal channel */
    //x->out_dsp = outlet_new(&x->x_obj, &s_float);				/* create outlet to handle dsp status */
	
	if (lenArg == 0) {
		lenArg = 128;								/* standard defined filter length is 128 */
	}
	x->len = lenArg;
													/* user defined filter length */
	
	post("        SOFAlizer~: binaural filter with measured reponses\n");
    post("        elevation: -40 to 90 degrees, azimuth: 0 to 360 degrees\n");
    post("Filter length is %u.\n",x->len);
    
    SOFAlizer_tilde_open(x, filenameArg);
	 
	/* Initialization */
    for (int i = 0; i < maxlen; i++) {				
		x->leftIR[i] = 0;					/* initialise left fir buffer */
		x->rightIR[i] = 0;					/* initialise right fir buffer */
		x->convBuffer[i] = 0; 				/* initialise convolution buffer */
	}
    x->bufferPin = 0;					  	/* initialise index of signal for convolution */
    x->block  = 0;
    
    return (x);								/* return object x */
}

/* perform routine method for dsp */
static t_int *SOFAlizer_tilde_perform(t_int *w) /* array w contains the pointers that were passed via dsp_add */
{	/*cast pointer array w back to their real types */
    t_SOFAlizer_tilde *x = (t_SOFAlizer_tilde *)(w[1]); 	/* userdata */
    t_float *in = (t_float *)(w[2]);						/* in_Samples */
    t_float *right_out = (t_float *)(w[3]);					/* out_R */
    t_float *left_out = (t_float *)(w[4]); 					/* out_ L */
    int blocksize = (int)(w[5]);							/* blocksize */
    
    if (x->block == 0) {
		post("blocksize is %u", blocksize);
		x->block = 1;
	}
	
	float azi = x-> azimuth; 	/* pass variable for azimuth */	
	float ele = x->elevation ; 	/* pass variable for elevation */
	int n;
	int nearest = 0;
	
	//if ( (azi >= x->azi_old + 2.5) || (azi >= x->azi_old - 2.5) || (ele >= x->ele_old + 5) || (ele >= x->ele_old - 5) ) {		
		/* find nearest impulse reponses */
		x->values[0] = x->radius * cos(PI/180 * ele) * cos(PI/180 * azi);	/* calculate coordinate x */
		x->values[1] = x->radius * cos(PI/180 * ele) * sin(PI/180 * azi);	/* calculate coordinate y */
		x->values[2] = x->radius * sin(PI/180 * ele);						/* calculate coordinate z */
	
	
		nearest = mysofa_lookup(x->sofa->lookup, x->values);	/* function call from sofa api to find index of nearest set of hrtfs*/
	//}
	// interpolation
	// int* neighbors = mysofa_neighborhood(x->sofa->neighborhood, nearest);
	// float* IR = mysofa_interpolate(x->sofa->hrtf, x->values, nearest, neighbors, x->sofa->hrtf->DataIR.values, x->sofa->hrtf->DataDelay.values);
	/*float leftIR[len]; // [-1. till 1]
	float rightIR[len];
	float leftDelay;          // unit is sec.	
	float rightDelay;         // unit is sec.

	mysofa_getfilter_float(x->sofa, x->values[0], x->values[1], x->values[2], leftIR, rightIR, &leftDelay, &rightDelay);
	*/
	/*if (nearest != x->nearest_old)
		post("index = %u \n", nearest);
	
	if (nearest != x->nearest_old) {
		for (int i = 0; i<32; i++){
			post("%f ", x->sofa->hrtf->DataIR.values[nearest * 512 + i]);	
		}
	}
	*/
	
	float inSample;							/* holds sample from input signal*/
	float leftSum;							/* to accumulate the sum of left channel during convolution */
	float rightSum;							/* to accumulate the sum of left channel during convolution */
	int size = x->N * x->sofa->hrtf->R; 	/* number of samples of left and right hrtfs = 2 receivers */
		
	/* convolution algorithm */
	while (blocksize--) {
	
		leftSum = 0;		/* initialize convolution sum left */
		rightSum = 0;		/* initialize convolution sum right */
		inSample = *(in++); /* get input signal */
		x->convBuffer[x->bufferPin] = inSample;		/* place input sample in convolution buffer */
			
		for (n = 0; n < x->len; n++) { 				/* calculate convolution sum for whole buffers */
			
			// interpolated
			//leftSum += IR[n] * x->convBuffer[(2*len + x->bufferPin - n) % len];	
			//rightSum += IR[x->N +n] * x->convBuffer[(2*len + x->bufferPin - n) % len];
			
			// not interpolated
			leftSum += x->sofa->hrtf->DataIR.values[nearest * size + n] * x->convBuffer[(2*x->len + x->bufferPin - n) % x->len];	
			rightSum += x->sofa->hrtf->DataIR.values[nearest * size + x->N + n] * x->convBuffer[(2*x->len + x->bufferPin - n) % x->len];
		}	
		
		*left_out++ = leftSum;		/* output left convolution sum to left output */
		*right_out++ = rightSum;	/* output right convolution sum to right output */
			
		x->bufferPin = (x->bufferPin + 1) % x->len;	/* update index of convolution buffer */
	}
	//x->nearest_old = nearest;
	//x->azi_old = azi;
	//x->ele_old = ele;
    return (w+6); /* returns a pointer to integer, that points to the address behind the stored pointers of the routine */
}

/* dsp method when audio engine is turned on */
static void SOFAlizer_tilde_dsp(t_SOFAlizer_tilde *x, t_signal **sp)
{
	/* adds a perform routine to dsp */
    dsp_add(SOFAlizer_tilde_perform, 5, x,  sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
    /*           callback,    params, userdata, in_Samples, out_R,		out_L,		  blocksize. */
}

/* destructor: free method to free memory */
void SOFAlizer_tilde_free(t_SOFAlizer_tilde *x)
{
  mysofa_close(x->sofa);	/* close sofa file and free all memory */
  mysofa_lookup_free(x->sofa->lookup);
  mysofa_neighborhood_free(x->sofa->neighborhood);
}

/* setup class to call methods like initializer, free, dsp... */
void SOFAlizer_tilde_setup(void)
{
    SOFAlizer_tilde_class = class_new(gensym("SOFAlizer~"), (t_newmethod)SOFAlizer_tilde_new,  /* generation of a new class */
     	(t_method)SOFAlizer_tilde_free, sizeof(t_SOFAlizer_tilde), CLASS_DEFAULT,
     	 A_DEFSYMBOL, A_DEFFLOAT, 0);
   
    CLASS_MAINSIGNALIN(SOFAlizer_tilde_class, t_SOFAlizer_tilde, f);	/*  to provide signal-inlets */
    //class_addmethod(SOFAlizer_tilde_class, (t_method)SOFAlizer_dspState, gensym("dspState"), A_FLOAT, 0); /* method to start and stop audio engine */
	class_addmethod(SOFAlizer_tilde_class, (t_method)SOFAlizer_tilde_open, gensym("open"), A_DEFSYMBOL, 0); /* method to open new sofa file */
    class_addmethod(SOFAlizer_tilde_class, (t_method)SOFAlizer_tilde_dsp, gensym("dsp"), A_CANT, 0); /* provides a method for signal processing */
}
