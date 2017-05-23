/**   
*           The HMM-Based Speech Synthesis System (HTS)             
*                       HTS Working Group                           
*                                                                   
*                  Department of Computer Science                   
*                  Nagoya Institute of Technology                   
*                               and                                 
*   Interdisciplinary Graduate School of Science and Engineering    
*                  Tokyo Institute of Technology                    
*                                                                   
*                Portions Copyright (c) 2001-2006                       
*                       All Rights Reserved.
*                         
*              Portions Copyright 2000-2007 DFKI GmbH.
*                      All Rights Reserved.                  
*                                                                   
*  Permission is hereby granted, free of charge, to use and         
*  distribute this software and its documentation without           
*  restriction, including without limitation the rights to use,     
*  copy, modify, merge, publish, distribute, sublicense, and/or     
*  sell copies of this work, and to permit persons to whom this     
*  work is furnished to do so, subject to the following conditions: 
*                                                                   
*    1. The source code must retain the above copyright notice,     
*       this list of conditions and the following disclaimer.       
*                                                                   
*    2. Any modifications to the source code must be clearly        
*       marked as such.                                             
*                                                                   
*    3. Redistributions in binary form must reproduce the above     
*       copyright notice, this list of conditions and the           
*       following disclaimer in the documentation and/or other      
*       materials provided with the distribution.  Otherwise, one   
*       must contact the HTS working group.                         
*                                                                   
*  NAGOYA INSTITUTE OF TECHNOLOGY, TOKYO INSTITUTE OF TECHNOLOGY,   
*  HTS WORKING GROUP, AND THE CONTRIBUTORS TO THIS WORK DISCLAIM    
*  ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL       
*  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   
*  SHALL NAGOYA INSTITUTE OF TECHNOLOGY, TOKYO INSTITUTE OF         
*  TECHNOLOGY, HTS WORKING GROUP, NOR THE CONTRIBUTORS BE LIABLE    
*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY        
*  DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,  
*  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTUOUS   
*  ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR          
*  PERFORMANCE OF THIS SOFTWARE.                                    
*                                                                   
*
*  This software was translated to C for use within Festival to offer
*  multi-excitation MLSA
*            Alan W Black (awb@cs.cmu.edu) 3rd April 2009
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <EST_walloc.h>
#include "festival.h"

#include "mlsa_resynthesis.h"

/**
 * Synthesis of speech out of speech parameters.
 * Mixed excitation MLSA vocoder. 
 * 
 * Java port and extension of HTS engine version 2.0
 * Extension: mixed excitation
 * @author Marcela Charfuelan 
 * And ported to C by Alan W Black (awb@cs.cmu.edu)
 */

#define boolean int
#define true 1
#define false 0

typedef struct HTSData_struct {

    int rate;
    int fperiod;
    double rhos;

    int stage;
    double alpha;
    double beta;
    boolean useLogGain;
    double uf;
    boolean algnst; /* use state level alignment for duration     */
    boolean algnph; /* use phoneme level alignment for duration   */
    boolean useMixExc;  /* use Mixed Excitation */
    boolean useFourierMag; /* use Fourier magnitudes for pulse generation */
    boolean useGV; /* use global variance in parameter generation */
    boolean useGmmGV; /* use global variance as a Gaussian Mixture Model */
    boolean useUnitDurationContinuousFeature; /* for using external duration, so it will not be generated from HMMs*/
    boolean useUnitLogF0ContinuousFeature;    /* for using external f0, so it will not be generated from HMMs*/
    
    /** variables for controling generation of speech in the vocoder       
     * these variables have default values but can be fixed and read from the
     * audio effects component. [Default][min--max]   */
    double length;   /* total number of frame for generated speech */
            /* length of generated speech (in seconds)   [N/A][0.0--30.0]  */
    double durationScale; /* less than 1.0 is faster and more than 1.0 is slower, min=0.1 max=3.0 */

    boolean LogGain;
    char *PdfStrFile, *PdfMagFile;
    
    int NumFilters, OrderFilters;
    double **MixFilters;
    double F0Std;
    double F0Mean;

} HTSData;

#if 0
typedef struct HTSData_struct {

    int rate = 16000;
    int fperiod = 80;
    double rhos = 0.0;

    int stage = 0;
    double alpha = 0.42;
    boolean useLogGain = false;
    double uf = 0.5;
    boolean algnst = false; /* use state level alignment for duration     */
    boolean algnph = false; /* use phoneme level alignment for duration   */
    boolean useMixExc     = true;  /* use Mixed Excitation */
    boolean useFourierMag = false; /* use Fourier magnitudes for pulse generation */
    boolean useGV = false; /* use global variance in parameter generation */
    boolean useGmmGV = false; /* use global variance as a Gaussian Mixture Model */
    boolean useUnitDurationContinuousFeature = false; /* for using external duration, so it will not be generated from HMMs*/
    boolean useUnitLogF0ContinuousFeature = false;    /* for using external f0, so it will not be generated from HMMs*/
    
    /** variables for controling generation of speech in the vocoder       
     * these variables have default values but can be fixed and read from the
     * audio effects component. [Default][min--max]   */
    double f0Std   = 1.0;   /* variable for f0 control, multiply f0  [1.0][0.0--5.0]   */
    double f0Mean  = 0.0;   /* variable for f0 control, add f0 [0.0][0.0--100.0] */
    double length  = 0.0;   /* total number of frame for generated speech */
            /* length of generated speech (in seconds)   [N/A][0.0--30.0]  */
    double durationScale = 1.0; /* less than 1.0 is faster and more than 1.0 is slower, min=0.1 max=3.0 */

} HTSData;
#endif

static  int IPERIOD = 1;
static  boolean GAUSS = true;
static  int PADEORDER = 5;       /* pade order for MLSA filter */
static  int IRLENG    = 96;      /* length of impulse response */
    
/* for MGLSA filter (mel-generalised log spectrum approximation filter) */
static  boolean NORMFLG1 = true;
static  boolean NORMFLG2 = false;
static  boolean MULGFLG1 = true;
static  boolean MULGFLG2 = false;
static  boolean NGAIN    = false;
    
static  double ZERO  = 1.0e-10;            /* ~(0) */
static  double LZERO = (-1.0e+10);         /* ~log(0) */
    
static int stage;             /* Gamma=-1/stage : if stage=0 then Gamma=0 */
static double xgamma;          /* Gamma */
static boolean use_log_gain;  /* log gain flag (for LSP) */
static int fprd;              /* frame shift */
static int iprd;              /* interpolation period */
static boolean gauss;         /* flag to use Gaussian noise */
static double p1;             /* used in excitation generation */
static double pc;             /* used in excitation generation */
static double *pade;         /* used in mlsadf */
static int ppade;             /* offset for vector ppade */  

static double *C;            /* used in the MLSA/MGLSA filter */
static double *CC;           /* used in the MLSA/MGLSA filter */
static double *CINC;         /* used in the MLSA/MGLSA filter */
static double *D1;           /* used in the MLSA/MGLSA filter */
static int CINC_length, CC_length, C_length, D1_length;

static double rate;
static int pt1;                            /* used in mlsadf1 */
static int pt2;                            /* used in mlsadf2 */
static int *pt3;                          /* used in mlsadf2 */
    
/* mixed excitation variables */  
static int numM;           /* Number of bandpass filters for mixed excitation */
static int orderM;         /* Order of filters for mixed excitation */
static double **h;              /* filters for mixed excitation */  
static double *xpulseSignal;     /* the size of this should be orderM */
static double *xnoiseSignal;     /* the size of this should be orderM */
static boolean mixedExcitation   = false;
static boolean fourierMagnitudes = false;
    
static boolean lpcVocoder        = false;     /* true if lpc vocoder is used, then the input should be lsp parameters */

void initVocoder(int mcep_order, int mcep_vsize, HTSData *htsData);
int  htsMLSAVocoder(EST_Track *lf0Pst, 
                    EST_Track *mcepPst, 
                    EST_Track *strPst, 
                    EST_Track *magPst, 
                    int *voiced,
                    HTSData *htsData,
                    EST_Wave *wave);


LISP me_mlsa_resynthesis(LISP ltrack, LISP strack)
{
    /* Resynthesizes a wave from given track with mixed excitation*/
    EST_Track *t;
    EST_Track *str_track;
    EST_Wave *wave = 0;
    EST_Track *mcep;
    EST_Track *f0v;
    EST_Track *str;
    EST_Track *mag;
    int *voiced;
    int sr = 16000;
    int i,j;
    double shift;
    HTSData htsData;

    htsData.alpha = 0.42;
    htsData.beta = 0.0;

    if ((ltrack == NULL) ||
        (TYPEP(ltrack,tc_string) &&
         (streq(get_c_string(ltrack),"nil"))))
        return siod(new EST_Wave(0,1,sr));

    t = track(ltrack);
    str_track = track(strack);

    f0v = new EST_Track(t->num_frames(),1);
    mcep = new EST_Track(t->num_frames(),25);
    str = new EST_Track(t->num_frames(),5);
    mag = new EST_Track(t->num_frames(),10);
    voiced = walloc(int,t->num_frames());

    for (i=0; i<t->num_frames(); i++)
    {
        f0v->a(i) = t->a(i,0);
        if (f0v->a(i) > 0)
            voiced[i] = 1;
        else
            voiced[i] = 0;
        for (j=1; j<26; j++)
            mcep->a(i,j-1) = t->a(i,j);

        for (j=0; j<5; j++)
        {
            str->a(i,j) = str_track->a(i,j);
        }
        /*        printf("awb_debug str %d 0 %f 1 %f 2 %f 3 %f 4 %f\n",
            i,str->a(i,0),str->a(i,1),str->a(i,2),str->a(i,3),str->a(i,4));*/
#if 0
        for (j=57; j<66; j++)
            mag->a(i,j-57) = t->a(i,j);
#endif
    }

    if (t->num_frames() > 1)
        shift = 1000.0*(t->t(1)-t->t(0));
    else
        shift = 5.0;

    htsData.alpha = FLONM(siod_get_lval("mlsa_alpha_param",
                                        "mlsa: mlsa_alpha_param not set"));
    htsData.beta = FLONM(siod_get_lval("mlsa_beta_param",
                                       "mlsa: mlsa_beta_param not set"));
    htsData.stage = 0;
    htsData.LogGain = false;
    htsData.fperiod = 80;
    htsData.rate = 16000;
    htsData.rhos = 0.0;

    htsData.uf = 0.5;
    htsData.algnst = false; /* use state level alignment for duration     */
    htsData.algnph = false; /* use phoneme level alignment for duration   */
    htsData.useMixExc     = true;  /* use Mixed Excitation */
    htsData.useFourierMag = false; /* use Fourier magnitudes for pulse generation */
    htsData.useGV = false; /* use global variance in parameter generation */
    htsData.useGmmGV = false; /* use global variance as a Gaussian Mixture Model */
    htsData.useUnitDurationContinuousFeature = false; /* for using external duration, so it will not be generated from HMMs*/
    htsData.useUnitLogF0ContinuousFeature = false;    /* for using external f0, so it will not be generated from HMMs*/
    
    /** variables for controling generation of speech in the vocoder       
     * these variables have default values but can be fixed and read from the
     * audio effects component. [Default][min--max]   */
    htsData.F0Std   = 1.0;   /* variable for f0 control, multiply f0  [1.0][0.0--5.0]   */
    htsData.F0Mean  = 0.0;   /* variable for f0 control, add f0 [0.0][0.0--100.0] */
    htsData.length  = 0.0;   /* total number of frame for generated speech */
            /* length of generated speech (in seconds)   [N/A][0.0--30.0]  */
    htsData.durationScale = 1.0; /* less than 1.0 is faster and more than 1.0 is slower, min=0.1 max=3.0 */

    LISP filters = siod_get_lval("me_mix_filters",
                                 "mlsa: me_mix_filters not set");
    LISP f;
    int fl;
    htsData.NumFilters = 5;
    for (fl=0,f=filters; f; fl++)
        f=cdr(f);
    htsData.OrderFilters = fl/htsData.NumFilters;
    htsData.MixFilters = walloc(double *,htsData.NumFilters);
    for (i=0; i < htsData.NumFilters; i++)
    {
        htsData.MixFilters[i] = walloc(double,htsData.OrderFilters);
        for (j=0; j<htsData.OrderFilters; j++)
        {
            htsData.MixFilters[i][j] = FLONM(car(filters));
            filters = cdr(filters);
        }
    }

    wave = new EST_Wave(0,1,sr);

    if (mcep->num_frames() > 0)
    /* mcep_order and number of deltas */
        htsMLSAVocoder(f0v,mcep,str,mag,voiced,&htsData,wave);

    delete f0v;
    delete mcep;
    delete str;
    delete mag;
    delete voiced;

    return siod(wave);
}
    
/** The initialisation of VocoderSetup should be done when there is already 
 * information about the number of feature vectors to be processed,
 * size of the mcep vector file, etc. */
void initVocoder(int mcep_order, int mcep_vsize, HTSData *htsData) 
{
    int vector_size;
    double xrand;
        
    stage = htsData->stage;
    if(stage != 0)
        xgamma = -1.0 / stage;
    else
        xgamma = 0.0;
    use_log_gain = htsData->LogGain;
        
    fprd  = htsData->fperiod;
    rate  = htsData->rate;
    iprd  = IPERIOD;
    gauss = GAUSS;

    /* XXX */
    xrand = rand();

    if(stage == 0 ){  /* for MCP */
            
        /* mcep_order=74 and pd=PADEORDER=5 (if no HTS_EMBEDDED is used) */
        vector_size = (mcep_vsize * ( 3 + PADEORDER) + 5 * PADEORDER + 6) - (3 * (mcep_order+1));
        CINC_length = CC_length = C_length = mcep_order+1;
        D1_length = vector_size;
        C    = walloc(double,C_length);
        CC   = walloc(double,CC_length);
        CINC = walloc(double,CINC_length);
        D1   = walloc(double,D1_length);
                    
        vector_size=21;
        pade = walloc(double,vector_size);
        /* ppade is a copy of pade in mlsadf() function : ppade = &( pade[pd*(pd+1)/2] ); */
        ppade = PADEORDER*(PADEORDER+1)/2;  /* offset for vector pade */
        pade[0] = 1.0;
        pade[1] = 1.0; 
        pade[2] = 0.0;
        pade[3] = 1.0; 
        pade[4] = 0.0;       
        pade[5] = 0.0;
        pade[6] = 1.0; 
        pade[7] = 0.0;       
        pade[8] = 0.0;        
        pade[9] = 0.0;
        pade[10] = 1.0;
        pade[11] = 0.4999273; 
        pade[12] = 0.1067005; 
        pade[13] = 0.01170221; 
        pade[14] = 0.0005656279;
        pade[15] = 1.0; 
        pade[16] = 0.4999391; 
        pade[17] = 0.1107098; 
        pade[18] = 0.01369984; 
        pade[19] = 0.0009564853;
        pade[20] = 0.00003041721;
        
        pt1 = PADEORDER+1;
        pt2 = ( 2 * (PADEORDER+1)) + (PADEORDER * (mcep_order+2));
        pt3 = new int[PADEORDER+1];
        for(int i=PADEORDER; i>=1; i--)
            pt3[i] = ( 2 * (PADEORDER+1)) + ((i-1)*(mcep_order+2));
          
    } else { /* for LSP */
        vector_size = ((mcep_vsize+1) * (stage+3)) - ( 3 * (mcep_order+1));
        CINC_length = CC_length = C_length = mcep_order+1;
        D1_length = vector_size;
        C    = walloc(double,C_length);
        CC   = walloc(double,CC_length);
        CINC = walloc(double,CINC_length);
        D1   = walloc(double,D1_length);
    }
        
    /* excitation initialisation */
    p1 = -1;
    pc = 0.0;  
    
} /* method initVocoder */
    

    
/** 
 * HTS_MLSA_Vocoder: Synthesis of speech out of mel-cepstral coefficients. 
 * This procedure uses the parameters generated in pdf2par stored in:
 *   PStream mceppst: Mel-cepstral coefficients
 *   PStream strpst : Filter bank stregths for mixed excitation
 *   PStream magpst : Fourier magnitudes ( OJO!! this is not used yet)
 *   PStream lf0pst : Log F0  
 */
#if 0
AudioInputStream htsMLSAVocoder(HTSParameterGeneration pdf2par, HMMData htsData) 
{
    float sampleRate = 16000.0F;  //8000,11025,16000,22050,44100
    int sampleSizeInBits = 16;  //8,16
    int channels = 1;     //1,2
    boolean signed = true;    //true,false
    boolean bigEndian = false;  //true,false
    AudioFormat af = new AudioFormat(
                                     sampleRate,
                                     sampleSizeInBits,
                                     channels,
                                     signed,
                                     bigEndian);
    double [] audio_double = NULL;
        
    audio_double = htsMLSAVocoder(pdf2par.getlf0Pst(), pdf2par.getMcepPst(), pdf2par.getStrPst(), pdf2par.getMagPst(),
                                  pdf2par.getVoicedArray(), htsData);
        
    long lengthInSamples = (audio_double.length * 2 ) / (sampleSizeInBits/8);
    logger.info("length in samples=" + lengthInSamples );
        
    /* Normalise the signal before return, this will normalise between 1 and -1 */
    double MaxSample = MathUtils.getAbsMax(audio_double);
    for (int i=0; i<audio_double.length; i++)        
        audio_double[i] = 0.3 * ( audio_double[i] / MaxSample );
                
    DDSAudioInputStream oais = new DDSAudioInputStream(new BufferedDoubleDataSource(audio_double), af);
    return oais;
        
        
} /* method htsMLSAVocoder() */
#endif

static double mlsafir(double x, double *b, int m, double a, double aa, double *d, int _pt3 )
{
    double y = 0.0;
    int i;

    d[_pt3+0] = x;
    d[_pt3+1] = aa * d[_pt3+0] + ( a * d[_pt3+1] );

    for(i=2; i<=m; i++){
        d[_pt3+i] +=  a * ( d[_pt3+i+1] - d[_pt3+i-1]);
    }
      
    for(i=2; i<=m; i++){ 
        y += d[_pt3+i] * b[i];
    }
       
    for(i=m+1; i>1; i--){
        d[_pt3+i] = d[_pt3+i-1];  
    }
       
    return(y);
}

/** mlsdaf1:  sub functions for MLSA filter */
static double mlsadf1(double x, double *b, int m, double a, double aa, double *d) 
{
    double v;
    double out = 0.0;
    int i;
    //pt1 --> pt = &d1[pd+1]  
       
    for(i=PADEORDER; i>=1; i--) {
        d[i] = aa * d[pt1+i-1] + a * d[i];  
        d[pt1+i] = d[i] * b[1];
        v = d[pt1+i] * pade[ppade+i];
      
        //x += (1 & i) ? v : -v;
        if(i == 1 || i == 3 || i == 5)
            x += v;
        else 
            x += -v;
        out += v;
    }
    d[pt1+0] = x;
    out += x;

    return(out);
      
}

/** mlsdaf2: sub functions for MLSA filter */
static double mlsadf2(double x, double *b, int m, double a, double aa, double *d) 
{
    double v;
    double out = 0.0;
    int i; 
    // pt2 --> pt = &d1[pd * (m+2)] 
    // pt3 --> pt = &d1[ 2*(pd+1) ] 
      
    for(i=PADEORDER; i>=1; i--) {   
        d[pt2+i] = mlsafir(d[(pt2+i)-1], b, m, a, aa, d, pt3[i]);
        v = d[pt2+i] * pade[ppade+i];
          
        if(i == 1 || i == 3 || i == 5)
            x += v;
        else
            x += -v;
        out += v;
        
    }
    d[pt2+0] = x;
    out += x;
       
    return out;     
}
    
/** mlsadf: HTS Mel Log Spectrum Approximation filter */
static double mlsadf(double x, double *b, int m, double a, double aa, double *d) 
{
        
    x = mlsadf1(x, b, m,   a, aa, d);  
    x = mlsadf2(x, b, m-1, a, aa, d);
       
    return x; 
}
    
    
/** uniform_rand: generate uniformly distributed random numbers 1 or -1 */
static double uniformRand() 
{  
    double x;

    x = rand(); /* double uniformly distributed between 0.0 <= Math.random() < 1.0.*/
    if(x >= RAND_MAX/2.0)
        return 1.0;
    else
        return -1.0;
}
       
/** mc2b: transform mel-cepstrum to MLSA digital filter coefficients */
static void mc2b(double *mc, double *b, int m, double a ) 
{

    b[m] = mc[m];
    for(m--; m>=0; m--) {
        b[m] = mc[m] - a * b[m+1];  
    }
}
    
/** b2mc: transform MLSA digital filter coefficients to mel-cepstrum */
static void b2mc(double *b, double *mc, int m, double a)
{
    double d, o;
    int i;
    d = mc[m] = b[m];
    for(i=m--; i>=0; i--) {
        o = b[i] + (a * d);
        d = b[i];
        mc[i] = o;
    }
}
    
 
/** freqt: frequency transformation */
//private void freqt(double c1[], int m1, int cepIndex, int m2, double a){
static void freqt(double *c1, int m1, double *c2, int m2, double a)
{
    double *freqt_buff=NULL;        /* used in freqt */
    int    freqt_size=0;          /* buffer size for freqt */
    int i, j;
    double b = 1 - a * a;
    int g; /* offset of freqt_buff */
       
    if(m2 > freqt_size) {
        freqt_buff = walloc(double,m2 + m2 + 2);
        freqt_size = m2;  
    }
    g = freqt_size +1;
     
    for(i = 0; i < m2+1; i++)
        freqt_buff[g+i] = 0.0;
     
    for(i = -m1; i <= 0; i++){
        if(0 <= m2 )  
            freqt_buff[g+0] = c1[-i] + a * (freqt_buff[0] = freqt_buff[g+0]);
        if(1 <= m2)
            freqt_buff[g+1] = b * freqt_buff[0] + a * (freqt_buff[1] = freqt_buff[g+1]);
           
        for(j=2; j<=m2; j++)
            freqt_buff[g+j] = freqt_buff[j-1] + a * ( (freqt_buff[j] = freqt_buff[g+j]) - freqt_buff[g+j-1]);
           
    }
     
    /* move memory */
    for(i=0; i<m2+1; i++)
        c2[i] = freqt_buff[g+i];

    if (freqt_buff)
        wfree(freqt_buff);
       
}
   
/** c2ir: The minimum phase impulse response is evaluated from the minimum phase cepstrum */
static void c2ir(double *c, int nc, double *hh, int leng )
{
    int n, k, upl;
    double d;

    hh[0] = exp(c[0]);
    for(n = 1; n < leng; n++) {
        d = 0;
        upl = (n >= nc) ? nc - 1 : n;
        for(k = 1; k <= upl; k++ )
            d += k * c[k] * hh[n - k];
        hh[n] = d / n;
    }
}
   
/** b2en: functions for postfiltering */ 
static double b2en(double *b, int m, double a)
{
    double *spectrum2en_buff=NULL;  /* used in spectrum2en */
    int    spectrum2en_size=0;    /* buffer size for spectrum2en */
    double en = 0.0;
    int i;
    double *cep, *ir; 
      
    if(spectrum2en_size < m) {
        spectrum2en_buff = walloc(double,(m+1) + 2 * IRLENG);
        spectrum2en_size = m;
    }
    cep = walloc(double,(m+1) + 2 * IRLENG); /* CHECK! these sizes!!! */
    ir = walloc(double,(m+1) + 2 * IRLENG);
      
    b2mc(b, spectrum2en_buff, m, a);
    /* freqt(vs->mc, m, vs->cep, vs->irleng - 1, -a);*/
    freqt(spectrum2en_buff, m, cep, IRLENG-1, -a);
    /* HTS_c2ir(vs->cep, vs->irleng, vs->ir, vs->irleng); */
    c2ir(cep, IRLENG, ir, IRLENG);
    en = 0.0;
      
    for(i = 0; i < IRLENG; i++)
        en += ir[i] * ir[i];

    if (spectrum2en_buff)
        wfree(spectrum2en_buff);
    wfree(cep);
    wfree(ir);
      
    return(en);  
}  
    
/** ignorm: inverse gain normalization */
static void ignorm(double *c1, double *c2, int m, double ng)
{
    double k;
    int i;
    if(ng != 0.0 ) {
        k = pow(c1[0], ng);
        for(i=m; i>=1; i--)
            c2[i] = k * c1[i];
        c2[0] = (k - 1.0) / ng;
    } else {
        /* movem */  
        for(i=1; i<m; i++)  
            c2[i] = c1[i];
        c2[0] = log(c1[0]);   
    }
}
   
/** ignorm: gain normalization */
static void gnorm(double *c1, double *c2, int m, double g)
{
    double k;
    int i;
    if(g != 0.0) {
        k = 1.0 + g * c1[0];
        for(; m>=1; m--)
            c2[m] = c1[m] / k;
        c2[0] = pow(k, 1.0 / g);
    } else {
        /* movem */  
        for(i=1; i<=m; i++)  
            c2[i] = c1[i];
        c2[0] = exp(c1[0]);
    }
       
}
   
/** lsp2lpc: transform LSP to LPC. lsp[1..m] --> a=lpc[0..m]  a[0]=1.0 */
static void lsp2lpc(double *lsp, double *a, int m)
{
    double *lsp2lpc_buff=NULL;      /* used in lsp2lpc */
    int    lsp2lpc_size=0;        /* buffer size of lsp2lpc */
    int i, k, mh1, mh2, flag_odd;
    double xx, xf, xff;
    int p, q;                    /* offsets of lsp2lpc_buff */
    int a0, a1, a2, b0, b1, b2;  /* offsets of lsp2lpc_buff */
      
    flag_odd = 0;
    if(m % 2 == 0)
        mh1 = mh2 = m / 2;
    else {
        mh1 = (m+1) / 2;
        mh2 = (m-1) / 2;
        flag_odd = 1;
    }
      
    if(m > lsp2lpc_size){
        lsp2lpc_buff = walloc(double,5 * m + 6);
        lsp2lpc_size = m;
    }
      
    /* offsets of lsp2lpcbuff */
    p = m;
    q = p + mh1;
    a0 = q + mh2;
    a1 = a0 + (mh1 +1);
    a2 = a1 + (mh1 +1);
    b0 = a2 + (mh1 +1);
    b1 = b0 + (mh2 +1);
    b2 = b1 + (mh2 +1);
      
    /* move lsp -> lsp2lpc_buff */
    for(i=0; i<m; i++)
        lsp2lpc_buff[i] = lsp[i+1];
      
    for (i = 0; i < mh1 + 1; i++)
        lsp2lpc_buff[a0 + i] = 0.0;
    for (i = 0; i < mh1 + 1; i++)
        lsp2lpc_buff[a1 + i] = 0.0;
    for (i = 0; i < mh1 + 1; i++)
        lsp2lpc_buff[a2 + i] = 0.0;
    for (i = 0; i < mh2 + 1; i++)
        lsp2lpc_buff[b0 + i] = 0.0;
    for (i = 0; i < mh2 + 1; i++)
        lsp2lpc_buff[b1 + i] = 0.0;
    for (i = 0; i < mh2 + 1; i++)
        lsp2lpc_buff[b2 + i] = 0.0;

    /* lsp filter parameters */
    for (i = k = 0; i < mh1; i++, k += 2)
        lsp2lpc_buff[p + i] = -2.0 * cos(lsp2lpc_buff[k]);
    for (i = k = 0; i < mh2; i++, k += 2)
        lsp2lpc_buff[q + i] = -2.0 * cos(lsp2lpc_buff[k + 1]);
      
    /* impulse response of analysis filter */
    xx = 1.0;
    xf = xff = 0.0;
      
    for (k = 0; k <= m; k++) {
        if (flag_odd == 1) {
            lsp2lpc_buff[a0 + 0] = xx;
            lsp2lpc_buff[b0 + 0] = xx - xff;
            xff = xf;
            xf = xx;
        } else {
            lsp2lpc_buff[a0 + 0] = xx + xf;
            lsp2lpc_buff[b0 + 0] = xx - xf;
            xf = xx;
        }

        for (i = 0; i < mh1; i++) {
            lsp2lpc_buff[a0 + i + 1] = lsp2lpc_buff[a0 + i] + lsp2lpc_buff[p + i] * lsp2lpc_buff[a1 + i] + lsp2lpc_buff[a2 + i];
            lsp2lpc_buff[a2 + i] = lsp2lpc_buff[a1 + i];
            lsp2lpc_buff[a1 + i] = lsp2lpc_buff[a0 + i];
        }

        for (i = 0; i < mh2; i++) {
            lsp2lpc_buff[b0 + i + 1] = lsp2lpc_buff[b0 + i] + lsp2lpc_buff[q + i] * lsp2lpc_buff[b1 + i] + lsp2lpc_buff[b2 + i];
            lsp2lpc_buff[b2 + i] = lsp2lpc_buff[b1 + i];
            lsp2lpc_buff[b1 + i] = lsp2lpc_buff[b0 + i];
        }

        if (k != 0)
            a[k - 1] = -0.5 * (lsp2lpc_buff[a0 + mh1] + lsp2lpc_buff[b0 + mh2]);
        xx = 0.0;
    }

    for (i = m - 1; i >= 0; i--)
        a[i + 1] = -a[i];
    a[0] = 1.0;

    if (lsp2lpc_buff)
        wfree(lsp2lpc_buff);
}
    
/** gc2gc: generalized cepstral transformation */
static void gc2gc(double *c1, int m1, double g1, double *c2, int m2, double g2)
{
    double *gc2gc_buff=NULL;        /* used in gc2gc */
    int    gc2gc_size=0;          /* buffer size for gc2gc */
    int i, min, k, mk;
    double ss1, ss2, cc;
      
    if( m1 > gc2gc_size ) {
        gc2gc_buff = walloc(double,m1 + 1); /* check if these buffers should be created all the time */
        gc2gc_size = m1;
    }
      
    /* movem*/
    for(i=0; i<(m1+1); i++)
        gc2gc_buff[i] = c1[i];
      
    c2[0] = gc2gc_buff[0];
      
    for( i=1; i<=m2; i++){
        ss1 = ss2 = 0.0;
        min = m1 < i ? m1 : i - 1;
        for(k=1; k<=min; k++){
            mk = i - k;
            cc = gc2gc_buff[k] * c2[mk];
            ss2 += k * cc;
            ss1 += mk * cc;
        }
        
        if(i <= m1)
            c2[i] = gc2gc_buff[i] + (g2 * ss2 - g1 * ss1) / i;
        else
            c2[i] = (g2 * ss2 - g1 * ss1) / i;   
    }

    if (gc2gc_buff)
        wfree(gc2gc_buff);
}
    
    /** mgc2mgc: frequency and generalized cepstral transformation */
static void mgc2mgc(double *c1, int m1, double a1, double g1, double *c2, int m2, double a2, double g2)
{
    double a;
      
    if(a1 == a2){
        gnorm(c1, c1, m1, g1);
        gc2gc(c1, m1, g1, c2, m2, g2);
        ignorm(c2, c2, m2, g2);          
    } else {
        a = (a2 -a1) / (1 - a1 * a2);
        freqt(c1, m1, c2, m2, a);
        gnorm(c2, c2, m2, g1);
        gc2gc(c2, m2, g1, c2, m2, g2);
        ignorm(c2, c2, m2, g2);
          
    }
}
    
/** lsp2mgc: transform LSP to MGC.  lsp=C[0..m]  mgc=C[0..m] */
static void lsp2mgc(double *lsp, double *mgc, int m, double alpha)
{
    int i;
    /* lsp2lpc */
    lsp2lpc(lsp, mgc, m);  /* lsp starts in 1!  lsp[1..m] --> mgc[0..m] */
    if(use_log_gain)
        mgc[0] = exp(lsp[0]);
    else
        mgc[0] = lsp[0];
      
    /* mgc2mgc*/
    if(NORMFLG1)
        ignorm(mgc, mgc, m, xgamma);  
    else if(MULGFLG1)
        mgc[0] = (1.0 - mgc[0]) * stage; 
      
    if(MULGFLG1)
        for(i=m; i>=1; i--) 
            mgc[i] *= -stage;    
      
    mgc2mgc(mgc, m, alpha, xgamma, mgc, m, alpha, xgamma);  /* input and output is in mgc=C */
      
    if(NORMFLG2)
        gnorm(mgc, mgc, m, xgamma);
    else if(MULGFLG2)
        mgc[0] = mgc[0] * xgamma + 1.0;
      
    if(MULGFLG2)
        for(i=m; i>=1; i--)
            mgc[i] *= xgamma;
        
}
    
/** mglsadf: sub functions for MGLSA filter */
static double mglsadff(double x, double *b, int m, double a, double *d, int d_offset)
{
    int i;
    double y;
    y = d[d_offset+0] * b[1];
      
    for(i=1; i<m; i++) {
        d[d_offset+i] += a * (d[d_offset+i+1] -d[d_offset+i-1]);
        y += d[d_offset+i] * b[i+1];
    }
    x -= y;
      
    for(i=m; i>0; i--)
        d[d_offset+i] = d[d_offset+i-1];
    d[d_offset+0] = a * d[d_offset+0] + (1 - a * a) * x;
      
    return x;
}

static double mglsadf(double x, double *b, int m, double a, int n, double *d) 
{
    int i;
    for(i=0; i<n; i++)
        x = mglsadff(x, b, m, a, d, (i*(m+1)));  
              
    return x;
}
   
/** posfilter: postfilter for mel-cepstrum. It uses alpha and beta defined in HMMData */
static void postfilter_mcp(double *mcp, int m, double alpha, double beta) 
{
    double *postfilter_buff=NULL;   /* used in postfiltering */
    int postfilter_size = 0;     /* buffer size for postfiltering */
        
    double e1, e2;
    int k;
      
    if(beta > 0.0 && m > 1){
        if(postfilter_size < m){
            postfilter_buff = walloc(double,m+1);
            postfilter_size = m;
        }
        mc2b(mcp, postfilter_buff, m, alpha);
        e1 = b2en(postfilter_buff, m, alpha);
        
        postfilter_buff[1] -= beta * alpha * mcp[2];
        for(k = 2; k < m; k++)
            postfilter_buff[k] *= (1.0 +beta);
        e2 = b2en(postfilter_buff, m, alpha);
        postfilter_buff[0] += log(e1/e2) / 2;
        b2mc(postfilter_buff, mcp, m, alpha);
          
    }

    if (postfilter_buff)
        wfree(postfilter_buff);
            
}

static int modShift(int n, int N)
{
    if( n < 0 )
        while( n < 0 )
            n = n + N;
    else
        while( n >= N )
            n = n - N;
    return n;
}
              
/** Generate one pitch period from Fourier magnitudes */
static double *genPulseFromFourierMag(EST_Track *mag, int n, double f0, boolean aperiodicFlag)
{
        
    int numHarm = mag->num_channels();
    int i;
    int currentF0 = (int)round(f0);
    int T, T2;
    double *pulse = NULL;
      
    if(currentF0 < 512)
        T = 512;
    else
        T = 1024;
    T2 = 2*T;
      
    /* since is FFT2 no aperiodicFlag or jitter of 25% is applied */
      
    /* get the pulse */      
    pulse = walloc(double,T);
    EST_FVector real(T2);
    EST_FVector imag(T2);

    /* copy Fourier magnitudes (Wai C. Chu "Speech Coding algorithms foundation and evolution of standardized coders" pg. 460) */
    real[0] = real[T] = 0.0;   /* DC component set to zero */
    for(i=1; i<=numHarm; i++){     
        real[i] = real[T-i] = real[T+i] =  real[T2-i] = mag->a(n, i-1);  /* Symetric extension */
        imag[i] = imag[T-i] = imag[T+i] =  imag[T2-i] = 0.0;
    }
    for(i=(numHarm+1); i<(T-numHarm); i++){   /* Default components set to 1.0 */
        real[i] = real[T-i] = real[T+i] =  real[T2-i]  = 1.0;
        imag[i] = imag[T-i] = imag[T+i] =  imag[T2-i] = 0.0;
    }
          
    /* Calculate inverse Fourier transform */
    IFFT(real, imag);
      
    /* circular shift and normalise multiplying by sqrt(F0) */
    double sqrt_f0 = sqrt((float)currentF0); 
    for(i=0; i<T; i++)  
        pulse[i] = real[modShift(i-numHarm,T)] * sqrt_f0;

    return pulse;
      
}
    
int htsMLSAVocoder(EST_Track *lf0Pst, 
                   EST_Track *mcepPst, 
                   EST_Track *strPst, 
                   EST_Track *magPst, 
                   int *voiced,
                   HTSData *htsData,
                   EST_Wave *wave)
{

    double inc, x;
    double xp=0.0,xn=0.0,fxp,fxn,mix;  /* samples for pulse and for noise and the filtered ones */
    int i, j, k, m, s, mcepframe, lf0frame, s_double; 
    double alpha = htsData->alpha;
    double beta  = htsData->beta;
    double aa = 1-alpha*alpha;
    int audio_size;                    /* audio size in samples, calculated as num frames * frame period */
    double *audio_double = NULL;
    double *magPulse = NULL;         /* pulse generated from Fourier magnitudes */
    int magSample, magPulseSize;
    boolean aperiodicFlag = false;
      
    double *d;                        /* used in the lpc vocoder */
      
    double f0, f0Std, f0Shift, f0MeanOri;
    double *mc = NULL; /* feature vector for a particular frame */
    double *hp = NULL; /* pulse shaping filter, initialised once it is known orderM */  
    double *hn = NULL; /* noise shaping filter, initialised once it is known orderM */  
      
    /* Initialise vocoder and mixed excitation, once initialised it is known the order
     * of the filters so the shaping filters hp and hn can be initialised. */
    m = mcepPst->num_channels();
    mc = walloc(double,m);

    initVocoder(m-1, mcepPst->num_frames(), htsData);
      
    d = walloc(double,m);
    if (lpcVocoder)
    {
        /*        printf("Using LPC vocoder\n");  */
        for(i=0; i<m; i++)
            d[i] = 0.0;
    }
    mixedExcitation = htsData->useMixExc;
    fourierMagnitudes = htsData->useFourierMag;
         
    if ( mixedExcitation )
    {  
        numM = htsData->NumFilters;
        orderM = htsData->OrderFilters;
        
        xpulseSignal = walloc(double,orderM);
        xnoiseSignal = walloc(double,orderM);
        /* initialise xp_sig and xn_sig */
        for(i=0; i<orderM; i++)
            xpulseSignal[i] = xnoiseSignal[i] = 0;    
        
        h = htsData->MixFilters;
        hp = walloc(double,orderM);
        hn = walloc(double,orderM); 
              
        //Check if the number of filters is equal to the order of strpst 
        //i.e. the number of filters is equal to the number of generated strengths per frame.
#if 0
        if(numM != strPst->num_channels()) {
            printf("htsMLSAVocoder: error num mix-excitation filters = %d "
                   " in configuration file is different from generated str order= %d\n",
                   numM, strPst->num_channels());
        }
        printf("HMM speech generation with mixed-excitation.\n");
#endif
    } 
#if 0
    else
        printf("HMM speech generation without mixed-excitation.\n");  
      
    if( fourierMagnitudes && htsData->PdfMagFile != NULL)
        printf("Pulse generated with Fourier Magnitudes.\n");
    else
        printf("Pulse generated as a unit pulse.\n");
      
    if(beta != 0.0)
        printf("Postfiltering applied with beta=%f",(float)beta);
    else
        printf("No postfiltering applied.\n");
#endif
          
    /* Clear content of c, should be done if this function is
       called more than once with a new set of generated parameters. */
    for(i=0; i< C_length; i++)
        C[i] = CC[i] = CINC[i] = 0.0;
    for(i=0; i< D1_length; i++)
        D1[i]=0.0;
    
    f0Std = htsData->F0Std;
    f0Shift = htsData->F0Mean;
    f0MeanOri = 0.0;

    /* XXX */
    for (mcepframe=0,lf0frame=0; mcepframe<mcepPst->num_frames(); mcepframe++) 
    {
        if(voiced[mcepframe])
        {   /* WAS WRONG */
            f0MeanOri = f0MeanOri + lf0Pst->a(mcepframe, 0);
            lf0frame++;
        }
    }
    f0MeanOri = f0MeanOri/lf0frame;
   
    /* ____________________Synthesize speech waveforms_____________________ */
    /* generate Nperiod samples per mcepframe */
    s = 0;   /* number of samples */
    s_double = 0;
    audio_size = mcepPst->num_frames() * (fprd);
    audio_double = walloc(double,audio_size);  /* initialise buffer for audio */
    magSample = 1;
    magPulseSize = 0;

    for(mcepframe=0,lf0frame=0; mcepframe<mcepPst->num_frames(); mcepframe++) 
    { 
        /* get current feature vector mcp */ 
        for(i=0; i<m; i++)
            mc[i] = mcepPst->a(mcepframe, i); 
   
        /* f0 modification through the MARY audio effects */
        if(voiced[mcepframe]){
            f0 = f0Std * lf0Pst->a(mcepframe, 0) + (1-f0Std) * f0MeanOri + f0Shift;       
            lf0frame++;
            if(f0 < 0.0)
                f0 = 0.0;
        }
        else{
            f0 = 0.0;          
        }
         
        /* if mixed excitation get shaping filters for this frame */
        if (mixedExcitation)
        {
            for(j=0; j<orderM; j++) 
            {
                hp[j] = hn[j] = 0.0;
                for(i=0; i<numM; i++) 
                {
                    hp[j] += strPst->a(mcepframe, i) * h[i][j];
                    hn[j] += ( 1 - strPst->a(mcepframe, i) ) * h[i][j];
                }
            }
        }
        
        /* f0->pitch, in original code here it is used p, so f0=p in the c code */
        if(f0 != 0.0)
            f0 = rate/f0;
        
        /* p1 is initialised in -1, so this will be done just for the first frame */
        if( p1 < 0 ) {  
            p1   = f0;           
            pc   = p1;   
            /* for LSP */
            if(stage != 0){
                if( use_log_gain)
                    C[0] = LZERO;
                else
                    C[0] = ZERO;
                for(i=0; i<m; i++ )  
                    C[i] = i * PI / m;
                /* LSP -> MGC */
                lsp2mgc(C, C, (m-1), alpha);
                mc2b(C, C, (m-1), alpha);
                gnorm(C, C, (m-1), xgamma);
                for(i=1; i<m; i++)
                    C[i] *= xgamma;   
            }
          
        }
        
        if(stage == 0){         
            /* postfiltering, this is done if beta>0.0 */
            postfilter_mcp(mc, (m-1), alpha, beta);
            /* mc2b: transform mel-cepstrum to MLSA digital filter coefficients */   
            mc2b(mc, CC, (m-1), alpha);
            for(i=0; i<m; i++)
                CINC[i] = (CC[i] - C[i]) * iprd / fprd;
        } else {
          
            lsp2mgc(mc, CC, (m-1), alpha );
          
            mc2b(CC, CC, (m-1), alpha);
         
            gnorm(CC, CC, (m-1), xgamma);
         
            for(i=1; i<m; i++)
                CC[i] *= xgamma;
         
            for(i=0; i<m; i++)
                CINC[i] = (CC[i] - C[i]) * iprd / fprd;
          
        } 
          
        /* p=f0 in c code!!! */
        if( p1 != 0.0 && f0 != 0.0 ) {
            inc = (f0 - p1) * (double)iprd/(double)fprd;
            //System.out.println("  inc=(f0-p1)/80=" + inc );
        } else {
            inc = 0.0;
            pc = f0;
            p1 = 0.0; 
        }
              
        /* Here need to generate both xp:pulse and xn:noise signals seprately*/ 
        gauss = false; /* Mixed excitation works better with nomal noise */
      
        /* Generate fperiod samples per feature vector, normally 80 samples per frame */
        //p1=0.0;
        gauss=false;
        for(j=fprd-1, i=(iprd+1)/2; j>=0; j--) {          
            if(p1 == 0.0) {
                if(gauss)
                    x = 0 /* rand.nextGaussian() */;  /* XXX returns double, gaussian distribution mean=0.0 and var=1.0 */
                else
                    x = uniformRand(); /* returns 1.0 or -1.0 uniformly distributed */
            
                if(mixedExcitation) {
                    xn = x;
                    xp = 0.0;
                }
            } else {
                if( (pc += 1.0) >= p1 ){
                    if(fourierMagnitudes){
                        /* jitter is applied just in voiced frames when the stregth of the first band is < 0.5*/
                        /* this will work just if Radix FFT is used */  
                        /*if(strPst.getPar(mcepframe, 0) < 0.5)
                          aperiodicFlag = true;
                          else
                          aperiodicFlag = false;          
                          magPulse = genPulseFromFourierMagRadix(magPst, mcepframe, p1, aperiodicFlag);
                        */
                    
                        magPulse = genPulseFromFourierMag(magPst, mcepframe, p1, aperiodicFlag);
                        magSample = 0;
                        magPulseSize = -27 /* magPulse.length*/; /** XXX **/
                        x = magPulse[magSample];
                        magSample++;
                    } else
                        x = sqrt(p1);  

                    pc = pc - p1;
                } else {
                
                    if(fourierMagnitudes){
                        if(magSample >= magPulseSize ){ 
                            x = 0.0;
                        }
                        else
                            x = magPulse[magSample];                
                        magSample++;
                    } else
                        x = 0.0;                 
                }
              
                if(mixedExcitation) {
                    xp = x;
                    if(gauss)
                        xn = 0 /* rand.nextGaussian() */ ; /* XXX */
                    else
                        xn = uniformRand();
                }
            } 
          
            /* apply the shaping filters to the pulse and noise samples */
            /* i need memory of at least for M samples in both signals */
            if(mixedExcitation) {
                fxp = 0.0;
                fxn = 0.0;
                for(k=orderM-1; k>0; k--) {
                    fxp += hp[k] * xpulseSignal[k];
                    fxn += hn[k] * xnoiseSignal[k];
                    xpulseSignal[k] = xpulseSignal[k-1];
                    xnoiseSignal[k] = xnoiseSignal[k-1];
                }
                fxp += hp[0] * xp;
                fxn += hn[0] * xn;
                xpulseSignal[0] = xp;
                xnoiseSignal[0] = xn;
          
                /* x is a pulse noise excitation and mix is mixed excitation */
                mix = fxp+fxn;
      
                /* comment this line if no mixed excitation, just pulse and noise */
                x = mix;   /* excitation sample */
                /*                printf("awb_debug me %d %f\n",(int)(s_double),(float)x); */
            }
           
            if(lpcVocoder){
                // LPC filter  C[k=0] = gain is not used!
                if(!NGAIN)
                    x *= C[0];
                for(k=(m-1); k>1; k--){
                    x = x - (C[k] * d[k]);
                    d[k] = d[k-1];
                }
                x = x - (C[1] * d[1]);
                d[1] = x;
               
            } else if(stage == 0 ){
                if(x != 0.0 )
                    x *= exp(C[0]);
                x = mlsadf(x, C, m, alpha, aa, D1);
            
            } else {
                if(!NGAIN)
                    x *= C[0];
                x = mglsadf(x, C, (m-1), alpha, stage, D1);
            }
        
            audio_double[s_double] = x;
            s_double++;
          
            if((--i) == 0 ) {
                p1 += inc;
                for(k=0; k<m; k++){
                    C[k] += CINC[k];  
                }
                i = iprd;
            }          
        } /* for each sample in a period fprd */
        
        p1 = f0;
     
        /* move elements in c */
        /* HTS_movem(v->cc, v->c, m + 1); */
        for(i=0; i<m; i++){
            C[i] = CC[i];  
        }
      
    } /* for each mcep frame */
    
    /*    printf("Finish processing %d mcep frames.\n",mcepframe); */
    
    wave->resize(audio_size,1);
    for (i=0; i<s_double; i++)
        wave->a(i) = (short)audio_double[i];
        
    return 0;
      
} /* method htsMLSAVocoder() */

    
