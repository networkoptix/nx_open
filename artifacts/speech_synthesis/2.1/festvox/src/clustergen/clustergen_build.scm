;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;                                                                       ;;
;;;                     Carnegie Mellon University                        ;;
;;;                      Copyright (c) 2005-2006                          ;;
;;;                        All Rights Reserved.                           ;;
;;;                                                                       ;;
;;;  Permission is hereby granted, free of charge, to use and distribute  ;;
;;;  this software and its documentation without restriction, including   ;;
;;;  without limitation the rights to use, copy, modify, merge, publish,  ;;
;;;  distribute, sublicense, and/or sell copies of this work, and to      ;;
;;;  permit persons to whom this work is furnished to do so, subject to   ;;
;;;  the following conditions:                                            ;;
;;;   1. The code must retain the above copyright notice, this list of    ;;
;;;      conditions and the following disclaimer.                         ;;
;;;   2. Any modifications must be clearly marked as such.                ;;
;;;   3. Original authors' names are not deleted.                         ;;
;;;   4. The authors' names are not used to endorse or promote products   ;;
;;;      derived from this software without specific prior written        ;;
;;;      permission.                                                      ;;
;;;                                                                       ;;
;;;  CARNEGIE MELLON UNIVERSITY AND THE CONTRIBUTORS TO THIS WORK         ;;
;;;  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      ;;
;;;  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO         ;;
;;;  EVENT SHALL CARNEGIE MELLON UNIVERSITY NOR THE CONTRIBUTORS BE       ;;
;;;  LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY     ;;
;;;  DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,      ;;
;;;  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS       ;;
;;;  ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR              ;;
;;;  PERFORMANCE OF THIS SOFTWARE.                                        ;;
;;;                                                                       ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;                                                                       ;;
;;;  Author: Alan W Black (awb@cs.cmu.edu) Nov 2005                       ;;
;;;                                                                       ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;                                                                       ;;
;;;  Use wagon to do the HMM-state clustering and indexing for HTS        ;;
;;;  We are skipping the HTK part to give us more control over the        ;;
;;;  features (the multiple mapping isn't very portable across languages) ;;
;;;                                                                       ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;;  As the size of utterances can get *very* big, we don't follow clunits
;;;  doing processing on all utterances, but do things utt by utt.
;;;  This means unit type specific splitting happens later in the process
;;;
;;;  Thus:
;;;
;;;  for each utt
;;;     load hmmstates
;;;     load related combined cooefficients
;;;     name the units
;;;     dump the vectors for each unittype
;;;     dump the features for each unittype
;;;
;;;  extract unit specific vector and feat files
;;;
;;;  do clustering (cross-validation or stepwise) of each unittype
;;;  collect trees into single file
;;;
;;;  12/01/06 non-cv non-stepwise is now the default, its fastest and
;;;           gives better results (for slt) than the cuter techniques
;;;  17/01/06 make F0, Duration, and Spectral models different and
;;;           dump all the data at the same time
;;;  25/02/06 trajectory modeling
;;;  03/03/06 trajectory_ola modeling
;;;  29/05/06 cga: adaptation/convertion filter
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(require_module 'clunits)  ;; C++ modules support
(require 'clunits)         ;; run time scheme support
(require 'clunits_build)   ;; Mostly similar to non-parametric clustering

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;;  ClusterGen build stuff
;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(set! cluster_feature_filename "mcep.desc")
;(set! cluster_feature_filename "mceptraj.desc")
(defvar cg_predict_unvoiced t)
(defvar clustergen_mcep_trees nil)
(defvar cg::trajectory_ola nil)

(define (build_clustergen file)
  "(build_clustergen file)
Build cluster synthesizer for the given recorded data and domain."
  (set! datafile file)
  (build_clunits_init file)
  (do_clustergen file)
)

(define (do_clustergen datafile)
  (let ()

    (set_backtrace t)
    (set! clustergen_params 
          (append
           (list
            '(clunit_relation mcep)
            '(wagon_cluster_size_mcep 70) ;; 70 normally
            '(wagon_cluster_size_f0 200)
            )
           clunits_params))

    ;; New technique that does it utt by utt
    (set! cg::unittypes nil)
    (set! cg::durstats nil)
    (set! cg::unitid 0)

    (mapcar
     (lambda (f)
       (format t "%s Processing\n" f)
       (set! track_info_fd 
             (fopen (format nil "festival/coeffs/%s.mcep" f) "w"))
       (set! feat_info_fd 
             (fopen (format nil "festival/coeffs/%s.feats" f) "w"))
       (unwind-protect
        (begin
          (set! utt (utt.load nil (format nil "festival/utts/%s.utt" f)))
          (clustergen::load_hmmstates_utt utt clustergen_params)
          (clustergen::load_ccoefs_utt utt clustergen_params)
          (clustergen::collect_prosody_stats utt clustergen_params)
          (utt.save utt (format nil "festival/utts_hmm/%s.utt" f))
          (clustergen::name_units utt clustergen_params)
          (clustergen::dump_vectors_and_feats_utt utt clustergen_params))
        )
       (fclose track_info_fd)
       (fclose feat_info_fd)
       t
       )
     (cadr (assoc 'files clustergen_params)))

    ;; Build three models

;    ;; Duration model
;    (format t "Building duration model\n")
;    (clustergen::extract_unittype_dur_files datafile cg::unittypes)
;    (clustergen::do_dur_clustering
;     (mapcar car cg::unittypes) clustergen_params cg_build_tree)
;    (clustergen:collect_trees cg::unittypes clustergen_params "dur")

    (set! f0_desc_fd (fopen "festival/clunits/f0.desc" "wb"))
    (pprintf
     (cons '(f0 float) 
            (cdr (car (load "festival/clunits/mcep.desc" t)))
            )
     f0_desc_fd)
    (fclose f0_desc_fd)

    (format t "Extracting features by unittype\n")
    (clustergen::extract_unittype_all_files datafile cg::unittypes)

;    (clustergen::extract_unittype_f0_files datafile cg::unittypes)

    ;; F0 model
    (format t "Building F0 model\n")
    (clustergen::do_clustering
     cg::unittypes clustergen_params 
     clustergen_build_f0_tree "f0")
    (clustergen:collect_trees cg::unittypes clustergen_params "f0")

    ;; Spectral model
    (format t "Building spectral model\n")

;    (clustergen::extract_unittype_mcep_files datafile cg::unittypes)

    (clustergen::do_clustering 
     cg::unittypes clustergen_params 
     clustergen_build_mcep_tree "mcep")
    (clustergen:collect_mcep_trees cg::unittypes clustergen_params)

    (format t "Tree models and vector params dumped\n")
    
  )
)

(define (build_clustergen_traj file)
  "(build_clustergen_traj file)
Build cluster synthesizer for the given recorded data and domain:
trajectory model."
  (set! datafile file)
  (build_clunits_init file)
  (do_clustergen_traj file)
)

(define (do_clustergen_traj datafile)
  (let ()

    (set_backtrace t)
    (set! cg::trajectory t)
;    (set! cg::trajectory_ola nil)
    (set! clustergen_mcep_trees '((cg::trajectory t)))
    (if cg::trajectory_ola
        (set! clustergen_mcep_trees 
              (append clustergen_mcep_trees
                      '((cg::trajectory_ola t)))))
    (set! clustergen_params 
          (append
           (list
            '(clunit_relation HMMstate)
            '(wagon_cluster_size_mcep 10)
            '(wagon_cluster_size_f0 20)
            )
           clunits_params))

    ;; Do processing utt by utt to keep memory requirements small
    (set! cg::unittypes nil)
    (set! cg::durstats nil)
    (set! cg::unitid 0)
    (set! cg::global_frame_pos 0)

    (mapcar
     (lambda (f)
       (format t "%s Processing trajectories\n" f)
       (set! track_info_fd 
             (fopen (format nil "festival/coeffs/%s.mcep" f) "w"))
       (set! feat_info_fd 
             (fopen (format nil "festival/coeffs/%s.feats" f) "w"))
       (unwind-protect
        (begin
          (set! utt (utt.load nil (format nil "festival/utts/%s.utt" f)))
          (clustergen::load_hmmstates_utt utt clustergen_params)
          (clustergen::load_ccoefs_utt utt clustergen_params)
          (clustergen::collect_prosody_stats utt clustergen_params)
          (utt.save utt (format nil "festival/utts_hmm/%s.utt" f))
          (clustergen_traj::name_units utt clustergen_params)
          (clustergen_traj::dump_vectors_and_feats_utt utt clustergen_params))
        )
       (fclose track_info_fd)
       (fclose feat_info_fd)
       t
       )
     (cadr (assoc 'files clustergen_params)))

    ;; Build three models

;    ;; Duration model
;    (format t "Building duration model\n")
;    (clustergen::extract_unittype_dur_files datafile cg::unittypes)
;    (clustergen::do_dur_clustering
;     (mapcar car cg::unittypes) clustergen_params cg_build_tree)
;    (clustergen:collect_trees cg::unittypes clustergen_params "dur")

    (set! f0_desc_fd (fopen "festival/clunits/f0.desc" "wb"))
    (pprintf
     (cons '(f0 float) 
            (cdr (car (load "festival/clunits/mcep.desc" t)))
            )
     f0_desc_fd)
    (fclose f0_desc_fd)

    (format t "Extracting features by unittype\n")
    (clustergen::extract_unittype_all_files_traj datafile cg::unittypes)

;    (clustergen::extract_unittype_f0_files datafile cg::unittypes)

    ;; F0 model
;    (format t "Building F0 model\n")
;    (clustergen::do_clustering
;     cg::unittypes clustergen_params 
;     clustergen_build_f0_tree "f0")
;    (clustergen:collect_trees cg::unittypes clustergen_params "f0")

    ;; Spectral model
    (format t "Building trajectory spectral model\n")

;    (clustergen::extract_unittype_mcep_files datafile cg::unittypes)

    (clustergen::do_clustering 
     cg::unittypes clustergen_params 
     clustergen_build_mcep_trajectory_tree "mcep")

    (clustergen:collect_mcep_trajectory_trees cg::unittypes clustergen_params)

    (format t "Tree models and trajectory params dumped\n")
    
  )
)

(define (cg_remove_featdescs l rfl)
  (cond
   ((null l) l)
   ((string-equal (car rfl) (caar l))
    (cdr l))
   (t
    (cons (car l) (cg_remove_featdescs (cdr l) rfl))))
)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Duration and F0 stats collection
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (get_phone_data phone)
  (let ((a (assoc phone cg::durstats)))
    (if a
	(cadr a)
	(begin ;; first time for this phone
	  (set! cg::durstats
		(cons (list phone (suffstats.new)) cg::durstats))
	  (car (cdr (assoc phone cg::durstats)))))))

(define (duration i)
  ;; for any item
  (if (item.prev i)
      (- (item.feat i "end") (item.feat i "p.end"))
      (item.feat i "end")))

(define (cummulate_duration utt durrelation)

  (mapcar
   (lambda (s)
     (suffstats.add 
      (get_phone_data (item.name s))
      (duration s)))
   (utt.relation.items utt durrelation))
  t)

(define (clustergen::collect_prosody_stats utt params)

  (cummulate_duration utt "HMMstate")

  t
)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (clustergen::name_units utt params)
  (let ((cg_name_feat (get_param 'clunit_name_feat params "name"))
        (cg_relation (get_param 'clunit_relation params "Segment")))
    ;; note name_units and dump_vectors_and_feats must traverse
    ;; the units in the same order
    (mapcar
     (lambda (s)
       (let ((cname (item.feat s cg_name_feat)))
         (if (or (string-equal cname "0")
                 (string-equal cname "ignore"))
             t ;; do nothing
             (begin
               (item.set_feat s "clunit_name" cname)
               (item.set_feat s "unitid" cg::unitid)
               (set! cg::unitid (+ 1 cg::unitid))
               (let ((p (assoc cname cg::unittypes)))
		    (if p
                        (begin
                          (set! occurid (+ 1 (cadr p)))
                          (set-cdr! p (cons occurid nil)))
                        (begin
                          (set! occurid 0)
                          (set! cg::unittypes
			      (cons (list cname occurid) cg::unittypes)))
                        ))
               (item.set_feat s "occurid" occurid)
               ))
         t))
     (reverse (utt.relation.items utt cg_relation))
     )
    t))

(define (clustergen::dump_vectors_and_feats_utt utt params)
  (let ((cg_relation (get_param 'clunit_relation params "Segment"))
        (cg_mcep (utt.feat utt "param_track"))
        (feats (car (cdr (assoc 'feats params))))
        (cname "ignore"))
    ;; note name_units and dump_vectors_and_feats must traverse
    ;; the units in the same order
    (set! cg_mcep_channels (track.num_channels cg_mcep))
    (set! last_frame_number 0)
  (mapcar 
   (lambda (s)
     (set! cname (item.feat s "clunit_name"))
     (if (or (string-equal cname "0")
             (string-equal cname "ignore"))
         t ;; do nothing
         (begin
           ;; Vector values
           (format track_info_fd "%s " cname)
           (set! f 0)
           (set! frame_number (item.feat s "frame_number"))
           (while (< f cg_mcep_channels)
              (format track_info_fd "%f "
                      (track.get cg_mcep frame_number f))
              (set! f (+ 1 f)))
           ;; deltas
;           (set! f 0)
;           (while (< f cg_mcep_channels)
;              (format track_info_fd "%f "
;                       (- (track.get cg_mcep frame_number f)
;                          (track.get cg_mcep last_frame_number f)
;;                          )
;                       )
;              (set! f (+ 1 f)))
;           (set! last_frame_number frame_number)
           (format track_info_fd "\n")
           ;; Feature values
           (format feat_info_fd "%s " cname)
           (mapcar 
            (lambda (f)
              (set! fval (unwind-protect (item.feat s f) "0"))
              (if (string-matches fval " *")
                  (format feat_info_fd "%l " fval)
                  (format feat_info_fd "%s " fval)))
            feats)
           (format feat_info_fd "\n")
           ))
     t)
   (reverse (utt.relation.items utt cg_relation))
   )
  t)
)

(define (clustergen_traj::name_units utt params)
  (let ((cg_name_feat (get_param 'clunit_name_feat params "name"))
        (cg_relation (get_param 'clunit_relation params "Segment")))
    ;; note name_units and dump_vectors_and_feats must traverse
    ;; the units in the same order
    (mapcar
     (lambda (s)
       (let ((cname (item.feat s cg_name_feat)))
         (if (or (string-equal cname "0")
                 (string-equal cname "ignore")
                 (null (item.relation.daughters s 'mcep_link)))
             t ;; do nothing
             (begin
               (item.set_feat s "clunit_name" cname)
               (item.set_feat s "unitid" cg::unitid)
               (set! cg::unitid (+ 1 cg::unitid))
               (let ((p (assoc cname cg::unittypes))
                     (l 
                      (if (assoc 'cg::trajectory_ola clustergen_mcep_trees)
                          (+ (length (item.relation.daughters s 'mcep_link))
                             2
                             (if (item.prev s)
                                 (length (item.relation.daughters 
                                          (item.prev s) 'mcep_link))
                                 0))
                          (length (item.relation.daughters s 'mcep_link)))))
		    (if p
                        (begin
                          (set! occurid (cadr p))
                          (set-cdr! 
                           p 
                           (cons 
                            (+ l occurid)
                            nil)))
                        (begin
                          (set! occurid 0)
                          (set! cg::unittypes
			      (cons 
                               (list 
                                cname 
                                (+ l occurid))
                               cg::unittypes)))
                        ))
               (item.set_feat s "occurid" occurid)
               ))
         t))
     (reverse (utt.relation.items utt cg_relation))
     )
    t))

(define (clustergen_traj::dump_vectors_and_feats_utt utt params)
  (let ((cg_relation (get_param 'clunit_relation params "Segment"))
        (cg_mcep (utt.feat utt "param_track"))
        (feats (car (cdr (assoc 'feats params))))
        (cname "ignore"))
    ;; note name_units and dump_vectors_and_feats must traverse
    ;; the units in the same order
    (set! cg_mcep_channels (track.num_channels cg_mcep))
    (set! last_frame_number 0)
  (mapcar 
   (lambda (s)
     (set! cname (item.feat s "clunit_name"))
     (if (or (string-equal cname "0")
             (string-equal cname "ignore")
             (null (item.relation.daughters s 'mcep_link)))
         t ;; do nothing
         (begin
           ;; Vector values
           (item.set_feat s "num_frames" 0)
           (item.set_feat s "global_frame_pos" cg::global_frame_pos)
           (if (assoc 'cg::trajectory_ola clustergen_mcep_trees)
               (begin
                 (if (item.prev s)
                     (set! frames (item.relation.daughters (item.prev s) 'mcep_link))
                     (set! frames nil))
                 (set! num_frames (length frames))
                 (item.set_feat s "num_frames" 
                                (+ (item.feat s "num_frames") num_frames 1))
                 (mapcar
                  (lambda (frame)
                    (set! frame_number (item.feat frame "frame_number"))
                    (set! f 0)
                    (format track_info_fd "%s " cname)
                    (while (< f cg_mcep_channels)
                           (format track_info_fd "%f "
                                   (track.get cg_mcep frame_number f))
                           (set! f (+ 1 f)))
                    (format track_info_fd "\n")
                    (set! cg::global_frame_pos (+ 1 cg::global_frame_pos))
                    )
                  frames)
                 ;; mid point marker
                 (set! f 0)
                 (format track_info_fd "%s " cname)
                 (while (< f cg_mcep_channels)
                           (format track_info_fd "-1.0 ")
                           (set! f (+ 1 f)))
                 (format track_info_fd "\n")
                 (set! cg::global_frame_pos (+ 1 cg::global_frame_pos))
                 ))

           ;; rest of the frames
           (set! frames (item.relation.daughters s 'mcep_link))
           (set! num_frames (length frames))
           (item.set_feat 
            s "num_frames" 
            (+ (item.feat s "num_frames") num_frames 
               (if (assoc 'cg::trajectory_ola clustergen_mcep_trees)
                   1
                   0)))
           (mapcar
            (lambda (frame)
              (set! f 0)
              (set! frame_number (item.feat frame "frame_number"))
              (format track_info_fd "%s " cname)
              (while (< f cg_mcep_channels)
                     (format track_info_fd "%f "
                             (track.get cg_mcep frame_number f))
                     (set! f (+ 1 f)))
              (format track_info_fd "\n")
              (set! cg::global_frame_pos (+ 1 cg::global_frame_pos))
              )
            frames)
           ;; Feature values
           (format feat_info_fd "%s " cname)
           (mapcar 
            (lambda (f)
              (set! fval (unwind-protect (item.feat s f) "0"))
              (if (string-matches fval " *")
                  (format feat_info_fd "%l " fval)
                  (format feat_info_fd "%s " fval)))
            feats)
           (format feat_info_fd "\n")
;           (set! cg::global_frame_pos (+ 1 cg::global_frame_pos))
           (if (assoc 'cg::trajectory_ola clustergen_mcep_trees)
               (begin
                 (set! f 0)
                 (format track_info_fd "%s " cname)
                 (while (< f cg_mcep_channels)
                        (format track_info_fd "-2.0 ")
                        (set! f (+ 1 f)))
                 (format track_info_fd "\n")
                 (set! cg::global_frame_pos (+ 1 cg::global_frame_pos))))
           ))
     t)
   (reverse (utt.relation.items utt cg_relation))
   )
  t)
)

(define (clustergen::load_hmmstates_utt u params)
"(clustergen::load_hmmstates_utt utterances params)
Load in the labels from from a different label dir.  This assumes 
HMM state sized labels, but it doesn't really care."
   (utt.relation.create u "HMMstate")
   (utt.relation.create u "segstate")
   (utt.relation.load u "HMMstate"
                      (format nil "lab/%s.sl" 
                              (utt.feat u "fileid")))
   ;; Link HMMstate labels to Segment labels
   (set! seg (utt.relation.first u 'Segment))
   (set! state (utt.relation.first u 'HMMstate))
   (while seg
          (set! segstate (utt.relation.append u 'segstate seg))
          (set! segname (item.name seg))
          (set! seg_end (item.feat seg "end"))

          (while (and state (<= (item.feat state "end") seg_end))
             (item.append_daughter segstate state)
             ;; upto space because the label reading code forces this
             (item.set_name 
              state (format nil "%s_%s" segname 
                            (string-before (item.name state) " ")
                            ))
;             (format t "awb_debug %s %f %f\n" (item.name state) 
;                     (item.feat state "end") (item.feat seg "end"))
             (set! state (item.next state)))

          (set! seg (item.next seg))
          (if seg (set! seg_end (item.feat seg "end")))
          )

   ;; Dispose of trailing end states (probably pauses)
   (while state
     (set! next_state (item.next state))
     (item.delete state)
     (set! state next_state))

   t)

(define (clustergen::load_ccoefs_utt utt params)
  "(load_ccoefs utt params) 
Load Combined Coefficients into this utt and link it in"
  (let ( (ccoefs (track.load (format nil "ccoefs/%s.mcep" (utt.feat utt "fileid"))))
         (clunit_name_feat (get_param 'clunit_name_feat params "name"))
         (x 0))
    
    (utt.relation.create utt 'mcep)
    (utt.relation.create utt 'mcep_link)
    (utt.set_feat utt "param_track" ccoefs)
    (set! param_track_num_frames (track.num_frames ccoefs))
    (utt.set_feat utt "param_track_num_frames"
                  param_track_num_frames)

    (set! states (utt.relation.items utt 'HMMstate))
    (set! mcep_pos 0)
    (set! x 0)

    (while states
      (set! end (item.feat (car states) "end"))
      (set! mcep_parent (utt.relation.append utt 'mcep_link (car states)))
      (while (and 
              (<= mcep_pos (+ 0.0001 end))  ;; floating point precision problem
              (< x param_track_num_frames))
             (set! mcep_item (utt.relation.append utt 'mcep))
             (item.append_daughter mcep_parent mcep_item)
             (item.set_feat mcep_item "frame_number" x)
             (item.set_feat mcep_item "name" (item.name mcep_parent))
             (item.set_feat mcep_item "time" (track.get_time ccoefs x))
             (set! mcep_pos (track.get_time ccoefs x))
             (set! x (+ 1 x)))
      (set! states (cdr states)))
    utt))

(define (clustergen::extract_unittype_mcep_files datafile unittypes)
  ;; For each unittype extract their type specific vectors and 
  ;; feats and put them in a unittype specific file.  This additional
  ;; step is required in the utt-by-utt processing model
  (mapcar
   (lambda (unittype)
     (format t "extracting MCEP %d vectors and feature lists for %s\n" 
             (+ 1 (cadr unittype)) (car unittype))
     (system 
      ;; Get track info
      (format 
       nil 
       "$FESTVOXDIR/src/clustergen/cg_get_track %s %s festival/disttabs/%s.mcep\n"
       datafile
       (car unittype)
       (car unittype)))
     ;; Get feats info
     (system 
      (format 
       nil 
       "$FESTVOXDIR/src/clustergen/cg_get_feats %s %s festival/feats/%s.feats\n"
       datafile
       (car unittype)
       (car unittype)))
     )
   unittypes)
  t)

(define (clustergen::extract_unittype_all_files datafile unittypes)
  ;; For each unittype extract their type specific vectors and 
  ;; feats and put them in a unittype specific file.  This additional
  ;; step is required in the utt-by-utt processing model

  (system
   (format 
    nil 
    "$CLUSTERGENDIR/cg_get_feats_all %s\n"
    datafile))
  t)

(define (clustergen::extract_unittype_all_files_traj datafile unittypes)
  ;; For each unittype extract their type specific vectors and 
  ;; feats and put them in a unittype specific file.  This additional
  ;; step is required in the utt-by-utt processing model

  (system
   (format nil 
    "$CLUSTERGENDIR/cg_get_feats_all %s\n"
    datafile))

  (system
   (format nil 
    "$CLUSTERGENDIR/cg_get_feats_all_traj %s\n"
    datafile))

  t)

(define (clustergen::extract_unittype_f0_files datafile unittypes)
  ;; For each unittype extract their type specific vectors and 
  ;; feats and put them in a unittype specific file.  This additional
  ;; step is required in the utt-by-utt processing model
  (mapcar
   (lambda (unittype)
     (format t "extracting %d F0 feature lists for %s\n" 
             (+ 1 (cadr unittype)) (car unittype))
     (system 
      (format 
       nil 
       "$CLUSTERGENDIR/cg_get_f0_feats %s %s festival/feats/%s_f0.feats\n"
       datafile
       (car unittype)
       (car unittype)))
     )
   unittypes)
  t)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;  Clustering functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (clustergen::do_clustering unittypes cg_params build_tree type)
  "(clustergen::do_clustering unittypes cg_params)
Cluster different unit types."
  (mapcar
   (lambda (unittype)
     (format t "Clustergen %s tree build on: %s\n" type (car unittype))
     (build_tree (car unittype) cg_params)
     t)
   unittypes)
  t)

(defvar wagon-balance-size 0)

(define (clustergen_build_mcep_tree unittype cg_params)
"Build tree with Wagon for this unittype."
;;; AWB DEBUG added -track_end 14 -- not general
  (let ((command 
	 (format nil "%s %s -track_start 1 -vertex_output best -desc %s -data '%s' -test '%s' -balance %s -track '%s' -stop %s -output '%s' %s"
		 (get_param 'wagon_progname cg_params "$ESTDIR/bin/wagon")
;                 "-stepwise -swopt rmse"
                 ""  ;; seems to be better with recent tests 12/01/06
                 (get_param 'wagon_field_desc cg_params "wagon")
                 (format nil "festival/feats/%s.feats" ;; .train
                         unittype)
                 (format nil "festival/feats/%s.feats" ;; .test
                         unittype)
		 (get_param 'wagon_balance_size cg_params 0)
                 (format nil "festival/disttabs/%s.mcep" unittype)
		 (get_param 'wagon_cluster_size_mcep cg_params 100)
                 (format nil "festival/trees/%s_mcep.tree" unittype)
		 (get_param 'wagon_other_params cg_params "")
		 )))
;;    Needed if you want to do stepwise
;     (system (format nil
;                     "./bin/traintest %s\n"
; 		 (string-append 
; 		  (get_param 'db_dir cg_params "./")
; 		  (get_param 'feats_dir cg_params "festival/feats/")
; 		  unittype
; 		  (get_param 'feats_ext cg_params ".feats"))))
        (format t "%s\n" command)
        (system command)))

(define (clustergen_build_mcep_trajectory_tree unittype cg_params)
"Build tree with Wagon for this unittype."
  (let ((command 
	 (format nil "%s %s -track_start 0 -desc %s -data '%s' -test '%s' -balance %s -track '%s' -unittrack '%s' -stop %s -output '%s' %s"
		 (get_param 'wagon_progname cg_params "wagon")
;                 "-stepwise -swopt rmse"
                 ""  ;; seems to be better with recent tests 12/01/06
                 (get_param 'wagon_field_desc cg_params "wagon")
                 (format nil "festival/feats/%s.feats" ;; .train
                         unittype)
                 (format nil "festival/feats/%s.feats" ;; .test
                         unittype)
		 (get_param 'wagon_balance_size cg_params 0)
                 (format nil "festival/disttabs/%s.mcep" unittype)
                 (format nil "festival/disttabs/%s.idx" unittype)
		 (get_param 'wagon_cluster_size_mcep cg_params 100)
                 (format nil "festival/trees/%s_mcep.tree" unittype)
		 (get_param 'wagon_other_params cg_params "")
		 )))
;;    Needed if you want to do stepwise
;     (system (format nil
;                     "./bin/traintest %s\n"
; 		 (string-append 
; 		  (get_param 'db_dir cg_params "./")
; 		  (get_param 'feats_dir cg_params "festival/feats/")
; 		  unittype
; 		  (get_param 'feats_ext cg_params ".feats"))))
    (format t "%s\n" command)
    (system command)))

(define (clustergen_build_f0_tree unittype cg_params)
"Build tree with Wagon for this unittype."
  (let ((command 
	 (format nil "%s %s -desc %s -data '%s' -test '%s' -balance %s -stop %s -output '%s' %s"
		 (get_param 'wagon_progname cg_params "wagon")
;                 "-stepwise -swopt rmse"
                 "-ignore '(R:mcep_link.parent.lisp_duration)'"
                 (format nil "festival/clunits/f0.desc")
                 (format nil "festival/feats/%s_f0.feats" ;; .train
                         unittype)
                 (format nil "festival/feats/%s_f0.feats" ;; .test
                         unittype)
		 (get_param 'wagon_balance_size cg_params 0)
		 (get_param 'wagon_cluster_size_f0 cg_params 200)
                 (format nil "festival/trees/%s_f0.tree" unittype)
		 (get_param 'wagon_other_params cg_params "")
		 )))
;;    Needed if you want to do stepwise
;     (system (format nil
;                     "./bin/traintest %s\n"
; 		 (string-append 
; 		  (get_param 'db_dir cg_params "./")
; 		  (get_param 'feats_dir cg_params "festival/feats/")
; 		  unittype
; 		  (get_param 'feats_ext cg_params ".feats"))))
    (format t "%s\n" command)
    (system command)))

(define (clustergen:collect_trees unittypes params type)
"Collect the trees into one file as an assoc list, and dump leafs into
a track file"
  (let ((fd (fopen 
             (format nil "festival/trees/%s_%s.tree"
	      (get_param 'index_name params "all.") type)
	      "wb")))
    (format fd ";; Autogenerated list of clustergen %s trees\n" type)
    (mapcar
     (lambda (unit)
       (set! tree (car (load (string-append "festival/trees/"
                                            (car unit) 
                                            "_" type
                                            ".tree") t)))
       (pprintf (list (car unit) tree) fd))
     unittypes)
    (format fd "\n")
    (fclose fd)
    ))

(define (clustergen:collect_mcep_trees unittypes params)
"Collect the trees into one file as an assoc list, and dump leafs into
a track file"
  (let ((fd (fopen 
             (format nil "festival/trees/%s_%s.tree"
	      (get_param 'index_name params "all.") "mcep")
	      "wb"))
        (rawtrackfd
         (fopen 
             (format nil "festival/trees/%s_%s.rawparams"
	      (get_param 'index_name params "all.") "mcep")
	      "wb")))
    (format fd ";; Autogenerated list of clustergen mcep trees\n")
    (set! vector_num 0)
    (mapcar
     (lambda (unit)
       (set! tree (car (load (string-append "festival/trees/"
                                            (car unit) 
                                            "_mcep" 
                                            ".tree") t)))
       (set! tree (clustergen::dump_tree_vectors tree rawtrackfd))
       (pprintf (list (car unit) tree) fd))
     unittypes)
    (format fd "\n")
    (fclose fd)
    (fclose rawtrackfd)
    ;; need to convert rawtrack to a headered track
    (system
     (format 
      nil
      "$ESTDIR/bin/ch_track -itype ascii -otype est_binary -s 0.005 -o %s %s"
      (format nil "festival/trees/%s_%s.params"
	      (get_param 'index_name params "all.") "mcep")
      (format nil "festival/trees/%s_%s.rawparams"
	      (get_param 'index_name params "all.") "mcep")))

    (format t "%d unittypes as %d subunittypes dumped\n"
            (length unittypes) vector_num)
    ))


(define (clustergen::dump_tree_vectors tree rawtrackfd)
"(clustergen::dump_tree_vectors tree rawtrackfd)
Dump the means and stds at each leaf into an ascii file 
replacing the leaf node with an index"
(cond
 ((cdr tree)  ;; a question
  (list
   (car tree)
   (clustergen::dump_tree_vectors (car (cdr tree)) rawtrackfd)
   (clustergen::dump_tree_vectors (car (cdr (cdr tree))) rawtrackfd)))
 (t           ;; tree leaf
  (mapcar
   (lambda (x) 
     ;; dump the mean and the std
     (format rawtrackfd "%f %f " 
             (if (string-equal "nan" (car x) )
                 0.0
                 (car x))
             (if (string-equal "nan" (cadr x) )
                 0.0
                 (cadr x))))
   (caar tree))
  (format rawtrackfd "\n")
  (set-car! (car tree) vector_num) ;; replace list of mean/stddev with index #
  (set! vector_num (+ 1 vector_num))
  tree)))

(define (clustergen:collect_mcep_trajectory_trees unittypes params)
"Collect the trees into one file as an assoc list, and dump leafs into
a track file"
  (let ((fd (fopen 
             (format nil "festival/trees/%s_%s.tree"
	      (get_param 'index_name params "all.") "mcep")
	      "wb"))
        (rawtrackfd
         (fopen 
             (format nil "festival/trees/%s_%s.rawparams"
	      (get_param 'index_name params "all.") "mcep")
	      "wb")))
    (format fd ";; Autogenerated list of clustergen mcep trees\n")
    (set! vector_num 0)
    (format fd "(cg::trajectory t)\n")
    (if cg::trajectory_ola
        (format fd "(cg::trajectory_ola t)\n"))
    (mapcar
     (lambda (unit)
       (set! tree (car (load (string-append "festival/trees/"
                                            (car unit) 
                                            "_mcep" 
                                            ".tree") t)))
       (set! tree (clustergen::dump_tree_trajectories tree rawtrackfd))
       (pprintf (list (car unit) tree) fd))
     unittypes)
    (format fd "\n")
    (fclose fd)
    (fclose rawtrackfd)
    ;; need to convert rawtrack to a headered track
    (system
     (format 
      nil
      "$ESTDIR/bin/ch_track -itype ascii -otype est_binary -s 0.005 -o %s %s"
      (format nil "festival/trees/%s_%s.params"
	      (get_param 'index_name params "all.") "mcep")
      (format nil "festival/trees/%s_%s.rawparams"
	      (get_param 'index_name params "all.") "mcep")))

    (format t "%d unittypes as %d subunittypes dumped\n"
            (length unittypes) vector_num)
    ))

(define (clustergen::dump_tree_trajectories tree rawtrackfd)
"(clustergen::dump_tree_vectors tree rawtrackfd)
Dump the means and stds at each leaf into an ascii file 
replacing the leaf node with an index"
(cond
 ((cdr tree)  ;; a question
  (list
   (car tree)
   (clustergen::dump_tree_trajectories (car (cdr tree)) rawtrackfd)
   (clustergen::dump_tree_trajectories (car (cdr (cdr tree))) rawtrackfd)))
 (t           ;; tree leaf
  (set! trajectory_length (length (caar tree)))
  (mapcar
   (lambda (v)
     (mapcar
      (lambda (x) 
        ;; dump the mean and the std
        (format rawtrackfd "%f %f " 
                (if (string-equal "nan" (car x) ) 0.0 (car x))
               (if (string-equal "nan" (cadr x) ) 0.0 (cadr x))))
      v)
     (format rawtrackfd "\n"))
   (caar tree)) ;; AWB DEBUG change this for psynch options
  (set-car! (car tree) ;; replace list of mean/stddev with index # + length
            (list vector_num trajectory_length))
  (set! vector_num (+ vector_num trajectory_length))
  tree)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;  Old functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (clustergen_build_tree_cv unittype cg_params)
"Build tree with Wagon for this unittype with cross-validation."
  (let ((command 
	 (format nil "$CLUSTERGENDIR/wagon_cv %s -desc %s -balance 0 -track '%s' -stop %s -output '%s' %s"
		 (string-append 
		  (get_param 'db_dir cg_params "./")
		  (get_param 'feats_dir cg_params "festival/feats/")
		  unittype
		  (get_param 'feats_ext cg_params ".feats"))
		 (if (probe_file
		      (string-append
		       (get_param 'db_dir cg_params "./")
		       (get_param 'wagon_field_desc cg_params "wagon")
		       "." unittype))
		     ;; So there can be unittype specific desc files
		     (string-append
		       (get_param 'db_dir cg_params "./")
		       (get_param 'wagon_field_desc cg_params "wagon")
		       "." unittype)
		     (string-append
		       (get_param 'db_dir cg_params "./")
		       (get_param 'wagon_field_desc cg_params "wagon")))
		 (string-append 
		  (get_param 'db_dir cg_params "./")
		  (get_param 'vector_dir cg_params "festival/disttabs/")
		  unittype
		  (get_param 'vector_ext cg_params ".mcep"))
		 (get_param 'wagon_cluster_size cg_params 20)
		 (string-append 
		  (get_param 'db_dir cg_params "./")
		  (get_param 'trees_dir cg_params "festival/trees/")
		  unittype
		  (get_param 'trees_ext cg_params ".tree"))
		 (get_param 'wagon_other_params cg_params "")
		 )))
    (format t "%s\n" command)
    (system command)))
(defvar cg_build_tree clustergen_build_tree_cv)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;  adaptation/conversion functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (cg_wave_nothing utt) utt)

(define (build_cga_source_files promptfile)

  (set! cluster_synth_method cg_wave_nothing)
  (mapcar
   (lambda (x)
     (format t "cga build source files for %s\n" (car x))
     (set! utt (SynthText (cadr x)))
     (set! param_track (utt.feat utt "param_track"))
     (utt.save utt 
      (format nil "cga/source/utt/%s.utt" (car x)))
     (track.save param_track
      (format nil "cga/source/ccoefs/%s.mcep" (car x)))
     ;; Dump out 5ms labels
     (set! fd (fopen (format nil "cga/source/lab/%s.lab" (car x)) "w"))
     (format fd "#\n")
     (set! frame 0)
     (while (< frame (track.num_frames param_track))
        (format fd "%f 125 %f\n" 
                (track.get_time param_track frame)
                (track.get_time param_track frame)
                )
        (set! frame (+ 1 frame)))
     (fclose fd)
     )
   (load promptfile t))
  t
)

(define (cga_link_coefs utt fileid)
  (let (map_track m mframes p pframes)
    (set! map_track 
          (track.load (format nil "cga/target/ccoefs/%s.mcep" fileid)))
    (utt.set_feat utt "map_track" map_track)
    (set! param_track (utt.feat utt "param_track"))

    (utt.relation.create utt "param_map")
    (utt.relation.create utt "param_map_link")
    (utt.relation.create utt "param_align")
    (utt.relation.load utt "param_align"
        (format nil "cga/target/lab/%s.lab" fileid))
  
    (set! m 0)
    (set! mframes (track.num_frames map_track))
    (set! p 0)
    (set! pframes (track.num_frames param_track))
    (set! mseg (utt.relation.first utt "param_align"))
    (set! pseg (utt.relation.first utt "mcep"))
    
    (while (and mseg (< m mframes))
       ;; Find param_track vector we will map to
       (while (and pseg mseg 
                   (<=
                   (track.get_time param_track (item.feat pseg "frame_number"))
                   (parse-number (item.name mseg))))
          (set! pseg (item.next pseg)))
       (set! end (item.feat mseg "end"))
       (set! mcep_parent (utt.relation.append utt "param_map_link" pseg))
       (set! n m)
       ;; for each mapped frame add to param_map, and link to mcep
       (while (and pseg
                   (< (track.get_time map_track m) end)
                   (< m mframes))
          (set! nseg (utt.relation.append utt "param_map" nil))
          (item.append_daughter mcep_parent nseg)
          (item.set_feat nseg "frame_number" m)
          (item.set_feat nseg "name"
                         (item.feat nseg "R:param_map_link.parent.name")
                         )
          (set! m (+ 1 m))
          )
       (set! mseg (item.next mseg))
    )
  )
)

(define (clustergen_cga::dump_vectors_and_feats_utt utt params)
  (let ((cg_relation (get_param 'clunit_relation params "Segment"))
        (cg_mcep (utt.feat utt "map_track"))
        (feats (car (cdr (assoc 'feats params))))
        (cname "ignore"))
    ;; note name_units and dump_vectors_and_feats must traverse
    ;; the units in the same order
    (set! cg_mcep_channels (track.num_channels cg_mcep))
    (set! last_frame_number 0)
  (mapcar 
   (lambda (s)
     (set! cname (item.feat s "clunit_name"))
     (if (or (string-equal cname "0")
             (string-equal cname "ignore"))
         t ;; do nothing
         (begin
           ;; Vector values
           (format track_info_fd "%s " cname)
           (set! f 0)
           (set! frame_number (item.feat s "frame_number"))
           (while (< f cg_mcep_channels)
              (format track_info_fd "%f "
                      (track.get cg_mcep frame_number f))
              (set! f (+ 1 f)))
           ;; deltas
;           (set! f 0)
;           (while (< f cg_mcep_channels)
;              (format track_info_fd "%f "
;                       (- (track.get cg_mcep frame_number f)
;                          (track.get cg_mcep last_frame_number f)
;;                          )
;                       )
;              (set! f (+ 1 f)))
;           (set! last_frame_number frame_number)
           (format track_info_fd "\n")
           ;; Feature values
           (format feat_info_fd "%s " cname)
           (mapcar 
            (lambda (f)
              (set! fval (unwind-protect (item.feat s f) "0"))
              (if (string-matches fval " *")
                  (format feat_info_fd "%l " fval)
                  (format feat_info_fd "%s " fval)))
            feats)
           (format feat_info_fd "\n")
           ))
     t)
   (reverse (utt.relation.items utt cg_relation))
   )
  t)
)

(define (build_cga_model promptfile)

  (set! cluster_synth_method cg_wave_nothing)
  (set! cga:clustergen_params
        (append
         (list
          '(clunit_relation param_map)
          '(wagon_cluster_size_cga 50)
          '(clunit_name_feat "name")
          )
         clustergen_params))

  (set! cg::unittypes nil)
  (set! cg::durstats nil)
  (set! cg::unitid 0)

  (mapcar
   (lambda (x)
     (format t "%s Processing\n" (car x))
     (set! track_info_fd 
           (fopen (format nil "festival/coeffs/%s.mcep" (car x)) "w"))
     (set! feat_info_fd 
           (fopen (format nil "festival/coeffs/%s.feats" (car x)) "w"))
     (unwind-protect
      (begin
        ;; load source utts
        (set! utt (SynthText (cadr x)))
        ;; load target coeffs and align them
        (cga_link_coefs utt (car x))
        ;; dump features -- like clustergen
        (clustergen::name_units utt cga:clustergen_params)
        (clustergen_cga::dump_vectors_and_feats_utt utt cga:clustergen_params)
      ))
     (fclose track_info_fd)
     (fclose feat_info_fd)
     )
   (load promptfile t))

   (format t "Extracting features by unittype\n")
   (clustergen::extract_unittype_all_files promptfile cg::unittypes)

   (cga:get_prosody_stats)

   (clustergen::do_clustering
    cg::unittypes cga:clustergen_params
    clustergen_build_cga_tree "cga")
   (clustergen:collect_cga_trees cg::unittypes cga:clustergen_params)

  t)

(define (cga:get_prosody_stats)
  (let ((source_f0 (load "etc/f0.params" t))
        (target_f0 (load "cga/target/etc/f0.params" t)))
    (set! cga:source_f0_mean
          (parse-number (string-after (car source_f0) "=")))
    (set! cga:source_f0_stddev
          (parse-number (string-after (cadr source_f0) "=")))
    (set! cga:target_f0_mean
          (parse-number (string-after (car target_f0) "=")))
    (set! cga:target_f0_stddev
          (parse-number (string-after (cadr target_f0) "=")))
    ))

(define (clustergen:collect_cga_trees unittypes params)
"Collect the trees into one file as an assoc list, and dump leafs into
a track file"
  (let ((fd (fopen 
             (format nil "festival/trees/%s_%s.tree" 
                     (get_param 'index_name params "all.") "cga")
             "wb"))
        (rawtrackfd
         (fopen 
             (format nil "festival/trees/%s_%s.rawparams"
                     (get_param 'index_name params "all.") "cga")
	      "wb")))
    (format fd ";; Autogenerated list of clustergen cga trees\n")
    (set! vector_num 0)
    (format fd "(cga::source_f0_mean %f)\n" cga:source_f0_mean)
    (format fd "(cga::source_f0_stddev %f)\n" cga:source_f0_stddev)
    (format fd "(cga::target_f0_mean %f)\n" cga:target_f0_mean)
    (format fd "(cga::target_f0_stddev %f)\n" cga:target_f0_stddev)
    (mapcar
     (lambda (unit)
       (set! tree (car (load (string-append "festival/trees/"
                                            (car unit) 
                                            "_cga" 
                                            ".tree") t)))
       (set! tree (clustergen::dump_tree_vectors tree rawtrackfd))
       (pprintf (list (car unit) tree) fd))
     unittypes)
    (format fd "\n")
    (fclose fd)
    (fclose rawtrackfd)
    ;; need to convert rawtrack to a headered track
    (system
     (format 
      nil
      "$ESTDIR/bin/ch_track -itype ascii -otype est_binary -s 0.005 -o %s %s"
      (format nil "festival/trees/%s_%s.params"
	      (get_param 'index_name params "all.") "cga")
      (format nil "festival/trees/%s_%s.rawparams"
	      (get_param 'index_name params "all.") "cga")))

    (format t "%d unittypes as %d vectors dumped\n"
            (length unittypes) vector_num)
    ))

(define (clustergen_build_cga_tree unittype cg_params)
"Build tree with Wagon for this unittype."
  (let ((command 
	 (format nil "%s %s -track_start 1 -desc %s -data '%s' -test '%s' -balance %s -track '%s' -stop %s -output '%s' %s"
		 (get_param 'wagon_progname cg_params "wagon")
;                 "-stepwise -swopt rmse"
                 ""  ;; seems to be better with recent tests 12/01/06
                 (get_param 'wagon_field_desc cg_params "wagon")
                 (format nil "festival/feats/%s.feats" ;; .train
                         unittype)
                 (format nil "festival/feats/%s.feats" ;; .test
                         unittype)
		 (get_param 'wagon_balance_size cg_params 0)
                 (format nil "festival/disttabs/%s.mcep" unittype)
		 (get_param 'wagon_cluster_size_cga cg_params 100)
                 (format nil "festival/trees/%s_cga.tree" unittype)
		 (get_param 'wagon_other_params cg_params "")
		 )))
;;    Needed if you want to do stepwise
;     (system (format nil
;                     "./bin/traintest %s\n"
; 		 (string-append 
; 		  (get_param 'db_dir cg_params "./")
; 		  (get_param 'feats_dir cg_params "festival/feats/")
; 		  unittype
; 		  (get_param 'feats_ext cg_params ".feats"))))
    (format t "%s\n" command)
    (system command)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;  Testing function
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (ClusterGen_test_set filename testdir)
  (set! old_cg_predict_unvoiced cg_predict_unvoiced)
  (set! cg_predict_unvoiced nil)
  (mapcar 
   (lambda (x)
     (format t "CG test %s\n" (car x))
     (set! utt1 (SynthText (cadr x)))
     (utt.save.wave utt1 (format nil "%s/%s.wav" testdir (car x)))
     (track.save
      (utt.feat utt1 "param_track")
      (format nil "%s/%s.mcep" testdir (car x)))
     t)
   (load filename t))
  (set! cg_predict_unvoiced old_cg_predict_unvoiced)
  t)

(define (ClusterGen_test_resynth filename testdir)
  ;; Tests from natural durations

;    (set! traj::clustergen_param_vectors
;          (track.load
;  	 (string-append 
;  	  cmu_us_slt_arctic::dir "/"
;  	  "festival/trees/traj/"
;  	  (get_param 'index_name dt_params "all")
;  	  "_mcep.params")))
;    (set! traj::clustergen_mcep_trees
;          (load (string-append
;                 (string-append 
;                  cmu_us_slt_arctic::dir "/"
;                  "festival/trees/traj/"
;                  (get_param 'index_name dt_params "all")
;                  "_mcep.tree")) t))

  (set! old_cg_predict_unvoiced cg_predict_unvoiced)
  (set! cg_predict_unvoiced nil)
  (mapcar 
   (lambda (x)
     (format t "CG test_resynth %s\n" (car x))
     (gc)
     (set! real_track (track.load (format nil "ccoefs/%s.mcep" (car x))))
     (set! utt1 (utt.load nil (format nil "festival/utts/%s.utt" (car x))))
     (clustergen::load_hmmstates_utt utt1 clustergen_params)
     (clustergen::load_ccoefs_utt utt1 clustergen_params)
     (if (assoc 'cg::trajectory clustergen_mcep_trees)
        (ClusterGen_predict_trajectory utt1) ;; predict trajectory
        (ClusterGen_predict_mcep utt1) ;; predict vector types
;        (ClusterGen_acoustic_predict_mcep utt1 real_track) 
        )
;     (ClusterGen_predict_mcep utt1) ;; predict vector types
;     (ClusterGen_predict_trajectory utt1) ;; predict trajectory

     (track.save (utt.feat utt1 "param_track") 
                 (format nil "%s/%s.mcep" testdir (car x)))
     (if (boundp 'mlsa_resynthesis)
         (begin
           (set! cg_predict_unvoiced t)
           (if (assoc 'cg::trajectory clustergen_mcep_trees)
               (ClusterGen_predict_trajectory utt1) ;; predict trajectory
               (ClusterGen_predict_mcep utt1) ;; predict vector types
;               (ClusterGen_acoustic_predict_mcep utt1 real_track) 
               )
           (set! real_track 
                 (track.load (format nil "ccoefs/%s.mcep" (car x))))
           (set! predicted_track (utt.feat utt1 "param_track"))
           (set! i 0)
           (mapcar
            (lambda (m)
              (if (> (track.get predicted_track i 0) 0)
                  (track.set predicted_track i 0
                             (track.get real_track i 0)))
              (set! i (+ 1 i)))
            (utt.relation.items utt1 'mcep))
           (set! cg_predict_unvoiced nil)
           (wave.save
            (mlsa_resynthesis (utt.feat utt1 "param_track"))
            (format nil "%s/%s.wav" testdir (car x)))
           ))
     t)
   (load filename t))
  (set! cg_predict_unvoiced old_cg_predict_unvoiced)
  t)

(define (ClusterGen_acoustic_predict_mcep utt real_track)
  ;; Acoustically look for best mcep
  (let ((param_track nil)
        (frame_advance cg:frame_shift)
        (frame nil) (f nil) (f0_val)
        (num_channels (/ (track.num_channels clustergen_param_vectors) 2))
        )

    ;; Predict mcep values
    (set! i 0)
    (set! param_track
          (track.resize nil
           (utt.feat utt "param_track_num_frames")
           num_channels))
    (utt.set_feat utt "param_track" param_track)
    (mapcar
     (lambda (mcep)
       ;; Predict mcep frame
       (let ((mcep_tree (assoc_string (item.name mcep) clustergen_mcep_trees))
             (f0_tree (assoc_string (item.name mcep) clustergen_f0_trees)))
         (if (null mcep_tree)
             (format t "ClusterGen: can't find cluster tree for %s\n"
                     (item.name mcep))
             (begin
               ;; F0 prediction
               (set! f0_val (wagon mcep (cadr f0_tree)))
               (ClusterGen_predict_F0 mcep (cadr f0_val) param_track)

               ;; MCEP prediction
;               (format t "checking %s\n" (item.name mcep))
               (set! frame (find_closest_vector 
                            (cadr mcep_tree)
                            (list -1 10000000)
                            i real_track))
;               (format t "   bestest %l\n" frame)
               (set! j 1)
               (set! f (car frame))
               (while (< j num_channels)
                  (track.set param_track i j 
                    (track.get clustergen_param_vectors f (* 2 j)))
                  (set! j (+ 1 j)))))
         
         (track.set_time param_track i (* i frame_advance))
         (set! i (+ 1 i))))
     (utt.relation.items utt 'mcep))
    (cg_F0_smooth param_track 0)
    (mapcar
     (lambda (x)
       (cg_mcep_smooth param_track x))
     '( 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25))
    utt
  )
)

(define (find_closest_vector tree bests i track)
  ;; returns (best_index best_distance)
  (cond
   ((cdr tree) ;; a question
    (find_closest_vector
     (car (cdr tree))
     (find_closest_vector
      (car (cdr (cdr tree)))
      bests
      i track)
     i track))
   ((< (set! new_best
             (find_vector_distance 
              (caar tree) clustergen_param_vectors i track
              0 1 (cadr bests)))
       (cadr bests))
;    (format t "   better at %l\n" new_best)
    (list (caar tree) new_best))
   (t
    bests)))

(define (find_vector_distance a tracka b trackb sumx c best_val)
  (cond
   ((> sumx best_val)
    best_val) ;; not better 
   ((> c 24) 
    sumx)     ;; end, is better
   (t
    (set! d (- (track.get tracka a (* 2 c))
               (track.get trackb b c)))
    (find_vector_distance
     a tracka b trackb
     (+ (* d d) sumx)
     (+ 1 c)
     best_val))))

(provide 'clustergen_build)
