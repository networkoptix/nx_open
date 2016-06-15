;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;                                                                     ;;;
;;;                     Carnegie Mellon University                      ;;;
;;;                      Copyright (c) 1998-2005                        ;;;
;;;                        All Rights Reserved.                         ;;;
;;;                                                                     ;;;
;;; Permission is hereby granted, free of charge, to use and distribute ;;;
;;; this software and its documentation without restriction, including  ;;;
;;; without limitation the rights to use, copy, modify, merge, publish, ;;;
;;; distribute, sublicense, and/or sell copies of this work, and to     ;;;
;;; permit persons to whom this work is furnished to do so, subject to  ;;;
;;; the following conditions:                                           ;;;
;;;  1. The code must retain the above copyright notice, this list of   ;;;
;;;     conditions and the following disclaimer.                        ;;;
;;;  2. Any modifications must be clearly marked as such.               ;;;
;;;  3. Original authors' names are not deleted.                        ;;;
;;;  4. The authors' names are not used to endorse or promote products  ;;;
;;;     derived from this software without specific prior written       ;;;
;;;     permission.                                                     ;;;
;;;                                                                     ;;;
;;; CARNEGIE MELLON UNIVERSITY AND THE CONTRIBUTORS TO THIS WORK        ;;;
;;; DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING     ;;;
;;; ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT  ;;;
;;; SHALL CARNEGIE MELLON UNIVERSITY NOR THE CONTRIBUTORS BE LIABLE     ;;;
;;; FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES   ;;;
;;; WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN  ;;;
;;; AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,         ;;;
;;; ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF      ;;;
;;; THIS SOFTWARE.                                                      ;;;
;;;                                                                     ;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;                                                                      ;;
;;;  A generic voice definition file for a clustergen synthesizer        ;;
;;;  Customized for: INST_LANG_VOX                                       ;;
;;;                                                                      ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;; Try to find the directory where the voice is, this may be from
;;; .../festival/lib/voices/ or from the current directory
(if (assoc 'INST_LANG_VOX_cg voice-locations)
    (defvar INST_LANG_VOX::dir 
      (cdr (assoc 'INST_LANG_VOX_cg voice-locations)))
    (defvar INST_LANG_VOX::dir (string-append (pwd) "/")))

;;; Did we succeed in finding it
(if (not (probe_file (path-append INST_LANG_VOX::dir "festvox/")))
    (begin
     (format stderr "INST_LANG_VOX::clustergen: Can't find voice scm files they are not in\n")
     (format stderr "   %s\n" (path-append  INST_LANG_VOX::dir "festvox/"))
     (format stderr "   Either the voice isn't linked in Festival library\n")
     (format stderr "   or you are starting festival in the wrong directory\n")
     (error)))

;;;  Add the directory contains general voice stuff to load-path
(set! load-path (cons (path-append INST_LANG_VOX::dir "festvox/") 
		      load-path))

(require 'clustergen)  ;; runtime scheme support

;;; Voice specific parameter are defined in each of the following
;;; files
(require 'INST_LANG_VOX_phoneset)
(require 'INST_LANG_VOX_tokenizer)
(require 'INST_LANG_VOX_tagger)
(require 'INST_LANG_VOX_lexicon)
(require 'INST_LANG_VOX_phrasing)
(require 'INST_LANG_VOX_intonation)
(require 'INST_LANG_VOX_durdata_cg) 
(require 'INST_LANG_VOX_f0model)
(require 'INST_LANG_VOX_other)

(require 'INST_LANG_VOX_statenames)
;; ... and others as required

;;;
;;;  Code specific to the clustergen waveform synthesis method
;;;

(set! cluster_synth_method 
  (if (boundp 'mlsa_resynthesis)
      cg_wave_synth
      cg_wave_synth_external ))

;;; Flag to save multiple loading of db
(defvar INST_LANG_VOX::cg_loaded nil)
;;; When set to non-nil clunits voices *always* use their closest voice
;;; this is used when generating the prompts
(defvar INST_LANG_VOX::clunits_prompting_stage nil)

;;; You may wish to change this (only used in building the voice)
(set! INST_LANG_VOX::closest_voice 'voice_kal_diphone_LANG)

(set! LANG_phone_maps
      '(
;        (M_t t)
;        (M_dH d)
;        ...
        ))

(define (voice_kal_diphone_LANG_phone_maps utt)
  (mapcar
   (lambda (s) 
     (let ((m (assoc_string (item.name s) LANG_phone_maps)))
       (if m
           (item.set_feat s "us_diphone" (cadr m))
           (item.set_feat s "us_diphone"))))
   (utt.relation.items utt 'Segment))
  utt)

(define (voice_kal_diphone_LANG)
  (voice_kal_diphone)
  (set! UniSyn_module_hooks (list voice_kal_diphone_LANG_phone_maps ))

  'kal_diphone_LANG
)

;;;  These are the parameters which are needed at run time
;;;  build time parameters are added to this list from build_clunits.scm
(set! INST_LANG_VOX_cg::dt_params
      (list
       (list 'db_dir 
             (if (string-matches INST_LANG_VOX::dir ".*/")
                 INST_LANG_VOX::dir
                 (string-append INST_LANG_VOX::dir "/")))
       '(name INST_LANG_VOX)
       '(index_name INST_LANG_VOX)
       '(trees_dir "festival/trees/")
       '(catalogue_dir "festival/clunits/")
       '(coeffs_dir "ccoefs/")
       '(coeffs_ext ".mcep")
       '(clunit_name_feat lisp_INST_LANG_VOX::cg_name)
))

;; So as to fit nicely with existing clunit voices we check need to 
;; prepend these params if we already have some set.
(if (boundp 'INST_LANG_VOX::dt_params)
    (set! INST_LANG_VOX::dt_params
          (append 
           INST_LANG_VOX_cg::dt_params
           INST_LANG_VOX::dt_params))
    (set! INST_LANG_VOX::dt_params INST_LANG_VOX_cg::dt_params))

(define (INST_LANG_VOX::nextvoicing i)
  (let ((nname (item.feat i "n.name")))
    (cond
;     ((string-equal nname "pau")
;      "PAU")
     ((string-equal "+" (item.feat i "n.ph_vc"))
      "V")
     ((string-equal (item.feat i "n.ph_cvox") "+")
      "CVox")
     (t
      "UV"))))

(define (INST_LANG_VOX::cg_name i)
  (let ((x nil))
  (if (assoc 'cg::trajectory clustergen_mcep_trees)
      (set! x i)
      (set! x (item.relation.parent i 'mcep_link)))

  (let ((ph_clunit_name 
         (INST_LANG_VOX::clunit_name_real
          (item.relation
           (item.relation.parent x 'segstate)
           'Segment))))
    (cond
     ((string-equal ph_clunit_name "ignore")
      "ignore")
     (t
      (item.name i)))))
)

(define (INST_LANG_VOX::clunit_name_real i)
  "(INST_LANG_VOX::clunit_name i)
Defines the unit name for unit selection for LANG.  The can be modified
changes the basic classification of unit for the clustering.  By default
this we just use the phone name, but you may want to make this, phone
plus previous phone (or something else)."
  (let ((name (item.name i)))
    (cond
     ((and (not INST_LANG_VOX::cg_loaded)
	   (or (string-equal "h#" name) 
	       (string-equal "1" (item.feat i "ignore"))
	       (and (string-equal "pau" name)
		    (or (string-equal "pau" (item.feat i "p.name"))
			(string-equal "h#" (item.feat i "p.name")))
		    (string-equal "pau" (item.feat i "n.name")))))
      "ignore")
     ;; Comment out this if you want a more interesting unit name
     ((null nil)
      name)

     ;; Comment out the above if you want to use these rules
     ((string-equal "+" (item.feat i "ph_vc"))
      (string-append
       name
       "_"
       (item.feat i "R:SylStructure.parent.stress")
       "_"
       (INST_LANG_VOX::nextvoicing i)))
     ((string-equal name "pau")
      (string-append
       name
       "_"
       (INST_LANG_VOX::nextvoicing i)))
     (t
      (string-append
       name
       "_"
;       (item.feat i "seg_onsetcoda")
;       "_"
       (INST_LANG_VOX::nextvoicing i))))))

(define (INST_LANG_VOX::cg_load)
  "(INST_LANG_VOX::cg_load)
Function that actual loads in the databases and selection trees.
SHould only be called once per session."
  (set! dt_params INST_LANG_VOX::dt_params)
  (set! clustergen_params INST_LANG_VOX::dt_params)
  (set! INST_LANG_VOX::param_vectors
        (track.load
	 (string-append 
	  INST_LANG_VOX::dir "/"
	  (get_param 'trees_dir dt_params "trees/")
	  (get_param 'index_name dt_params "all")
	  "_mcep.params")))
  (set! INST_LANG_VOX::clustergen_mcep_trees
        (load (string-append
               (string-append 
                INST_LANG_VOX::dir "/"
                (get_param 'trees_dir dt_params "trees/")
                (get_param 'index_name dt_params "all")
                "_mcep.tree")) t))

  (if (null (assoc 'cg::trajectory INST_LANG_VOX::clustergen_mcep_trees))
      (set! INST_LANG_VOX::clustergen_f0_trees
            (load (string-append
                   (string-append 
                    INST_LANG_VOX::dir "/"
                    (get_param 'trees_dir dt_params "trees/")
                    (get_param 'index_name dt_params "all")
                    "_f0.tree")) t)))

  (set! INST_LANG_VOX::cg_loaded t)
)

(define (INST_LANG_VOX::voice_reset)
  "(INST_LANG_VOX::voice_reset)
Reset global variables back to previous voice."
  (INST_LANG_VOX::reset_phoneset)
  (INST_LANG_VOX::reset_tokenizer)
  (INST_LANG_VOX::reset_tagger)
  (INST_LANG_VOX::reset_lexicon)
  (INST_LANG_VOX::reset_phrasing)
  (INST_LANG_VOX::reset_intonation)
  (INST_LANG_VOX::reset_f0model)
  (INST_LANG_VOX::reset_other)

  t
)

;; This function is called to setup a voice.  It will typically
;; simply call functions that are defined in other files in this directory
;; Sometime these simply set up standard Festival modules othertimes
;; these will be specific to this voice.
;; Feel free to add to this list if your language requires it

(define (voice_INST_LANG_VOX_cg)
  "(voice_INST_LANG_VOX_cg)
Define voice for LANG."
  ;; *always* required
  (voice_reset)

  ;; Select appropriate phone set
  (INST_LANG_VOX::select_phoneset)

  ;; Select appropriate tokenization
  (INST_LANG_VOX::select_tokenizer)

  ;; For part of speech tagging
  (INST_LANG_VOX::select_tagger)

  (INST_LANG_VOX::select_lexicon)

  (INST_LANG_VOX::select_phrasing)

  (INST_LANG_VOX::select_intonation)

  ;; For CG voice there is no duration modeling at the seg level
  (Parameter.set 'Duration_Method 'Default)
  (set! duration_cart_tree_cg INST_LANG_VOX::zdur_tree)
  (set! duration_ph_info_cg INST_LANG_VOX::phone_durs)
  (Parameter.set 'Duration_Stretch 1.0)

  (INST_LANG_VOX::select_f0model)

  ;; Waveform synthesis model: cluster_gen
  (set! phone_to_states INST_LANG_VOX::phone_to_states)
  (if (not INST_LANG_VOX::clunits_prompting_stage)
      (begin
	(if (not INST_LANG_VOX::cg_loaded)
	    (INST_LANG_VOX::cg_load))
        (set! clustergen_param_vectors INST_LANG_VOX::param_vectors)
	(set! clustergen_mcep_trees INST_LANG_VOX::clustergen_mcep_trees)
        (if (boundp 'INST_LANG_VOX::clustergen_f0_trees)
            (set! clustergen_f0_trees INST_LANG_VOX::clustergen_f0_trees))
	(Parameter.set 'Synth_Method 'ClusterGen)
      ))

  ;; This is where you can modify power (and sampling rate) if desired
  (set! after_synth_hooks nil)
;  (set! after_synth_hooks
;      (list
;        (lambda (utt)
;          (utt.wave.rescale utt 2.1))))

  (set! current_voice_reset INST_LANG_VOX::voice_reset)

  (set! current-voice 'INST_LANG_VOX_cg)
)

(define (is_pau i)
  (if (phone_is_silence (item.name i))
      "1"
      "0"))

(provide 'INST_LANG_VOX_cg)

