/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                    Copyright (c) 1994,1995,1996                       */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*                       Author :  Simon King (taken from Tony Robinson) */
/*                       Date   :  July 1994                             */
/*-----------------------------------------------------------------------*/
/*                       windowing functions                             */
/*                                                                       */
/*=======================================================================*/

#include <iostream>
#include <fstream>
#include "EST_system.h"
//#include "EST_sigpr.h"
#include "sigpr/EST_Window.h"
#include "EST_TNamedEnum.h"
#include "EST_math.h"

//static inline int irint(float f) { return (int)(f+0.5); }
//static inline int irint(double f) { return (int)(f+0.5); }
static inline int min(int a, int b) { return (a<b)?a:b; }
static inline int max(int a, int b) { return (a>b)?a:b; }

 /*************************************************************************/
 /*                                                                       */
 /* The actual window functions are defined here.                         */
 /*                                                                       */
 /*************************************************************************/

static void Rectangular(int size,  EST_TBuffer<float> &r_window, int window_centre=-1)
{
  // this may be a little silly
  (void) window_centre; // not useful for rectangular window
  r_window.ensure(size);
  
  for( int i=0; i<size; i++ )
    r_window[i] = 1.0;
}

static void  Triangular(int size, EST_TBuffer<float> &r_window, int window_centre=-1)
{
  int i, c, end=size-1;
  
  r_window.ensure(size);

  if( window_centre < 0 ) { // symmetric window (default)
    c=size/2;
    const float k = 2.0 / (float)size;
    
    if( (size & 1) != 0 ) // odd
      r_window[c]=1.0;
    
    for( i=0; i<c; i++ ){
      r_window[i] = i * k;
      r_window[end-i] = r_window[i];
    }
  }
  else{
    c = window_centre;
    const float k_left  = 1.0 / (float) (window_centre+1);
    const float k_right = 1.0 / (float) (size-(window_centre+1));

    r_window[c] = 1.0;
      
    // left half
    for( i=0; i<c; ++i )
      r_window[i] = i * k_left;

    // right half
    const int righthand_size = size-1-window_centre; 
    for( i=0; i<righthand_size; ++i )
      r_window[end-i] = i * k_right;
  }
}

static void  Hanning(int size, EST_TBuffer<float> &r_window, int window_centre=-1)
{
  int i,c;
  float k;
  r_window.ensure(size);
  int end = size-1;
  
  if( window_centre < 0 ){ // symmetric window (default)
    c = size/2;

    // only need to calculate one half + copy
    if( (size & 1) != 0) // when odd
      r_window[c]=1.0;

    k = 2.0 * M_PI / size;
    for( i=0; i<c; ++i )
      r_window[end-i] = r_window[i] = 0.5 - 0.5 * cos(k * (i + 0.5));
  }
  else{
    c = window_centre;
    r_window[c]=1.0; // we assume "centre" is 1.0 

    // first half
    int effective_size = (2*window_centre)+1;
    k = 2.0 * M_PI / effective_size;
    for( i=0; i<c; ++i )
      r_window[i] = 0.5 - 0.5 * cos(k * (i + 0.5));

    // second half
    const int righthand_size = size-1-window_centre; 
    effective_size = (2*righthand_size)+1;
    k = 2.0 * M_PI / effective_size;
    for( i=0; i<righthand_size; ++i )
      r_window[end-i] = 0.5 - 0.5 * cos(k * (i + 0.5));
  }
}

static void  Hamming(int size, EST_TBuffer<float> &r_window, int window_centre=-1)
{
  float k;
  int i, c, end=size-1;

  r_window.ensure(size);

  if( window_centre < 0 ){ // symmetric window (default)
    c=size/2;
    k = 2.0 * M_PI / size;
    
    if( (size & 1) != 0) // odd
      r_window[c]=1.0;
    
    for( i=0; i<c; i++ ){
      r_window[i] = 0.54 - 0.46 * cos(k * (i + 0.5));
      r_window[end-i] = r_window[i];
    }
  }
  else{
    c = window_centre;
    r_window[c] = 1.0;
    
    //first half
    int effective_size = (2*window_centre)+1;
    k = 2.0 * M_PI / effective_size;
    for( i=0; i<c ; ++i )
      r_window[i] = 0.54 - 0.46 * cos(k * (i + 0.5));
    
    //second half
    const int righthand_size = size-1-window_centre;
    effective_size = (2*righthand_size)+1;
    k = 2.0 * M_PI / effective_size;
    for( i=0; i<righthand_size; ++i )
      r_window[end-i] = 0.54 - 0.46 * cos(k * (i + 0.5));
  }
}

 /*************************************************************************/
 /*                                                                       */
 /* Here is the interface.                                                */
 /*                                                                       */
 /*************************************************************************/

typedef enum EST_WindowType {
  wf_none=0,
  wf_rectangle=1,
  wf_triangle=2,
  wf_hanning=3,
  wf_hamming=4
} EST_WindowType;

typedef struct Info {
    EST_Window::Func *func;
    const char *description;
  } Info;

static EST_TValuedEnumDefinition<EST_WindowType, const char *, Info> window_names[] =
{
  { wf_none,		{ "none" },				
				{NULL, 		"unknown window type"}},
  { wf_rectangle,	{"rectangle", "rect", "rectangular"},	
				{Rectangular,	"Rectangular window"}},
  { wf_triangle,	{"triangle", "tri", "triangular"},	
				{Triangular,	"Triangular window"}},
  { wf_hanning,		{"hanning", "han"},			
				{Hanning,	"Hanning window"}},
  { wf_hamming,		{"hamming", "ham"},			
				{Hamming,	"Hamming window"}},
  { wf_none,		{ NULL }},
};

static EST_TNamedEnumI<EST_WindowType, Info> map(window_names);

EST_Window::Func *EST_Window::creator(const char *name, bool report_error)
{ 
    EST_WindowType key = map.token(name);

    if (key == wf_none)
    {
	if (report_error)
	    cerr << "no such window type %s" << name << endl;
	return NULL;
    }
    else
	return map.info(key).func;
}

EST_String EST_Window::description(const char *name)
{
  EST_WindowType key = map.token(name);

  return map.info(key).description;
}

/** Return the dc offset for a section of speech. 
  * This can safely go off the limits of the waveform.
  */

static float find_dc(const EST_Wave &sig, int start, int size)
{
    int i;
    double sum = 0;

    start = max(0, start);
    size = min(size, sig.num_samples()-start);

    for(i=0; i<size; i++)
      sum += sig.a_no_check(start+i);

    return (sum / (float)size);
}

void EST_Window::make_window( EST_TBuffer<float> &window_vals, int size, 
			      const char *name, int window_centre )
{
    EST_WindowFunc *make_window =  EST_Window::creator(name);
    window_vals.ensure(size, (bool)FALSE); 
    make_window(size, window_vals, window_centre);
}

void EST_Window::make_window( EST_FVector &window_vals, int size, 
			      const char *name, int window_centre )
{
    EST_TBuffer<float> fwindow;
    EST_WindowFunc *make_window =  EST_Window::creator(name);
    fwindow.ensure(size, (bool)FALSE); 
    make_window(size, fwindow, window_centre);
    window_vals.resize(size);
    for (int i = 0; i < size; ++i)
	window_vals[i] = fwindow[i];
}

void EST_Window::window_signal(const EST_Wave &sig, 
			       EST_WindowFunc *make_window, 
			       int start, int size, 
			       EST_TBuffer<float> &window)
{
    EST_TBuffer<float> window_vals(size); 
    int i;
    float dc; 
      
      // create the window shape
    make_window(size, window_vals,-1);
    window.ensure(size, (bool)FALSE);
    dc = find_dc(sig, start, size);

     /* There are three separate loops, one each for the beginning and
    ends, where virtual values off the end of the sig array are
    requested, and one for the majority of the processing which falls
    in the middle of the sig array.*/

    for(i=0; i<size && start+i<0; i++)
      window[i] =0;

    for(; i<size && start+i < sig.num_samples(); i++)
      window[i] = (window_vals(i) * (sig.a(start + i) - dc) + dc);

    for(; i<size; i++)
      window[i] = 0;

}

void EST_Window::window_signal(const EST_Wave &sig, 
			       const EST_String &window_name, 
			       int start, int size, 
			       EST_FVector &frame, int resize)
{
    EST_WindowFunc *wf = creator(window_name, true);
    window_signal(sig, wf, start, size, frame, resize);
}

void EST_Window::window_signal(const EST_Wave &sig, 
			       EST_WindowFunc *make_window, 
			       int start, int size, 
			       EST_FVector &frame, int resize)
{
    EST_TBuffer<float> window_vals(size); 
      // create the window shape
    make_window(size, window_vals,-1);

    window_signal(sig, 
		  window_vals, 
		  start, size, 
		  frame, resize);
}

void EST_Window::window_signal(const EST_Wave &sig, 
			       EST_TBuffer<float> &window_vals,
			       int start, int size, 
			       EST_FVector &frame, int resize)
{
    int i;
    float dc; 

    if (resize)
	frame.resize(size);
    else if (frame.length() < size)
    {
	cerr << "Frame is wrong size: expected " << size << " got " 
	    << frame.length() << endl;
	return;
    }
      
/*    cout << "window vals\n";
    for (i = 0; i < size; ++i)
	cout << window_vals[i] << " ";

    cout << endl << endl;
*/
	
    dc = find_dc(sig, start, size);
//    cout << "dc is " << dc << endl;
     /* There are three separate loops, one each for the beginning and
    ends, where virtual values off the end of the sig array are
    requested, and one for the majority of the processing which falls
    in the middle of the sig array.*/

    for(i = 0; i < size && start+i< 0; i++)
	frame.a_no_check(i) = 0;

    for (; (i < size) && (start + i < sig.num_samples()); i++)
	frame.a_no_check(i) = (window_vals(i) * (sig.a_no_check(start + i) - dc) + dc);

    for(; i < frame.length(); i++)
	frame.a_no_check(i) = 0;

/*    cout << "sig vals\n";
    for (i = 0; i < size; ++i)
	cout << sig.a(i + start) << " ";

    cout << "frame vals\n";
    for (i = 0; i < size; ++i)
	cout << frame[i] << " ";

    cout << endl << endl;
*/
}

EST_String EST_Window::options_supported(void)
{
    EST_String s;

    for(int n=0; n< map.n() ; n++)
    {
	const char *nm = map.name(map.token(n));
	const char *d = map.info(map.token(n)).description;

	s += EST_String::cat("    ", nm, EST_String(" ")*(12-strlen(nm)), d, "\n");
    }
    return s;
}

EST_String EST_Window::options_short(void)
{
  EST_String s("");

  for(int n=0; n< map.n() ; n++)
    {
      const char *nm = map.name(map.token(n));

      if (s != "")
	s += ", ";

      s += nm;

    }
  return s;
}

#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_TNamedEnum.cc"

template class EST_TNamedEnumI<EST_WindowType, Info>;
template class EST_TValuedEnumI<EST_WindowType, const char *, Info>;

#endif
