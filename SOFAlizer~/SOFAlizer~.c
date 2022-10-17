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
#include "m_pd.h"
#include "mysofa.h"                 /* Access to library libmysofa.a*/
#ifdef _MSC_VER	                    /* These pragmas only apply to Microsoft's compiler */
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

#define maxlen 512                  /* To initialise size of convolution buffer */ 
#define Pi 4*atan(1)                /* Define constant Pi */

/* Main handle for class SOFAlizer_tilde_class */	
static t_class *SOFAlizer_tilde_class;	

typedef struct _SOFAlizer_tilde     /* Data space of pd external */
{
    t_object x_obj;                 /* Pure Data convention to access variables in data space */
    t_outlet *left_channel;         /* Outlet of left audio channel  */
    t_outlet *right_channel;        /* Outlet of right audio channel */
    t_inlet *filenameIn;            /* Inlet to pass SOFA filename into pd external */
    t_float azimuth;                /* Store azimuth angle from inlet */
    t_float elevation;              /* Store elevation angle from inlet */
    struct MYSOFA_EASY *sofa;       /* Struct to import SOFA files */
    char filename[1000];            /* Store input argument filename */
    t_int block;                    /* To print out PD's block size just once */
    t_int M;                        /* M is number of impulse responses */
    t_int N;                        /* N is length of impulse responses */
    t_int R;                        /* R is number of receivers */
    t_int len;                      /* Defines the number of samples beeing processed from HRTFs */
    t_float values[3];              /* Values of cartesian coordinate system */
    t_float radius;                 /* Length of calculated radius in meters */
    t_float convBuffer[maxlen];     /* Convolution buffer */
    t_int bufferPin;                /* Indexing variable for convolution buffer */
    t_float f;                      /* Dummy float is needed for signal inlet in dsp */
} t_SOFAlizer_tilde;

/* This function is called to open a SOFA file */
static void SOFAlizer_tilde_open (t_SOFAlizer_tilde *x, t_symbol *filenameArg) 
{
	strcpy(x->filename, filenameArg->s_name);   
	if (x->filename[0] == '\0')
	{				
		error("No SOFA file has been specified. \n");
		return;
	}
	else 
	{
		post("SOFA file %s will be opened.. \n ", x->filename);
	}
	
    /* Function call from SOFA API to import SOFA file into struct x->sofa */
    int filter_length, err;											
    x->sofa = mysofa_open(x->filename, 44100, &filter_length, &err);	 
    if(x->sofa == NULL) {												
		error("SOFA file %s couldn't be opened. \n", x->filename);
		return;
	}
	
	x->M = x->sofa->hrtf->M;	/* Number of HRTF sets stored in SOFA file */
	x->N = x->sofa->hrtf->N;	/* Numer of samples per HRTF*/
	x->R = x->sofa->hrtf->R;	/* Number of receivers */
	

	post("	Metadata: \n		File name: %s \n		Name of database: %s \n		Title: %s \n		Short name: %s \n		Sample rate: %u Hz \n		Number of HRTF sets: %u",
							x->filename,
							mysofa_getAttribute(x->sofa->hrtf->attributes, "DatabaseName"),	
							mysofa_getAttribute(x->sofa->hrtf->attributes, "Title"),
							mysofa_getAttribute(x->sofa->hrtf->attributes, "ListenerShortName"),
							(unsigned int)(x->sofa->hrtf->DataSamplingRate.values[0]),
							(unsigned int)x->M				
							);
	
	/* Calculate radius of first source position with the three coordinates of the cartesian coordinate system */
	int i;
	for (i = 0; i < 3; i++) 
	{		
		x->values[i] = x->sofa->hrtf->SourcePosition.values[i];
	}
    /* calculate radius */
	x->radius = sqrtf(powf(x->values[0], 2.f) + powf(x->values[1], 2.f) + powf(x->values[2], 2.f)); 
	post("		Radius: %f m \n		Filter length: %u samples \n		Processed filter length: %u samples \n",
				x->radius, (unsigned int)x->N, x->len
				);
	
	/* Check for IR delays for number of receivers x->R */
	float delay = 0;    																		
	for (i = 0; i < x->R; i++)
	{  
		delay = x->sofa->hrtf->DataDelay.values[i];
		if (delay != 0.0)
		{							
			error("		Warning: This SOFA file will be processed incorrectly besause of non zero IR delays! \n"); 
		}
	}
}

/* Constructor: Initializer of class SOFAlizer~. It is called directly after setup function */
/* When a SOFAlizer~ object is created, two input parameters can be passed, */
/* SOFA filename and processed filter length, where the second is mandatory. */
static void *SOFAlizer_tilde_new(t_symbol *filenameArg, t_float lenArg)   
{
    /* Pointer to class data space */
    t_SOFAlizer_tilde *x = (t_SOFAlizer_tilde *)pd_new(SOFAlizer_tilde_class); 
    
    /* Create inlets and outlets */
    x->filenameIn = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_symbol, gensym("open"));
    floatinlet_new(&x->x_obj, &x->azimuth);						   
    floatinlet_new(&x->x_obj, &x->elevation);						
    x->left_channel = outlet_new(&x->x_obj, gensym("signal"));  
    x->right_channel = outlet_new(&x->x_obj, gensym("signal")); 
	
	if (lenArg == 0) {
		lenArg = 128;		/* Standard defined filter length is 128 */
	}
	x->len = lenArg;		/* User defined filter length */
	
	post("SOFAlizer~: Interactive, binaural real-time syntheses with HRTF sets provided from a SOFA file. \n");
    
    /* Open SOFA file */
    SOFAlizer_tilde_open(x, filenameArg);
	 
    /* Initialization */
    for (int i = 0; i < maxlen; i++) {									
		x->convBuffer[i] = 0; 				
	}
    x->bufferPin = 0;					  	
    x->block  = 0;							
    
    return (x);		/* Return object x */
}

/* Callback function perform routine that is called from DSP method*/
/* Array w contains the pointers that were passed via dsp_add */
static t_int *SOFAlizer_tilde_perform(t_int *w) 			
{	
    /*Cast pointer array w back to their real types */
    t_SOFAlizer_tilde *x = (t_SOFAlizer_tilde *)(w[1]);    /* Data space */
    t_float *in = (t_float *)(w[2]);                       /* Input stream */
    t_float *right_out = (t_float *)(w[3]);                /* Output stream right */
    t_float *left_out = (t_float *)(w[4]);                 /* Output stream left */
    int blocksize = (int)(w[5]);                           /* PD's block size */
    
    if (x->block == 0) {
        post("Block size is %u", blocksize);
        x->block = 1;
	}
	
    float azi = x-> azimuth; 		
    float ele = x->elevation ; 	
    int nearest = 0;
	
    /* Calculate x, y, z coordinate for given angles */
    x->values[0] = x->radius * cos(Pi/180 * ele) * cos(Pi/180 * azi);	
    x->values[1] = x->radius * cos(Pi/180 * ele) * sin(Pi/180 * azi);	
    x->values[2] = x->radius * sin(Pi/180 * ele);					
	
    /* Function call from SOFA API to find index of nearest set of HRTFs*/
    nearest = mysofa_lookup(x->sofa->lookup, x->values);	
	
    /* Input and output sample variables */
    float inSample;							
    float leftSum;							
    float rightSum;
    /* Number of samples of left and right HRTFs = 2 receivers */						
    int size = x->N * x->sofa->hrtf->R; 	
		
    /* Convolution algorithm */
    while (blocksize--) 
    {
        /* Initialization of convolution sum variables */
        leftSum = 0;								
        rightSum = 0;
        /* Get next input sample and place it in convolution buffer */								
        inSample = *(in++); 						
        x->convBuffer[x->bufferPin] = inSample;		
			
        /* Calculate convolution sum for whole buffers */
        for (int n = 0; n < x->len; n++)				
        { 				
            leftSum += x->sofa->hrtf->DataIR.values[nearest * size + n] * x->convBuffer[(2*x->len + x->bufferPin - n) % x->len];	
            rightSum += x->sofa->hrtf->DataIR.values[nearest * size + x->N + n] * x->convBuffer[(2*x->len + x->bufferPin - n) % x->len];
        }	
        /* Output */
        *left_out++ = leftSum;						
        *right_out++ = rightSum;					
        /* Update index of convolution buffer */	
        x->bufferPin = (x->bufferPin + 1) % x->len;	
    }
    return (w+6); /* Returns a pointer to integer, that points to the address behind the stored pointers of the routine */
}


/* DSP method is called when audio engine is turned on */
static void SOFAlizer_tilde_dsp(t_SOFAlizer_tilde *x, t_signal **sp)
{
    /* Adds a perform routine to dsp */
    dsp_add(SOFAlizer_tilde_perform,	 5,		 		x,		  sp[0]->s_vec,		 sp[1]->s_vec,		 sp[2]->s_vec,		 sp[0]->s_n);
            /* Callback function   NumberOfParam.   Data space    Input signal    Output signal right  Output signal left   PD's block size. */
}


/* This Method is called, when object SOFALizer~ is deleted in Pure Data */
/* Destructor: Method to close open files and free all memory */
void SOFAlizer_tilde_free(t_SOFAlizer_tilde *x)
{
    mysofa_close(x->sofa);	
    mysofa_lookup_free(x->sofa->lookup);
}


/* This function is called first, when an object SOFAlizer~ is created in Pure Data */
/* This setup function generates a new class SOFAlizer~ and its methods like constructor(initializer), free, open and dsp */
void SOFAlizer_tilde_setup(void)
{
    SOFAlizer_tilde_class = class_new(gensym("SOFAlizer~"), (t_newmethod)SOFAlizer_tilde_new,               /* Generation of a new class */
                            (t_method)SOFAlizer_tilde_free, sizeof(t_SOFAlizer_tilde), CLASS_DEFAULT,
                            A_DEFSYMBOL, A_DEFFLOAT, 0);
   
    CLASS_MAINSIGNALIN(SOFAlizer_tilde_class, t_SOFAlizer_tilde, f);                                        /* To provide signal-inlets */
    class_addmethod(SOFAlizer_tilde_class, (t_method)SOFAlizer_tilde_open, gensym("open"), A_DEFSYMBOL, 0); /* Method to open new SOFA file */
    class_addmethod(SOFAlizer_tilde_class, (t_method)SOFAlizer_tilde_dsp, gensym("dsp"), A_CANT, 0);        /* Provides a method for signal processing */
}
