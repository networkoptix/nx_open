/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1996                            */
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
/*                 Author :  Alan Black and Paul Taylor                  */
/*                 Date   :  June 1996                                   */
/*-----------------------------------------------------------------------*/
/*               General low level wave utils (C++)                      */
/*  Conversion routines and lowest level file format independent read    */
/*  and write functions                                                  */
/*                                                                       */
/*=======================================================================*/
#ifndef __EST_WAVE_UTILS_H__
#define __EST_WAVE_UTILS_H__

#include <stdio.h>
#include "EST_cutils.h"
#include "EST_rw_status.h"
#include "EST_WaveFile.h"

void ConvertToIeeeExtended(double num,unsigned char *bytes);
double ConvertFromIeeeExtended(unsigned char *bytes);

int get_word_size(enum EST_sample_type_t sample_type);
enum EST_sample_type_t str_to_sample_type(const char *type);
const char *sample_type_to_str(enum EST_sample_type_t type);

short *convert_raw_data(unsigned char *file_data,int data_length,
			enum EST_sample_type_t sample_type,int bo);
enum EST_write_status save_raw_data(FILE *fp, const short *data, int offset,
			       int num_samples, int num_channels, 
			       enum EST_sample_type_t sample_type, int bo);
enum EST_write_status save_raw_data_nc(FILE *fp, const short *data, int offset,
			       int num_samples, int num_channels, 
			       enum EST_sample_type_t sample_type, int bo);
    
#endif /* __EST_WAVE_UTILS_H__ */
