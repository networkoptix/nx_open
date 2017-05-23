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
;;;  Run Time Synthesis support for clustergen (HMM-generation) voices    ;;
;;;                                                                       ;;
;;;  This is voice-independant, and should be in festival/lib but is      ;;
;;;  currently copied into each voice                                     ;;
;;;                                                                       ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defvar cluster_synth_pre_hooks nil)
(defvar cluster_synth_post_hooks nil)
(defvar clustergen_mcep_trees nil)
(defvar cg:frame_shift 0.005)

(defSynthType ClusterGen

    (apply_hooks cluster_synth_pre_hooks utt)

    (set! clustergen_utt utt) ;; for debugging

    ;; Build the state relation
    (ClusterGen_make_HMMstate utt)
    ;; Predict number of frames, then predict the frame values
    (ClusterGen_make_mcep utt) ;; durations for # of vectors
    (if (assoc 'cg::trajectory clustergen_mcep_trees)
        (ClusterGen_predict_trajectory utt) ;; predict trajectory
        (ClusterGen_predict_mcep utt) ;; predict vector types
        )

    ;; Convert predicted mcep track into a waveform
    (cluster_synth_method utt)

    (apply_hooks cluster_synth_post_hooks utt)
    utt
)

(define (cg_wave_synth_external utt)
  ;; before we had it built-in to Festival
  (let ((trackname (make_tmp_filename))
        (wavename (make_tmp_filename))
        )
    (track.save (utt.feat utt "param_track") trackname "est")
    (system
     (format nil "$FESTVOXDIR/src/clustergen/cg_resynth %s %s"
             trackname wavename))
    (utt.import.wave utt wavename)
    (delete-file trackname)
    (delete-file wavename)
    utt)
)

(define (cg_wave_synth utt)
    (utt.relation.create utt 'Wave)
    (item.set_feat 
     (utt.relation.append utt 'Wave) 
     "wave" 
     (mlsa_resynthesis (utt.feat utt "param_track")))
    utt)

(define (cg_wave_synth_cga utt)
  ;; Use loaded cga model to predict a new map_track
  (format t "In Synth CGA\n")

  (cga:create_map utt)  ;; generate map_track
  (cga:predict_map utt)
  
  (utt.relation.create utt 'Wave)
  (item.set_feat 
   (utt.relation.append utt 'Wave) 
   "wave" 
   (mlsa_resynthesis (utt.feat utt "map_track")))
  utt)

(define (cga:create_map utt)
  ;; predict the map_param converted parameters

  ;; Need to do better duration stuff
  (set! map_track (track.copy (utt.feat utt "param_track")))
  (utt.set_feat utt "map_track" map_track)

  (utt.relation.create utt "param_map")
  (utt.relation.create utt "param_map_link")

  (set! pseg (utt.relation.first utt "mcep"))
  (set! m 0)
  (while pseg
     (set! mcep_parent (utt.relation.append utt "param_map_link" pseg))
     (set! mseg (utt.relation.append utt "param_map"))
     (item.append_daughter mcep_parent mseg)
     (item.set_feat mseg "frame_number" m)
     (item.set_feat mseg "name"
                         (item.feat mseg "R:param_map_link.parent.name"))
     (set! m (+ 1 m))
     (set! pseg (item.next pseg)))
  utt
)

(define (cga:predict_map utt)
  (let (i j f map_track num_channels
          s_f0_mean s_f0_stddev
          t_f0_mean t_f0_stddev)

  (set! i 0)
  (set! map_track (utt.feat utt "map_track"))
  (set! num_channels (track.num_channels map_track))

  (set! s_f0_mean (get_param 'cga::source_f0_mean clustergen_cga_trees 140))
  (set! s_f0_stddev (get_param 'cga::source_f0_stddev clustergen_cga_trees 20))
  (set! t_f0_mean (get_param 'cga::target_f0_mean clustergen_cga_trees 140))
  (set! t_f0_stddev (get_param 'cga::target_f0_stddev clustergen_cga_trees 20))

  (mapcar
   (lambda (x)
     (let ((map_tree (assoc_string (item.name x) clustergen_cga_trees)))
       (if (null map_tree)
           (format t "ClusterGenCGA: can't find cluster tree for %s\n"
                   (item.name x))
           (begin
             (set! frame (wagon x (cadr map_tree)))
             ;; Convert f0
             (if (> (track.get map_track i 0) 0)
                 (track.set 
                  map_track i 0
                  (+ t_f0_mean
                     (* t_f0_stddev
                        (/ (- (track.get map_track i 0) s_f0_mean)
                           s_f0_stddev)))))
             (set! j 1)
             (set! f (car frame))
             (while (< j num_channels)
                (track.set map_track i j 
                   (track.get clustergen_cga_vectors f (* 2 j)))
                (set! j (+ 1 j)))))
       (set! i (+ 1 i))))
   (utt.relation.items utt "param_map"))

  utt))

(define (ClusterGen_predict_states seg)
  ;; The names may change
  (cdr (assoc_string (item.name seg) phone_to_states)))

(define (ClusterGen_make_HMMstate utt)
  (let ((states)
        (segstate)
        (statepos))
    ;; Make HMMstate relation and items (three per phone)
    (utt.relation.create utt "HMMstate")
    (utt.relation.create utt "segstate")
    
    (mapcar
     (lambda (seg)
       (set! statepos 1)
       (set! states (ClusterGen_predict_states seg))
       (set! segstate (utt.relation.append utt 'segstate seg))
       (while states
          (set! state (utt.relation.append utt 'HMMstate))
          (item.append_daughter segstate state)
          (item.set_feat state "name" (car states))
          (item.set_feat state "statepos" statepos)
          (set! statepos (+ 1 statepos))
          (set! states (cdr states)))
       )
     (utt.relation.items utt 'Segment))
    )
)

(define (ClusterGen_state_duration state)
  (let ((zdur (wagon_predict state duration_cart_tree_cg))
        (ph_info (assoc_string (item.name state) duration_ph_info_cg))
        (seg_stretch (item.feat state "R:segstate.parent.dur_stretch"))
        (syl_stretch (item.feat state "R:segstate.parent.R:SylStructure.parent.dur_stretch"))
        (tok_stretch (item.feat state "R:segstate.parent.R:SylStructure.parent.parent.R:Token.parent.dur_stretch"))
        (global_stretch (Parameter.get 'Duration_Stretch))
        (stretch 1.0))
    (if (string-matches (item.name state) "pau_.*")
        ;; Its a pau so explicitly set the duration
        ;; Note we want sentence internal pauses to be about 100ms
        ;; and sentence final pauses to be 150ms, but there will also
        ;; sentence initial pauses of 150ms so we can treat all pauses as
        ;; 100ms, there are three states so we use 50ms
        (set! zdur 
              (/ (- 0.05 (car (cdr ph_info)))
                 (car (cdr (cdr ph_info))))))
    (if (not (string-equal seg_stretch "0"))
        (setq stretch (* stretch seg_stretch)))
    (if (not (string-equal syl_stretch "0"))
        (setq stretch (* stretch syl_stretch)))
    (if (not (string-equal tok_stretch "0"))
        (setq stretch (* stretch tok_stretch)))
    (if (not (string-equal global_stretch "0"))
        (setq stretch (* stretch global_stretch)))
    (if ph_info
        (* stretch
           (+ (car (cdr ph_info)) ;; mean
              (* (car (cdr (cdr ph_info))) ;; stddev
                 zdur)))
        (begin
          (format t "ClusterGen_state_duration: no dur phone info for %s\n"
                  (item.name state))
          0.1))))

(define (ClusterGen_make_mcep utt)
  (let ((num_frames 0)
        (frame_advance cg:frame_shift)
        (end 0.0))

    ;; Make HMMstate relation and items (three per phone)
    (utt.relation.create utt "mcep")
    (utt.relation.create utt "mcep_link")
    (mapcar
     (lambda (state)
       ;; Predict Duration
       (set! start end)
       (set! end (+ start (ClusterGen_state_duration state)))
       (item.set_feat state "end" end)
       ;; create that number of mcep frames up to state end
       (set! mcep_parent (utt.relation.append utt 'mcep_link state))
       (while (<= (* num_frames frame_advance) end)
              (set! mcep_frame (utt.relation.append utt 'mcep))
              (item.append_daughter mcep_parent mcep_frame)
              (item.set_feat mcep_frame "frame_number" num_frames)
              (item.set_feat mcep_frame "name" (item.name mcep_parent))
              (set! num_frames (+ 1 num_frames))
              )
       )
     (utt.relation.items utt 'HMMstate))

    ;; Copy the final state end back up on to the segment for consistency
    (mapcar
     (lambda (seg)
       (item.set_feat seg "end" (item.feat seg "R:segstate.daughtern.end")))
     (utt.relation.items utt 'Segment))

    (utt.set_feat utt "param_track_num_frames" num_frames)
    utt)
)

(define (mcep_12 i)
  (track.get
   (utt.feat (item.get_utt i) "param_track")
   (item.feat i "frame_number")
   12))
(define (mcep_11 i)
  (track.get
   (utt.feat (item.get_utt i) "param_track")
   (item.feat i "frame_number")
   11))
(define (mcep_10 i)
  (track.get
   (utt.feat (item.get_utt i) "param_track")
   (item.feat i "frame_number")
   10))
(define (mcep_9 i)
  (track.get
   (utt.feat (item.get_utt i) "param_track")
   (item.feat i "frame_number")
   9))
(define (mcep_8 i)
  (track.get
   (utt.feat (item.get_utt i) "param_track")
   (item.feat i "frame_number")
   8))
(define (mcep_7 i)
  (track.get
   (utt.feat (item.get_utt i) "param_track")
   (item.feat i "frame_number")
   7))
(define (mcep_6 i)
  (track.get
   (utt.feat (item.get_utt i) "param_track")
   (item.feat i "frame_number")
   6))
(define (mcep_5 i)
  (track.get
   (utt.feat (item.get_utt i) "param_track")
   (item.feat i "frame_number")
   5))
(define (mcep_4 i)
  (track.get
   (utt.feat (item.get_utt i) "param_track")
   (item.feat i "frame_number")
   4))
(define (mcep_3 i)
  (track.get
   (utt.feat (item.get_utt i) "param_track")
   (item.feat i "frame_number")
   3))
(define (mcep_2 i)
  (track.get
   (utt.feat (item.get_utt i) "param_track")
   (item.feat i "frame_number")
   2))
(define (mcep_1 i)
  (track.get
   (utt.feat (item.get_utt i) "param_track")
   (item.feat i "frame_number")
   1))

(define (mcep_0 i)
  (track.get
   (utt.feat (item.get_utt i) "param_track")
   (item.feat i "frame_number")
   0))

(define (cg_duration i)
  (if (item.prev i)
      (- (item.feat i "end") (item.feat i "p.end"))
      (item.feat i "end")))

(define (cg_state_pos i)
  (let ((n (item.name i)))
  (cond
   ((not (string-equal n (item.feat i "p.name")))
    "b")
   ((string-equal n (item.feat i "n.name"))
    "m")
   (t
    "e"))))

(define (cg_F0_smooth track j)
  (let ((p 0.0)
        (i 0)
        (num_frames (- (track.num_frames track) 1)))

    (set! i 1)
    (while (< i num_frames)
      (set! this (track.get track i j))
      (set! next (track.get track (+ i 1) j))
      (if (> this 0.0)
          (track.set 
           track i j
           (/ (+ (if (> p 0.0) p this)
                 this
                 (if (> next 0.0) next this))
              3.0)))
      (set! p this)
      (set! i (+ 1 i)))
    )
)

(define (cg_mcep_smooth track j)
  (let ((p 0.0)
        (i 0)
        (num_frames (- (track.num_frames track) 1)))

    (set! i 1)
    (while (< i num_frames)
      (set! this (track.get track i j))
      (set! next (track.get track (+ i 1) j))
      (track.set 
       track i j
       (/ (+ p this next) 3.0))
      (set! p this)
      (set! i (+ 1 i)))
    )
)

;; For normal synthesis make unvoiced states unvoiced, but we don't
;; do this during testing
(defvar cg_predict_unvoiced t)

(define (ClusterGen_predict_F0 mcep f0_val param_track)
  "(ClusterGen_predict_F0 mcep frame param_track)
Predict the F0 (or not)."
  (if (and cg_predict_unvoiced
           (string-equal "-"
            (item.feat 
             mcep "R:mcep_link.parent.R:segstate.parent.ph_vc"))
           (string-equal "-"
            (item.feat 
             mcep "R:mcep_link.parent.R:segstate.parent.ph_cvox")))
      (track.set param_track i 0 0.0) ;; make it unvoiced
      (track.set param_track i 0 f0_val)) ;; make it voiced
  )

(define (ClusterGen_predict_mcep utt)
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
               (set! frame (wagon mcep (cadr mcep_tree)))
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

(define (cg_voiced state)
  "(cg_voiced state)
t if this state is voices, nil otherwise."
  (if (and cg_predict_unvoiced
           (string-equal "-" (item.feat state "R:segstate.parent.ph_vc"))
           (string-equal "-" (item.feat state "R:segstate.parent.ph_cvox")))
      nil
      t))

(define (ClusterGen_predict_trajectory utt)
  (let ((param_track nil)
        (frame_advance cg:frame_shift)
        (frame nil) (f nil) (f0_val)
        (num_channels (/ (track.num_channels clustergen_param_vectors) 2))
;        (num_channels (/ (track.num_channels traj::clustergen_param_vectors) 2))
        )

    ;; Predict mcep values
    (set! i 0)
    (set! param_track
          (track.resize nil
           (utt.feat utt "param_track_num_frames")
           num_channels))
    (utt.set_feat utt "param_track" param_track)
;    (set! param_track (utt.feat utt "param_track"))
    (mapcar
     (lambda (state)
       ;; Predict mcep frame
;joint       (let ((mcep_tree (assoc_string (item.name state) traj::clustergen_mcep_trees))
       (let ((mcep_tree (assoc_string (item.name state) clustergen_mcep_trees))
             ;(f0_tree (assoc_string (item.name mcep) clustergen_f0_trees))
             )
         (if (null mcep_tree)
             (format t "ClusterGen: can't find cluster tree for %s\n"
                     (item.name state))
             (begin
               ;; feature prediction (F0 and mcep)
               (set! trajectory (wagon state (cadr mcep_tree)))
               (if (item.relation.daughters state 'mcep_link)
                   (begin
                    (if (assoc 'cg::trajectory_ola clustergen_mcep_trees)
;joint                    (if (assoc 'cg::trajectory_ola traj::clustergen_mcep_trees)
                     (cg:add_trajectory_ola
                      (caar trajectory)
                      (cadr (car trajectory))
                      state
                      num_channels
                      param_track
                      frame_advance)
                     (cg:add_trajectory
                      (caar trajectory)
                      (cadr (car trajectory))
                      state
                      num_channels
                      param_track
                      frame_advance))))
               ))))
     (utt.relation.items utt 'HMMstate))

    (track.save param_track "trajectory.track")

    (cg_F0_smooth param_track 0)
    (mapcar
     (lambda (x)
       (cg_mcep_smooth param_track x))
;     '( 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25)
      '( 1 2 3 )
     )
    utt
  )
)

(define (cg:add_trajectory s_start s_frames state num_channels
                           param_track frame_advance)
"(cg:add_trajectory start n state num_channels)
Add trajectory to daughters of state, interpolating as necessary."
  (let ((j 0) (i 0)
        (mceps (item.relation.daughters state 'mcep_link)))

    (set! t_start (item.feat (car mceps) "frame_number"))
    (set! t_frames (length mceps))
    (set! m (/ s_frames t_frames))
    (set! f 0)

    (while (< i t_frames)
       ;; find f
       (set! s_pos (nint (+ s_start f)))

       (if (cg_voiced state)
           (track.set param_track (+ i t_start) 0
                      (track.get clustergen_param_vectors s_pos 0)))
       (set! j 1)
       (while (< j num_channels)
              (track.set param_track (+ i t_start) j 
;joint               (+ (* 0.35 (track.get param_track (+ i t_start) j))
;                        (* 0.65 (track.get traj::clustergen_param_vectors 
;                                          s_pos (* 2 j)))))
                         (track.get clustergen_param_vectors 
                                    s_pos (* 2 j)))
              (set! j (+ 1 j)))
       (set! f (+ m f))
       (track.set_time param_track 
                       (+ i t_start) (* (+ i t_start) frame_advance))
       (set! i (+ i 1))
       )
    )
  )

(define (cg:add_trajectory_ola s_start s_frames state num_channels
                           param_track frame_advance)
"(cg:add_trajectory start n state num_channels)
Add trajectory to daughters of state, interpolating as necessary."
  (let ((j 0) (i 0) (s1l 0) (s2l 0) (m 0.0) (w 0.0)
        (t_start 0) (t_frames 0) (s_offset 0)
        (mceps1 nil) (mceps2 nil))

    (set! i 0)
    (while (< i s_frames)
     (if (equal? -1.0 (track.get clustergen_param_vectors (+ s_start i) 0))
         (set! s1l i))
     (set! i (+ i 1)))

    (if (and (item.prev state) 
             (item.relation.daughters (item.prev state) 'mcep_link)
             (> s1l 0))
        (begin ;; do overlap on previous 
          (set! mceps1 (item.relation.daughters (item.prev state) 'mcep_link))
          (set! first_half_delta (/ 1.0 (length mceps1)))
          (set! t_start (item.feat (car mceps1) "frame_number"))
          (set! t_frames (length mceps1))
          (set! m (/ s1l t_frames))
          (set! i 0)
          (set! w 0.0)
          (while (< i t_frames)
           (set! s_offset (nint (* i m)))
           (if (not (< s_offset s1l))
               (begin
;                 (format t "boing pre\n")
                 (set! s_offset (- s1l 1))))
           (set! s_pos (+ s_start s_offset))
           (if (< (track.get clustergen_param_vectors s_pos 0) 0)
               (format t "assigning pre -1/-2 %d %d %f\n" s_pos i m))
           ;; F0 Prediction
           (if (cg_voiced state)
               (track.set param_track (+ i t_start) 0
                (+ (* (- 1.0 w) (track.get param_track (+ i t_start) 0))
                   (* w (track.get clustergen_param_vectors s_pos 0)))))

           ;; MCEP Prediction
           (set! j 1)
           (while (< j num_channels)
             (track.set param_track (+ i t_start) j 
              (+ (* (- 1.0 w) (track.get param_track (+ i t_start) j))
                 (* w 
                    (track.get clustergen_param_vectors s_pos (* 2 j))
                    )
                 )
              )
             (set! j (+ 1 j)))
           (set! i (+ 1 i))
           (set! w (+ w first_half_delta))
           (if (> w 1.0) (set! w 1.0))
           )
          ))

    ;; do assignment on current unit 
    (set! mceps2 (item.relation.daughters state 'mcep_link))
    (set! t_start (item.feat (car mceps2) "frame_number"))
    (set! t_frames (length mceps2))
    (set! s2l (- s_frames (+ s1l 2)))
    (set! s2_start (+ s_start s1l 1))
    (set! m (/ s2l t_frames))
    (set! i 0)
    (while (< i t_frames)
     (set! s_offset (nint (* i m)))
     (if (not (< s_offset s2l))
         (set! s_offset (- s2l 1)))
     (set! s_pos (+ s2_start s_offset))
     (if (< (track.get clustergen_param_vectors s_pos 0) 0)
         (format t "assigning -1/-2 %d %d %f %f\n" s_pos i m
                 (track.get clustergen_param_vectors s_pos 0)))
     ;; F0 Prediction
     (if (cg_voiced state)
         (track.set param_track (+ i t_start) 0
                    (track.get clustergen_param_vectors s_pos 0)))
     ;; MCEP Prediction
     (set! j 1)
     (while (< j num_channels)
      (track.set param_track (+ i t_start) j 
                 (track.get clustergen_param_vectors s_pos (* 2 j)))
      (set! j (+ 1 j)))
     (track.set_time param_track 
                     (+ i t_start) (* (+ i t_start) frame_advance))
     (set! i (+ 1 i))
    )
  )
)

;;; For ClusterGen_predict_mcep
;;;   take into account actual and delta and try to combine both
;                   (if (and nil (> i 0))
;                       (begin ;; something a little fancier
;                   (set! m1 (track.get cpv f (* 2 j)))         ;; mean1
;                   (set! s1 (track.get cpv f (+ (* 2 j) 1)))   ;; sdev1
;                   (set! m2 (track.get cpv f (+ 26 (* 2 j))))  ;; mean2 (delta)
;                   (set! s2 (track.get cpv f (+ 26 (* 2 j) 1)));; sdev2 (delta)
;                   (set! p1 (track.get param_track (- i 1) j)) ;; p.value

;                   (if (equal? s2 0)
;                       (set! p m1)
;                       (set! p (/ (+ m1 (+ m2 p1)) 2.0))
; ;                      (set! p (/ (+ (/ m1 s1) (/ (+ m2 p1) s2))
; ;                                 (+ (/ 1.0 s1) (/ 1.0 s2))))
;                       )
;                   (track.set param_track i j p)
; ;                  (format t "m1 %f s1 %f m2 %f s2 %f p %f\n"
; ;                          m1 s1 (+ p1 m2) s2 p)
;                   )

;; Sort of historical it should be set in INST_LANG_VOX_cg.scm
;; but maybe not in old instantiations
(defvar cluster_synth_method 
  (if (boundp 'mlsa_resynthesis)
      cg_wave_synth
      cg_wave_synth_external ))

(provide 'clustergen)
