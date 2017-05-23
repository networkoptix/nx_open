/* ----------------------------------------------------------------- */
/*           The HMM-Based Speech Synthesis System (HTS)             */
/*           festopt_hts_engine developed by HTS Working Group       */
/*           http://hts-engine.sourceforge.net/                      */
/* ----------------------------------------------------------------- */
/*                                                                   */
/*  Copyright (c) 2001-2010  Nagoya Institute of Technology          */
/*                           Department of Computer Science          */
/*                                                                   */
/*                2001-2008  Tokyo Institute of Technology           */
/*                           Interdisciplinary Graduate School of    */
/*                           Science and Engineering                 */
/*                                                                   */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/* - Redistributions of source code must retain the above copyright  */
/*   notice, this list of conditions and the following disclaimer.   */
/* - Redistributions in binary form must reproduce the above         */
/*   copyright notice, this list of conditions and the following     */
/*   disclaimer in the documentation and/or other materials provided */
/*   with the distribution.                                          */
/* - Neither the name of the HTS working group nor the names of its  */
/*   contributors may be used to endorse or promote products derived */
/*   from this software without specific prior written permission.   */
/*                                                                   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND            */
/* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,       */
/* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF          */
/* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE          */
/* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS */
/* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,          */
/* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED   */
/* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,   */
/* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    */
/* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           */
/* POSSIBILITY OF SUCH DAMAGE.                                       */
/* ----------------------------------------------------------------- */

/* standard C libraries */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include "festival.h"

/* header of hts_engine API */
#ifdef __cplusplus
extern "C" {
#endif
#include "HTS_engine.h"
#ifdef __cplusplus
}
#endif
/* Getfp: wrapper for fopen */
    static FILE *Getfp(const char *name, const char *opt)
{
   FILE *fp = fopen(name, opt);

   if (fp == NULL) {
      cerr << "Getfp: Cannot open " << name << endl;
      festival_error();
   }

   return (fp);
}

/* HTS_Synthesize_Utt: generate speech from utt by using hts_engine API */
static LISP HTS_Synthesize_Utt(LISP utt)
{
   EST_Utterance *u = get_c_utt(utt);
   EST_Item *item = 0;
   LISP hts_engine_params = NIL;
   LISP hts_output_params = NIL;

   char *fn_ms_lf0 = NULL, *fn_ms_mcp = NULL, *fn_ms_dur = NULL;
   char *fn_ts_lf0 = NULL, *fn_ts_mcp = NULL, *fn_ts_dur = NULL;
   char *fn_ws_lf0[3], *fn_ws_mcp[3];
   char *fn_ms_gvl = NULL, *fn_ms_gvm = NULL;
   char *fn_ts_gvl = NULL, *fn_ts_gvm = NULL;
   char *fn_gv_switch = NULL;

   FILE *labfp = NULL;
   FILE *lf0fp = NULL, *mcpfp = NULL, *rawfp = NULL, *durfp = NULL;

   HTS_Engine engine;

   int sampling_rate;
   int fperiod;
   double alpha;
   int stage;
   double beta;
   double uv_threshold;

   /* get params */
   hts_engine_params =
       siod_get_lval("hts_engine_params",
                     "festopt_hts_engine: no parameters set for module");
   hts_output_params =
       siod_get_lval("hts_output_params",
                     "festopt_hts_engine: no output parameters set for module");

   /* get model file names */
   fn_ms_dur = (char *) get_param_str("-md", hts_engine_params, "hts/lf0.pdf");
   fn_ms_mcp = (char *) get_param_str("-mm", hts_engine_params, "hts/mgc.pdf");
   fn_ms_lf0 = (char *) get_param_str("-mf", hts_engine_params, "hts/dur.pdf");
   fn_ts_dur =
       (char *) get_param_str("-td", hts_engine_params, "hts/tree-lf0.inf");
   fn_ts_mcp =
       (char *) get_param_str("-tm", hts_engine_params, "hts/tree-mgc.inf");
   fn_ts_lf0 =
       (char *) get_param_str("-tf", hts_engine_params, "hts/tree-dur.inf");
   fn_ws_mcp[0] =
       (char *) get_param_str("-dm1", hts_engine_params, "hts/mgc.win1");
   fn_ws_mcp[1] =
       (char *) get_param_str("-dm2", hts_engine_params, "hts/mgc.win2");
   fn_ws_mcp[2] =
       (char *) get_param_str("-dm3", hts_engine_params, "hts/mgc.win3");
   fn_ws_lf0[0] =
       (char *) get_param_str("-df1", hts_engine_params, "hts/lf0.win1");
   fn_ws_lf0[1] =
       (char *) get_param_str("-df2", hts_engine_params, "hts/lf0.win2");
   fn_ws_lf0[2] =
       (char *) get_param_str("-df3", hts_engine_params, "hts/lf0.win3");
   fn_ms_gvm =
       (char *) get_param_str("-cm", hts_engine_params, "hts/gv-mgc.pdf");
   fn_ms_gvl =
       (char *) get_param_str("-cf", hts_engine_params, "hts/gv-lf0.pdf");
   fn_ts_gvm =
       (char *) get_param_str("-em", hts_engine_params, "hts/tree-gv-mgc.inf");
   fn_ts_gvl =
       (char *) get_param_str("-ef", hts_engine_params, "hts/tree-gv-lf0.inf");
   fn_gv_switch =
       (char *) get_param_str("-k", hts_engine_params, "hts/gv-switch.inf");

   /* open input file pointers */
   labfp =
       Getfp(get_param_str("-labelfile", hts_output_params, "utt.feats"), "r");

   /* open output file pointers */
   rawfp = Getfp(get_param_str("-or", hts_output_params, "tmp.raw"), "wb");
   lf0fp = Getfp(get_param_str("-of", hts_output_params, "tmp.lf0"), "wb");
   mcpfp = Getfp(get_param_str("-om", hts_output_params, "tmp.mgc"), "wb");
   durfp = Getfp(get_param_str("-od", hts_output_params, "tmp.lab"), "wb");

   /* get other params */
   sampling_rate = (int) get_param_float("-s", hts_engine_params, 16000.0);
   fperiod = (int) get_param_float("-p", hts_engine_params, 80.0);
   alpha = (double) get_param_float("-a", hts_engine_params, 0.42);
   stage = (int) get_param_float("-g", hts_engine_params, 0.0);
   beta = (double) get_param_float("-b", hts_engine_params, 0.0);
   uv_threshold = (double) get_param_float("-u", hts_engine_params, 0.5);

   /* initialize */
   HTS_Engine_initialize(&engine, 2);
   HTS_Engine_set_sampling_rate(&engine, sampling_rate);
   HTS_Engine_set_fperiod(&engine, fperiod);
   HTS_Engine_set_alpha(&engine, alpha);
   HTS_Engine_set_gamma(&engine, stage);
   HTS_Engine_set_beta(&engine, beta);
   HTS_Engine_set_msd_threshold(&engine, 1, uv_threshold);
   HTS_Engine_set_audio_buff_size(&engine, 0);

   /* load models */
   HTS_Engine_load_duration_from_fn(&engine, &fn_ms_dur, &fn_ts_dur, 1);
   HTS_Engine_load_parameter_from_fn(&engine, &fn_ms_mcp, &fn_ts_mcp, fn_ws_mcp,
                                     0, FALSE, 3, 1);
   HTS_Engine_load_parameter_from_fn(&engine, &fn_ms_lf0, &fn_ts_lf0, fn_ws_lf0,
                                     1, TRUE, 3, 1);
   HTS_Engine_load_gv_from_fn(&engine, &fn_ms_gvm, &fn_ts_gvm, 0, 1);
   HTS_Engine_load_gv_from_fn(&engine, &fn_ms_gvl, &fn_ts_gvl, 1, 1);
   HTS_Engine_load_gv_switch_from_fn(&engine, fn_gv_switch);

   /* generate speech */
   if (u->relation("Segment")->first()) {       /* only if there segments */
      HTS_Engine_load_label_from_fp(&engine, labfp);
      HTS_Engine_create_sstream(&engine);
      HTS_Engine_create_pstream(&engine);
      HTS_Engine_create_gstream(&engine);
      if (rawfp != NULL)
         HTS_Engine_save_generated_speech(&engine, rawfp);
      if (durfp != NULL)
         HTS_Engine_save_label(&engine, durfp);
      if (lf0fp != NULL)
         HTS_Engine_save_generated_parameter(&engine, lf0fp, 1);
      if (mcpfp != NULL)
         HTS_Engine_save_generated_parameter(&engine, mcpfp, 1);
      HTS_Engine_refresh(&engine);
   }

   /* free */
   HTS_Engine_clear(&engine);

   /* close output file pointers */
   if (rawfp != NULL)
      fclose(rawfp);
   if (durfp != NULL)
      fclose(durfp);
   if (lf0fp != NULL)
      fclose(lf0fp);
   if (mcpfp != NULL)
      fclose(mcpfp);

   /* close input file pointers */
   if (labfp != NULL)
      fclose(labfp);

   /* Load back in the waveform */
   EST_Wave *w = new EST_Wave;
   w->resample(sampling_rate);

   if (u->relation("Segment")->first()) /* only if there segments */
      w->load_file(get_param_str("-or", hts_output_params, "tmp.raw"), "raw",
                   sampling_rate, "short", str_to_bo("native"), 1);

   item = u->create_relation("Wave")->append();
   item->set_val("wave", est_val(w));

   /* Load back in the segment times */
   EST_Relation *r = new EST_Relation;
   EST_Item *s,*o;
   
   r->load(get_param_str("-od", hts_output_params, "tmp.lab"),"htk");

   for(o = r->first(), s = u->relation("Segment")->first() ; (o != NULL) && (s != NULL) ; o = o->next(), s = s->next() )
      if (o->S("name").before("+").after("-").matches(s->S("name")))
         s->set("end",o->F("end")); 
      else
         cerr << "HTS_Synthesize_Utt: Output segment mismatch";

   delete r;

   return utt;
}

void festival_hts_engine_init(void)
{
   char buf[1024];

   HTS_get_copyright(buf);
   proclaim_module("hts_engine", buf);

   festival_def_utt_module("HTS_Synthesize", HTS_Synthesize_Utt,
                           "(HTS_Synthesis UTT)\n  Synthesize a waveform using the hts_engine and the current models");
}
