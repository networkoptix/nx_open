;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;-*-mode:scheme-*-
;;                                                                       ;;
;;                Centre for Speech Technology Research                  ;;
;;                     University of Edinburgh, UK                       ;;
;;                       Copyright (c) 1996,1997                         ;;
;;                        All Rights Reserved.                           ;;
;;                                                                       ;;
;;  Permission is hereby granted, free of charge, to use and distribute  ;;
;;  this software and its documentation without restriction, including   ;;
;;  without limitation the rights to use, copy, modify, merge, publish,  ;;
;;  distribute, sublicense, and/or sell copies of this work, and to      ;;
;;  permit persons to whom this work is furnished to do so, subject to   ;;
;;  the following conditions:                                            ;;
;;   1. The code must retain the above copyright notice, this list of    ;;
;;      conditions and the following disclaimer.                         ;;
;;   2. Any modifications must be clearly marked as such.                ;;
;;   3. Original authors' names are not deleted.                         ;;
;;   4. The authors' names are not used to endorse or promote products   ;;
;;      derived from this software without specific prior written        ;;
;;      permission.                                                      ;;
;;                                                                       ;;
;;  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        ;;
;;  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      ;;
;;  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   ;;
;;  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     ;;
;;  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    ;;
;;  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   ;;
;;  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          ;;
;;  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       ;;
;;  THIS SOFTWARE.                                                       ;;
;;                                                                       ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;           Author:  Alan W Black
;;;           Date:    December 1997
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;;  Find the means and standard deviations of the durations of all
;;;  phones in the given utterances.
;;;

;;; Because this is a --script type file it has to explicitly
;;; load the initfiles: init.scm and user's .festivalrc
(load (path-append libdir "init.scm"))

(define (durmeanstd_help)
  (format t "%s\n"
  "durmeanstd [options] festival/utts/*.utts
  Find means and standard deviation of phone durations in utterances
  Options
  -output <ofile>
             File to save output in
  -relation <string>
             Relation for phones (default Segment)
  -log 
             Take log of durations first
  -from_file 
             file with list of utt names
")
  (quit))

;;; Default options values
(defvar utt_files nil)
(defvar outfile "durs.meanstd")
(defvar log_domain nil)
(defvar durrelation 'Segment)

;;; Get options
(define (get_options)
  (let ((files nil)
	(o argv))
    (if (or (member_string "-h" argv)
	    (member_string "-help" argv)
	    (member_string "--help" argv)
	    (member_string "-?" argv))
	(durmeanstd_help))
    (while o
      (begin
	(cond
	 ((string-equal "-output" (car o))
	  (if (not (cdr o))
	      (durmeanstd_error "no output file specified"))
	  (set! outfile (car (cdr o)))
	  (set! o (cdr o)))
	 ((string-equal "-relation" (car o))
	  (if (not (cdr o))
	      (durmeanstd_error "no relation file specified"))
	  (set! durrelation (car (cdr o)))
	  (set! o (cdr o)))
	 ((string-equal "-from_file" (car o))
	  (if (not (cdr o))
	      (durmeanstd_error "no file of utts names file specified"))
          (set! files 
                (append
                 (load (car (cdr o)) t) files))
	  (set! o (cdr o)))
	 ((string-equal "-log" (car o))
	  (set! log_domain t))
	 (t
	  (set! files (cons (car o) files))))
	(set! o (cdr o))))
    (if files
	(set! utt_files (reverse files)))))

(define (durmeanstd_error message)
  (format stderr "%s: %s\n" "durmeanstd" message)
  (durmeanstd_help))

;;; No gc messages
(gc-status nil)

;;;  A simple sufficient statistics class
(define (suffstats.new)
  (list
   0    ;; n
   0    ;; sum
   0    ;; sumx
   ))

(define (suffstats.set_n x n)
  (set-car! x n))
(define (suffstats.set_sum x sum)
  (set-car! (cdr x) sum))
(define (suffstats.set_sumx x sumx)
  (set-car! (cdr (cdr x)) sumx))
(define (suffstats.n x)
  (car x))
(define (suffstats.sum x)
  (car (cdr x)))
(define (suffstats.sumx x)
  (car (cdr (cdr x))))
(define (suffstats.reset x)
  (suffstats.set_n x 0)
  (suffstats.set_sum x 0)
  (suffstats.set_sumx x 0))
(define (suffstats.add x d)
  (suffstats.set_n x (+ (suffstats.n x) 1))
  (suffstats.set_sum x (+ (suffstats.sum x) d))
  (suffstats.set_sumx x (+ (suffstats.sumx x) (* d d)))
)

(define (suffstats.mean x)
  (/ (suffstats.sum x) (suffstats.n x)))
(define (suffstats.variance x)
  (cond
   ((or (< (suffstats.n x) 2 )
        (equal? (* (suffstats.n x) (suffstats.sumx x))
                (* (suffstats.sum x) (suffstats.sum x))))
    ;; avoid 0 variance 
    (/ (suffstats.mean x) 10.0))
   (t
      (/ (- (* (suffstats.n x) (suffstats.sumx x))
            (* (suffstats.sum x) (suffstats.sum x)))
         (* (suffstats.n x) (- (suffstats.n x) 1))))))
(define (suffstats.stddev x)
  (sqrt (suffstats.variance x)))

;;; Index for each phone
(defvar phonelist nil) ;; index of phone to suffstats
(define (get_phone_data phone)
  (let ((a (car (cdr (assoc phone phonelist)))))
    (if a
	a
	(begin ;; first time for this phone
	  (set! phonelist
		(cons
		 (list phone (suffstats.new))
		 phonelist))
	  (car (cdr (assoc phone phonelist)))))))

(define (duration i)
  (if (item.prev i)
      (- (item.feat i "end") (item.feat i "p.end"))
      (item.feat i "end")))

(define (cummulate_seg_durs utt_name)
  (let ((utt (utt.load nil utt_name)))
    (mapcar
     (lambda (s)
       (suffstats.add 
	(get_phone_data (item.name s))
	(if log_domain
	    (log (duration s))
            (duration s))))
     (utt.relation.items utt durrelation))))

(define (output_dur_data data outfile)
  (let ((fd (fopen outfile "w")))
    (mapcar
     (lambda (d)
       (format fd "(%s %f %f)\n"
	       (car d)
	       (suffstats.mean (car (cdr d)))
	       (suffstats.stddev (car (cdr d)))))
     data)
    (fclose fd)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;   The main work
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(define (main)
  (get_options)

  (mapcar 
   (lambda (u)
     (unwind-protect
      (cummulate_seg_durs u)
      nil))
   utt_files)
  (output_dur_data phonelist outfile)
)

(main)
