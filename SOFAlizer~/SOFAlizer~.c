/* RT binaural filter: SOFAlizer~        */
/* Fix the header!??? */
/* based on KEMAR impulse measurement  */
/* Pei Xiang, summer 2004              */
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

static t_class *SOFAlizer_class;

int filter_length;
struct MYSOFA_EASY *hrtf = NULL;
int M;
int N;
float values[3];

typedef struct _SOFAlizer
{
    t_object x_obj; 
    t_outlet *left_channel ; 
    t_outlet *right_channel ; 

    t_float azimuth ;           
    t_float elevation ;         
    t_float azi ;
    t_float ele ;
     
    t_float crossCoef[8192] ; 
    t_float azimScale[13] ;
    t_int azimOffset[13] ;

    t_float previousImpulse[2][128] ;
    t_float currentImpulse[2][128] ;
    t_float convBuffer[128] ;
    t_float (*impulses)[2][128];     /* required???*/     /* a 3D array of 368x2x128 */
    t_float f ;                   /* dummy float for dsp */
    t_int bufferPin;
} t_SOFAlizer;

static t_int *SOFAlizer_perform(t_int *w)
{
    t_SOFAlizer *x = (t_SOFAlizer *)(w[1]) ;
    t_float *in = (t_float *)(w[2]);
    t_float *right_out = (t_float *)(w[3]);
    t_float *left_out = (t_float *)(w[4]); 
    int blocksize = (int)(w[5]);

    unsigned i;

    x->azi = x->azimuth ; 
    x->ele = x->elevation ;


	float Pi = 3.1415926536;
	values[0] = 1.4 * cos(Pi/180 * x->ele) * cos(Pi/180 * x->azi);
	values[1] = 1.4 * cos(Pi/180 * x->ele) * sin(Pi/180 * x->azi);
	values[2] = 1.4 * sin(Pi/180 * x->ele);
	
/* Diesen Teil ändern auf mysofa_getfilter_float()*/
	int reg = 0;
	reg = mysofa_lookup(hrtf->lookup, values);
	reg = floor(reg /2);

    float inSample;
	float convSum[2]; // to accumulate the sum during convolution.
    int blockScale = 8192 / blocksize;

	// Convolve the - interpolated - HRIRs (Left and Right) with the input signal.
	/* 1) Wie möchtest du die Faltung durchführen???
	   2) Wie wird in earplug die Faltung gemacht??? 
	*/
    while (blocksize--)
    {
		convSum[0] = 0; 
		convSum[1] = 0;	

		inSample = *(in++);

		x->convBuffer[x->bufferPin] = inSample;
		unsigned scaledBlocksize = blocksize * blockScale; // wozu ist das???
		unsigned blocksizeDelta = 8191 - scaledBlocksize;  // wozu ist das???
		for (i = 0; i < 128; i++)
		{ 
			convSum[0] += (x->previousImpulse[0][i] * x->crossCoef[blocksizeDelta] + 
							x->impulses[reg][0][i] * x->crossCoef[scaledBlocksize]) * 
							x->convBuffer[(x->bufferPin - i) &127];
			convSum[1] += (x->previousImpulse[1][i] * x->crossCoef[blocksizeDelta] + 
							x->impulses[reg][1][i] * x->crossCoef[scaledBlocksize]) * 
							x->convBuffer[(x->bufferPin - i) &127];

			x->previousImpulse[0][i] = x->impulses[reg][0][i];
			x->previousImpulse[1][i] = x->impulses[reg][1][i];
		}	
		x->bufferPin = (x->bufferPin + 1) & 127;
  
        *left_out++ = convSum[0];
     	*right_out++ = convSum[1];
    }
    return (w+6); // warum gerade 6???
}

static void SOFAlizer_dsp(t_SOFAlizer *x, t_signal **sp)
{
		// callback, params, userdata, in_samples, out_L,		out_R,		blocksize.
    dsp_add(SOFAlizer_perform, 5, x,  sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
}

static void *SOFAlizer_new(t_floatarg azimArg, t_floatarg elevArg)
{
    /*********************/
    
    int i, j, err;; 
    
    hrtf = mysofa_open("mit_kemar_normal_pinna.sofa", 44100, &filter_length, &err);
    if(hrtf == NULL)
    return err;
            
    M = hrtf->hrtf->M;
	N = hrtf->hrtf->N;
	
	fprintf(stderr, "mit_kemar_normal_pinna.sofa: %u HRTFs, %u samples @ %u Hz. %s, %s, %s.\n",
							hrtf->hrtf->M, hrtf->hrtf->N,
							(unsigned int)(hrtf->hrtf->DataSamplingRate.values[0]),
							mysofa_getAttribute(hrtf->hrtf->attributes, "DatabaseName"),
							mysofa_getAttribute(hrtf->hrtf->attributes, "Title"),
							mysofa_getAttribute(hrtf->hrtf->attributes, "ListenerShortName"));

/************************/
							
    t_SOFAlizer *x = (t_SOFAlizer *)pd_new(SOFAlizer_class);
    x->left_channel = outlet_new(&x->x_obj, gensym("signal"));
    x->right_channel = outlet_new(&x->x_obj, gensym("signal"));
    floatinlet_new(&x->x_obj, &x->azimuth) ;     /* 0 to 360 degrees */
    floatinlet_new(&x->x_obj, &x->elevation) ;   /* -40 to 90 degrees */

    x->azimuth = azimArg ;
    x->elevation = elevArg ;
    
/*****************************/
    t_float SOFAlizer_impulses[M][2][128];
    
    for (i = 0; i < M/2; i++) {
	    for (j = 0 ; j < 128 ; j++) {
			SOFAlizer_impulses[i][0][j] = hrtf->hrtf->DataIR.values[2*i*N+j];
			SOFAlizer_impulses[i][1][j] = hrtf->hrtf->DataIR.values[(2*i+1)*N+j];
		}
    }
/*********************/

/**************************************
    int i, j;
    FILE *fp;
    char buff[1024], *bufptr;
    int filedesc;
	filedesc = open_via_path(canvas_getdir(canvas_getcurrent())->s_name, "earplug_data.txt", "", buff, &bufptr, 1024, 0 );
    if (filedesc >= 0) // If there was no error opening the text file...
    {	
		post("[SOFAlizer~] found impulse reponse file, overriding defaults:") ;
        post("let's try loading %s/earplug_data.txt", buff);
        fp = fdopen(filedesc, "r") ;  
        for (i = 0; i < 368; i++) 
        {
            while(fgetc(fp) != 10) ;
            for (j = 0 ; j < 128 ; j++) {
				fscanf(fp, "%f %f ", &SOFAlizer_impulses[i][0][j], &SOFAlizer_impulses[i][1][j]);
            }
        }
        fclose(fp) ;
    }
**************************************/

    x->impulses = SOFAlizer_impulses;
        
    
    post("        SOFAlizer~: binaural filter with measured reponses\n") ;
    post("        elevation: -40 to 90 degrees. azimuth: 360") ;
    post("        dont let blocksize > 8192\n"); 


       
    for (i = 0; i < 128 ; i++)
    {
         x->convBuffer[i] = 0; 
		 x->previousImpulse[0][i] = 0; 
		 x->previousImpulse[1][i] = 0;
    }
	
    x->bufferPin = 0;

    for (i = 0; i < 8192 ; i++)
    {	
		x->crossCoef[i] = 1.0 * i / 8192;
	}

	// This is the scaling factor for the azimuth so that it corresponds to an HRTF in the KEMAR database
    //x->azimScale[0] = 0.153846153; x->azimScale[8] = 0.153846153;   /* -40 and 40 degree */
    //x->azimScale[1] = 0.166666666; x->azimScale[7] = 0.166666666;   /* -30 and 30 degree */
    //x->azimScale[2] = 0.2; x->azimScale[3]=0.2; x->azimScale[4]=0.2; x->azimScale[5]=0.2; x->azimScale[6]=0.2;  /* -20 to 20 degree */
    //x->azimScale[9] = 0.125;  		/* 50 degree */
    //x->azimScale[10] = 0.1;		/* 60 degree */
    //x->azimScale[11] = 0.066666666;      /* 70 degree */
    //x->azimScale[12] = 0.033333333;	/* 80 degree */
/*******************************************************************************
    x->azimOffset[0] = 0 ; 
    x->azimOffset[1] = 29 ;
    x->azimOffset[2] = 60 ;
    x->azimOffset[3] = 97 ;
    x->azimOffset[4] = 134 ;
    x->azimOffset[5] = 171 ;
    x->azimOffset[6] = 208 ;
    x->azimOffset[7] = 245 ;
    x->azimOffset[8] = 276 ;
    x->azimOffset[9] = 305 ;
    x->azimOffset[10] = 328 ;
    x->azimOffset[11] = 347 ;
    x->azimOffset[12] = 360 ;
***********************************************************************************/
    return (x);
}

void SOFAlizer_tilde_setup(void)
{
    SOFAlizer_class = class_new(gensym("SOFAlizer~"), (t_newmethod)SOFAlizer_new, 0,
    	sizeof(t_SOFAlizer), CLASS_DEFAULT, A_DEFFLOAT, A_DEFFLOAT, 0);
   
    CLASS_MAINSIGNALIN(SOFAlizer_class, t_SOFAlizer, f);
   
    class_addmethod(SOFAlizer_class, (t_method)SOFAlizer_dsp, gensym("dsp"), 0);
}
