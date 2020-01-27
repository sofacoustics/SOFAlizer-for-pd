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


static t_class *SOFAlizer_tilde_class; /* main handle for class SOFAlizer_tilde_class */

/* pd external data space */
typedef struct _SOFAlizer_tilde
{
    t_object x_obj; 		      /* pure data convention */
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

    t_float convBuffer[128];	  /* convolution buffer */
    t_float f ;                   /* dummy float needed for signal inlet in dsp */
    t_int bufferPin;			  /* indexing in convolution buffer */ 
} t_SOFAlizer_tilde;

/* constructor: initializer for class */ 
static void *SOFAlizer_tilde_new(t_symbol *filenameArg, t_floatarg azimArg, t_floatarg elevArg)
{
	t_SOFAlizer_tilde *x = (t_SOFAlizer_tilde *)pd_new(SOFAlizer_tilde_class); /* pointer to class */
	strcpy(x->filename, filenameArg->s_name);   /* copy string from first argument */
	if (x->filename[0] == '\0') {				/* if no argument is passed -> error */
		error("No SOFA file specified");
		return 0;
	}
	else {
		post("SOFA file %s will be opened..", x->filename); /* else file is opened */
	}
    
    floatinlet_new(&x->x_obj, &x->azimuth);  	/* create inlet for azimuth */   
    floatinlet_new(&x->x_obj, &x->elevation);   /* create inlet for elevation */
    x->azimuth = azimArg;						/* pass argument azimuth from inlet (0 to 360 degrees) */
    x->elevation = elevArg;						/* pass argument elevation from inlet (-90 to 90 degrees) */	
    x->left_channel = outlet_new(&x->x_obj, gensym("signal"));  /* create outlet for left signal channel */
    x->right_channel = outlet_new(&x->x_obj, gensym("signal")); /* create outlet for right signal channel */
    
    /* Read in SOFA file */
    int filter_length, err;												/* variables to open sofa file */ 
    x->hrtf = mysofa_open(x->filename, 44100, &filter_length, &err);	/* function call from sofa api to open sofa file */ 
    if(x->hrtf == NULL) {												/* error if file can't be opened */
		error("SOFA file %s couldn't be opened.", x->filename);
		return 0;
	}
	
	post("        SOFAlizer~: binaural filter with measured reponses\n") ;
    post("        elevation: -90 to 90 degrees, azimuth: 0 to 360 degrees\n") ;
	       
    x->M = x->hrtf->hrtf->M;	/* number of filters stored in sofa file */
	x->N = x->hrtf->hrtf->N;	/* numer of sample points per stored filter */
	
	post("Metadata: %s %u HRTFs, %u samples @ %u Hz. %s, %s, %s.\n",					/* print out some metadata */
							x->filename,(unsigned int)x->M, (unsigned int)x->N,				
							(unsigned int)(x->hrtf->hrtf->DataSamplingRate.values[0]),  	
							mysofa_getAttribute(x->hrtf->hrtf->attributes, "DatabaseName"),	
							mysofa_getAttribute(x->hrtf->hrtf->attributes, "Title"),
							mysofa_getAttribute(x->hrtf->hrtf->attributes, "ListenerShortName"));
	
	/* Check for IR delays */
	float delay = 0;    								/* holds delays */
	int warn = 0;										/* to print out error message only once */
	int i;
	for (i = 0; i < 2*x->M; i++) {  
		delay = x->hrtf->hrtf->DataDelay.values[i];		/* read in possible delays */
		if (delay > 1.0) {								/* if there are delays print them out */
			warn = 1;
			error(" delay = %f", delay);
		}
	}	
	if (warn == 1){ 
		error(" Warning: This SOFA file will be processed wrong besause of IR delays!"); 
	} 		 
	
    for (i = 0; i < 128; i++) {				/* initialise fir buffers */
		x->leftIR[i] = 0;				  
		x->rightIR[i] = 0;
	}
        
    for (i = 0; i < 128 ; i++) {			/* initialise convolution buffer */
         x->convBuffer[i] = 0; 			  
    }
	
    x->bufferPin = 0;					  	/* initialise index of signal for convolution */
	
	/* Calculate radius of first measurement */
	for (i = 0; i < 3; i++) {										/* read in first three coordinates */		
		x->values[i] = x->hrtf->hrtf->SourcePosition.values[i];
	}
	x->rad = sqrtf(powf(x->values[0], 2.f) + powf(x->values[1], 2.f) + powf(x->values[2], 2.f)); /* calculate radius */
	post("Radius is %f m. \n", x->rad);

    return (x);		/* return object x */
}

/* perform routine method for dsp */
static t_int *SOFAlizer_tilde_perform(t_int *w) /* array w contains the pointers, that were passed via dsp_add */
{	/*cast pointer array w back to their real types */
    t_SOFAlizer_tilde *x = (t_SOFAlizer_tilde *)(w[1]); 	/* userdata */
    t_float *in = (t_float *)(w[2]);						/* in_Samples */
    t_float *left_out = (t_float *)(w[3]);					/* out_L */
    t_float *right_out = (t_float *)(w[4]); 				/* out_ R */
    int blocksize = (int)(w[5]);							/* blocksize */

    x->azi = x->azimuth ; 		/* pass variable for azimuth */
    x->ele = x->elevation ;		/* pass variable for elevation */
		
	int i, n;
	
	/* find nearest impulse reponses */
	float Pi = 3.1415926536;
	x->values[0] = x->rad * cos(Pi/180 * x->ele) * cos(Pi/180 * x->azi);	/* calculate coordinate x */
	x->values[1] = x->rad * cos(Pi/180 * x->ele) * sin(Pi/180 * x->azi);	/* calculate coordinate y */
	x->values[2] = x->rad * sin(Pi/180 * x->ele);							/* calculate coordinate z */
	
	int nearest;							/* holds index for nearest impulse response according to given coordinates */
	int size = x->N * x->hrtf->hrtf->R;		/* number of samples of left and right hrtfs = 2 receivers */
	nearest = mysofa_lookup(x->hrtf->lookup, x->values);	/* function call from sofa api to find nearest set of hrtfs*/
		
	for (i = 0; i < 128; i++) {								/* write hrtfs into left and right array */
		x->leftIR[i] = x->hrtf->hrtf->DataIR.values[nearest * size + i];
		x->rightIR[i] = x->hrtf->hrtf->DataIR.values[nearest * size + x->N + i];
	}

    float inSample;		/* holds sample from input signal*/
	float convSum[2];	/* to accumulate the sum during convolution */
	/* convolution algorithm */
	while (blocksize--) {		
		convSum[0] = 0;		/* initialize convolution sum left */
		convSum[1] = 0;		/* initialize convolution sum right */
		inSample = *(in++); /* get input signal */
		x->convBuffer[x->bufferPin] = inSample;		/* place input sample in convolution buffer */
		
		for (n = 0; n < 128; n++) { 				/* calculate convolution sum for whole buffer */
			convSum[0] += x->leftIR[n] * x->convBuffer[(x->bufferPin-n) &127];
			convSum[1] += x->rightIR[n] * x->convBuffer[(x->bufferPin-n) &127];
		}	
		
		*left_out++ = convSum[0];		/* output left convolution sum to left output */
		*right_out++ = convSum[1];		/* output right convolution sum to right output */
     	
     	x->bufferPin = (x->bufferPin + 1) & 127;	/* update index of convolution buffer */
    }
	
    return (w+6); /* returns a pointer to integer, that points to the address behind the stored pointers of the routine */
}

/* dsp method when audio engine is turned on */
static void SOFAlizer_tilde_dsp(t_SOFAlizer_tilde *x, t_signal **sp)
{
	/* adds a perform routine to dsp */
    dsp_add(SOFAlizer_tilde_perform, 5, x,  sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
    /*           callback,    params, userdata, in_Samples, out_L,		out_R,		  blocksize. */
}

/* destructor: free method to free memory */
void SOFAlizer_tilde_free(t_SOFAlizer_tilde *x)
{
  mysofa_close(x->hrtf);	/* close sofa file and free all memory */
}

/* setup class to call methods like initializer, free, dsp... */
void SOFAlizer_tilde_setup(void)
{
    SOFAlizer_tilde_class = class_new(gensym("SOFAlizer~"), (t_newmethod)SOFAlizer_tilde_new,  /* generation of a new class */
     	(t_method)SOFAlizer_tilde_free, sizeof(t_SOFAlizer_tilde), CLASS_DEFAULT,
     	 A_DEFSYMBOL, A_DEFFLOAT, A_DEFFLOAT, 0);
   
    CLASS_MAINSIGNALIN(SOFAlizer_tilde_class, t_SOFAlizer_tilde, f);	/*  to provide signal-inlets */
	
    class_addmethod(SOFAlizer_tilde_class, (t_method)SOFAlizer_tilde_dsp, gensym("dsp"), 0); /* provides a method for signal processing */
}
