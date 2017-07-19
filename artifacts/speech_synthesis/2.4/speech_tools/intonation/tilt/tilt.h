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
/*                    Author :  Paul Taylor                              */
/*                    Date   :  February 1996                            */
/*-----------------------------------------------------------------------*/
/*                    Internal tilt functions                            */
/*                                                                       */
/*=======================================================================*/

#ifndef __TILT_H__
#define __TILT_H__


#include "ling_class/EST_Relation.h"

/*float rise_amp(EST_Item *e);
float fall_amp(EST_Item *e);
float gen_amp(EST_Item *e);

float rise_dur(EST_Item *e);
float fall_dur(EST_Item *e);
float gen_dur(EST_Item *e);

float rfc_to_tilt_amp(EST_Item *e);
float rfc_to_tilt_dur(EST_Item *e);
float rfc_to_a_tilt(EST_Item *e);
float rfc_to_d_tilt(EST_Item *e);
float rfc_to_t_tilt(EST_Item *e);

float tilt_to_rise_amp(EST_Item *e);
float tilt_to_rise_dur(EST_Item *e);
float tilt_to_peak_f0(EST_Item *e);
float tilt_to_peak_pos(EST_Item *e);
*/

float fncurve(float length, float t, float curve);
float unit_curve(float amp, float dur, float t);

int event_item(EST_Item &e);
int sil_item(EST_Item &e);
int connection_item(EST_Item &e);
void fill_rfc_types(EST_Relation &ev);

/*
void print_rfc2_events(EST_Relation &ev);
*/
void set_fn_start(EST_Relation &ev);

/*
inline float dur(EST_Item &e) 
{ 
    return e.fF("end") - e.fF("start");
}
*/

#endif /* TILT_UTILS */
