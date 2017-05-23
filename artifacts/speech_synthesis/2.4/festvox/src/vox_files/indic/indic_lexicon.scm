;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;                                                                     ;;;
;;;                     Carnegie Mellon University                      ;;;
;;;                  and Alan W Black and Kevin Lenzo                   ;;;
;;;                      Copyright (c) 1998-2000                        ;;;
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
;;;        Alok Parlikar (aup@cs.cmu.edu)                               ;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;; Lexicon, LTS and Postlexical rules for cmu_indic
;;;

;; Supported languages: Hindi, Bengali, Kannada, Tamil.
;; Voice should set this variable
(defvar lex:language 'Hindi)

(define (delete_final_schwa)
  "(delete_final_schwa)
Returns t if final schwa is deleted in the current language"
  (member lex:language '(Hindi Gujarati Rajasthani Bengali Assamese)))

(define (delete_medial_schwa)
  "(delete_medial_schwa)
Returns t if medial schwa in words is deleted in the current language"
  (member lex:language '(Hindi Gujarati Rajasthani)))

(require 'lts)

;; Load Mapping of unicode characters into decimal integers
(set! indic_char_ord_map
	  (load (path-append 
                 INST_LANG_VOX::dir
                 "festvox/indic_utf8_ord_map.scm") t))


;; Load Mapping of unicode ordinal to SAMPA phones
(set! indic_ord_phone_map
	  (load 
           (path-append
            INST_LANG_VOX::dir
           "festvox/unicode_sampa_map_new.scm") t))


;; Set classes of indic characters
(set! indic_char_types '(independent_vowel
						 consonant vowel
						 digits punc ignore
						 anuswaar visarga nukta
						 avagraha halant
						 ))

(set! indic_char_type_ranges
	  '(
		(independent_vowel (
							;; Devnagari
							(2308 2324) ;; ऄ to औ
							(2384 2384) ;; Aum
							(2400 2401) ;; ॠ and ॡ
							(2418 2423) ;; ॲ to ॶ

							;; Bengali
							(2437 2444)
							(2447 2448)
							(2451 2452)
							(2528 2529)

							;; Tamil
							(2949 2954)
							(2958 2960)
							(2962 2964)

							;; Kannada
							(3205 3212)
							(3214 3216)
							(3218 3220)
							(3296 3297)

							;; Telugu
							(3077 3084)
							(3086 3088)
							(3090 3092)
							(3168 3169) ;; additional vowels for sanskrit

							;; Gujarati
                                                        (2693 2701)
                                                        (2703 2705)
                                                        (2707 2708)

							 ))
		(consonant         (
							;; Devnagari
							(2325 2361) ;; क to ह
							(2392 2399) ;; क़ to य़
							(2425 2431) ;; ॹ to ॿ

							;; Bengali
							(2453 2472)
							(2474 2480)
							(2482 2482)
							(2486 2489)
							(2510 2510)
							(2524 2525)
							(2527 2527)
							(2544 2545)

							;; Tamil
							(2965 2965)
							(2969 2970)
							(2972 2972)
							(2974 2975)
							(2979 2980)
							(2984 2986)
							(2990 3001)

							;; Kannada
							(3221 3240)
							(3242 3251)
							(3253 3257)
							(3294 3294)

							;; Telugu
							(3093 3112)
							(3114 3123)
							(3125 3129)
							(3157 3158) ;; historic phonetic variants

							 ;; Gujarati
                                                        (2709 2728)
                                                        (2730 2736)
                                                        (2738 2739)
                                                        (2741 2745)

							))

		(vowel             (
							;; Devnagari
							(2362 2363) ;;
							(2366 2380) ;; ा to ौ
							(2383 2383) ;;
							(2389 2391) ;;
							(2402 2403) ;;

							;; Bengali
							(2494 2500)
							(2503 2504)
							(2507 2508)
							(2519 2519)
							(2530 2531)

							;; Tamil
							(3006 3010)
							(3014 3016)
							(3016 3020)

							;; Kannada
							(3262 3268)
							(3270 3272)
							(3274 3276)
							(3298 3299)

							;; Telugu
							(3134 3140)
							(3142 3144)
							(3146 3148)
							(3170 3171) ;; dependent vowels

                                                        ;; Gujarati
                                                        (2750 2757)
                                                        (2759 2761)
                                                        (2763 2764)

							))

		(anuswaar          (
							;; Devnagari
							(2305 2306)

							;; Bengali
							(2433 2434)

							;; Tamil
							(2946 2946)

							;; Kannada
							(3202 3202)

							;; Telugu
							(3073 3074)

                                                        ;; Gujarati
                                                        (2689 2690)

							))

		(visarga           (
							;; Devnagari
							(2307 2307)

							;; Bengali
							(2435 2435)

							;; Tamil
							(2947 2947)

							;; Kannada
							(3203 3203)
	
							;; Telugu
							(3075 3075)

                                                        ;; Gujarati
                                                        (2691 2691)

							))

		(nukta             (
							;; Devnagari
							(2364 2364)

							;; Bengali
							(2492 2492)

							;; Tamil: It doesn't have a nukta, but it
							;; has vowel lengthener that behaves like
							;; a nukta (modifies the previous vowel to
							;; map to something else
							(3031 3031)

							;; Kannada
							(3260 3260)

							;; Telugu does not seem to have nukta

                                                        ;; Gujarati
                                                        (2748 2748)

							))

		(avagraha          (
							;; Devnagari
							(2365 2365)

							;; Bengali
							(2493 2493)

							;; Kannada
							(3261 3261)

							;; Telugu
							(3133 3133)
	
                                                        ;; Gujarati
                                                        (2749 2749)

							))

		(halant            (                 ; Also known as Virama
							;; Devnagari
							(2381 2381)

							;; Bengali
							(2509 2509)

							;; Tamil
							(3021 3021)

							;; Kannada
							(3277 3277)

							;; Telugu
							(3149 3149)

                                                        ;; Gujarati
                                                        (2765 2765)
							))

		(digits            (
							;; Devnagari
							(2406 2415) ; Digits

							;; Bengali
							(2534 2543)

							;; Tamil
							(3046 3058) ; Digits

							;; Kannada
							(3302 3311)

							;; Telugu
							(3174 3183)

                                                        ;; Gujarati
                                                        (2790 2799)

							))

		(punc              (
							;; Devnagari
							(2404 2405)
							(2416 2417)
							))

		(ignore            (
							;; Devnaragi
							(2382 2382)  ;; Prishthamatra E
							(2385 2388)  ;; Devnag Accents
							(2424 2424)  ;; Reserved

							;; Bengali
							(2445 2446)
							(2449 2450)
							(2473 2473)
							(2481 2481)
							(2483 2485)
							(2501 2502)
							(2526 2526)
							(2532 2533)
							(2546 2559)

							;; Tamil
							(2955 2957)
							(2691 2691)
							(2966 2968)
							(2971 2971)
							(2973 2973)
							(2976 2978)
							(2981 2983)
							(2987 2989)
							(3002 3005)
							(3011 3013)
							(3017 3017)
							(3022 3023)
							(3025 3030)
							(3032 3045)
							(3059 3066) ; Misc Symbols shouldn't ideally be ignored
							(3067 3071)

							;; Kannada
							(3213 3213)
							(3217 3217)
							(3241 3241)
							(3252 3252)
							(3269 3269)
							(3273 3273)
							(3285 3286)  ;; Vowel lengtheners. Ideally shouldn't be ignored. But not handled so far
							(3300 3301)
							(3326 3327)

							;; Telugu
							(3157 3158) ;; vowel length marks, ideally should not be ignored
							(3192 3199) ;; fractions and weights

			                                 ;; Gujarati
                                                        (2768 2768) ;; om symbol
                                                        (2784 2787) ;; sanskrit vowels, map?
                                                        (2800 2800) ;; abbreviation symbol
                                                        (2801 2801) ;; rupee symbol

							))
		))

;; Mapping of characters in the context of a nukta.
;; ( from  to )
(set! nukta_map
	  '(
		;; Devnagari
		( 2325 2392 ) ; क़
		( 2326 2393 ) ; ख़
		( 2327 2394 ) ; ग़
		( 2332 2395 ) ; ज़
		( 2337 2396 ) ; ड़
		( 2338 2397 ) ; ढ़
		( 2347 2398 ) ; फ़
		( 2351 2399 ) ; य़

		;; Bengali
		( 2465 2524 )
		( 2466 2525 )
		( 2479 2527 )

		;; Tamil aU vowel in Tamil gets lengthened. See Tamil nukta
		;; above for details.
		( 2962 2964 )
		))


(define (indic_type ord)
  "(indic_type ord)
Given a unicode character ordinal (decimal), return the type of its indic character:
'independent_vowel, 'vowel, 'consonant, etc."

  (set! ret nil)

  (if ord
	  (mapcar
	   (lambda (type)
		 (set! type_ranges (cadr (assoc type indic_char_type_ranges)))
		 (mapcar
		  (lambda (range)
			(set! start (car range))
			(set! end (cadr range))
			(if (and (>= ord start)
					 (<= ord end))
				(set! ret type)))
		  type_ranges))
	   indic_char_types))
  (if (and ord (not ret))
	  (format t "Warning: Can not handle Unicode ORD %d\n" ord))
  ret)

(define (utf8_ord char)
  "(utf8_ord char)
Given a string representing one Unicode character, return an integer
representing the Unicode code point of that character."
  (set! ret (cadr (assoc_string char indic_char_ord_map)))
  (if (not ret)
	  (begin
		(format t "Warning: Character %s not handled correctly.\n" char)
		0)
	  ret))


(define (ord_to_phoneme ord)
  (set! ret (car (cadr (assoc ord indic_ord_phone_map))))
  (if (not ret)
	  (format t "Warning: Phoneme for unicode ordinal %d not found\n" ord))
  ret)


;; Function to remove ignore values from a unicode ord list
(define (indic_remove_ignore_ords ordlist)
  "(indic_remove_ignore_ords ordlist)
From a list containing integer representations of indic characters,
remove all characters of type 'ignore"

  (set! output (list))
  (mapcar (lambda (x)
			(if (not (eq 'ignore (indic_type x)))
				(set! output (append output (list x)))))
		  ordlist)
  output)


;; Function to map nukta characters appropriately
(define (indic_map_nukta ordlist)
  "(indic_map_nukta ordlist)
Go over all character-ordinals in the list.  If any character is a nukta,
then look at the previous character in map it into its nukta
equivalent.  If there's no mapping item, ignore the nukta."

  (set! output (list))
  (set! ordlist (reverse ordlist))

  (set! map_next_item nil)
  (mapcar (lambda (x)
			(if map_next_item
				;; we have to map x into its nukta equavalent
				(begin
				  (set! map_next_item nil)
				  (set! mapped_x (assoc x nukta_map))
				  (if (not mapped_x)
					  (format t "Warning: Nukta found for invalid character %d\n" x)
					  ;; else
					  (begin
						(set! output (append (list (cadr mapped_x)) output)))))
				;; else
				(begin
				  (if (eq 'nukta (indic_type x))
					  (set! map_next_item t)
					  ;; else
					  (begin
						(set! map_next_item nil)
						(set! output (append (list x) output)))))))
		  ordlist)
  output)


;; Rules for Letter to Sound
(define (indic_lts word)
  "(indic_lts word)
Letter to sound rules for Indic words"

  (set! out_phones (list))
  (set! in_phones (mapcar utf8_ord (utf8explode word)))

  ;; Remove all characters we should ignore
  (set! in_phones (indic_remove_ignore_ords in_phones))

  ;; Map nukta characters
  (set! in_phones (indic_map_nukta in_phones))

  (set! prev_char nil)
  (set! cur_char (car in_phones))
  (set! next_char (cadr in_phones))
  (set! remainder (cddr in_phones))

  (while cur_char ;; Loop over all characters
;		 (format t "Here: %l %l %l\n" cur_char next_char remainder)
		 (set! cur_char_type (indic_type cur_char))
		 (set! next_char_type (indic_type next_char))
		 (cond
		  ((eq cur_char_type 'consonant)
		   ;; Add the consonant to output list.
		   (set! out_phones
				 (append out_phones (ord_to_phoneme cur_char)))

		   ;; If a consonant is followed by a combination vowel, a
		   ;; halant, a punctuation then don't add a schwa after
		   ;; it. Otherwise, insert a schwa. For end-of-word, check
		   ;; whether we should insert schwa in this language.

		   (if (not next_char)  ;; We are at last char. Add schwa?
			   (if (or (not prev_char)  ;; Don't delete schwa in 1-char word
					   (not (delete_final_schwa)))
				   ;; then add schwa
				   (set! out_phones
						 (append out_phones (list 'A)))
				   ;; else
				   ;; 
				   ;; Schwa deletion should probably happen depending
				   ;; on whether there is a consonant cluster or not,
				   ;; at the end. But Adding that rule here seems to
				   ;; not have worked properly. Hence, we always
				   ;; delete the final schwa.
				   )
			   ;; else
			   (if (not (or (eq next_char_type 'vowel)
							(eq next_char_type 'halant)
							(eq next_char_type 'punc)
							(eq next_char_type 'ignore)))
				   (set! out_phones
						 (append out_phones (list 'A) )))))

		  ((or (eq cur_char_type 'independent_vowel)
			   (eq cur_char_type 'vowel)
			   (eq cur_char_type 'digit))
		   ;; We just append whatever the current char resolves to.
		   (set! out_phones (append out_phones (ord_to_phoneme cur_char))))

		  ((eq cur_char_type 'halant)
		   ;; We take care of halant inherently inside consonant case. So just ignore it here.
		   )

		  ((eq cur_char_type 'avagraha)
		   ;; We should add another item of the previous vowel, but not full vowel!
		   (if (eq (indic_type prev_char) 'vowel)
			   (set! out_phones (append out_phones (ord_to_phoneme prev_char)))
			   (format t "Warning: Invalid Avagraha after unicode char %d\n" prev_char)))
		  ((eq cur_char_type 'anuswaar)
		   ;; The realization of anuswaar is context dependent: We
		   ;; only generate a placeholder symbol and let postlexical
		   ;; rules take care of this.
		   (set! out_phones (append out_phones (list 'nX))))

		  ((eq cur_char_type 'visarga)
		   ;; Visarga is a voiceless glottal fricative. Just add the
		   ;; phone.
		   (set! out_phones (append out_phones (list 'h))))

		  (t
		   (format t "WARNING: Unhandled character %d of type %s\n" cur_char cur_char_type )
		   )
		  )

		 (set! prev_char cur_char)
		 (set! cur_char next_char)
		 (set! next_char (car remainder))
		 (set! remainder (cdr remainder))
;		 (format t "Here2: %l %l %l\n" cur_char next_char remainder))
		 )

  out_phones)


(define (indic_is_vowel phone)
  "(indic_is_vowel phone)
Returns t if the phone is a vowel"
  (string-equal "+" (phone_feature phone "vc")))


(define (indic_contains_vowel l)
  "(indic_contains_vowel l)
Returns t if the list l contains any phone that is a vowel."
  (member_string
   t
   (mapcar (lambda (x) (indic_is_vowel x)) l)))


;; Post-lexical rules to handle contextual nasalization
(define (indic_nasal_postfixes phones)
  "(indic_nasal_postfixes phones)
Given a phone sequence containing a special character nX (contextual
nasal), replace it with the appropriate nasal phone based on its
context"

  (cond
   ((null phones) nil)
   ((not (car phones)) nil)
   ((and (cdr phones)  ;; nasalize vowel followed by nX at end of word
         (indic_is_vowel (car phones))
		 (eq 'nX (cadr phones))
		 (null (caddr phones)))
	(if (eq 'A (car phones)) ;; If it's a schwa, it's not nasalized. nX becomes m
		(cons (car phones) (cons 'm (indic_nasal_postfixes (cddr phones))))
		(cons (symbolconc (car phones) 'nas ) (indic_nasal_postfixes (cddr phones)))))
   ((and (cdr phones)  ;; nX followed by velar becomes nG
         (eq 'nX (car phones))
		 (string-equal (phone_feature (cadr phones) 'cplace) "v"))
    (cons 'N (cons (cadr phones) (indic_nasal_postfixes (cddr phones)))))
   ((and (cdr phones)  ;; nX followed by palatal becomes n~
         (eq 'nX (car phones))
		 (string-equal (phone_feature (cadr phones) 'cplace) "p"))
    (cons 'n~ (cons (cadr phones) (indic_nasal_postfixes (cddr phones)))))
   ((and (cdr phones)  ;; nX followed by alveolar becomes nr
         (eq 'nX (car phones))
		 (string-equal (phone_feature (cadr phones) 'cplace) "a"))
    (cons 'nr (cons (cadr phones) (indic_nasal_postfixes (cddr phones)))))
   ((and (cdr phones)  ;; nX followed by dental becomes nB
         (eq 'nX (car phones))
		 (string-equal (phone_feature (cadr phones) 'cplace) "d"))
    (cons 'nB (cons (cadr phones) (indic_nasal_postfixes (cddr phones)))))
   ((and (cdr phones)  ;; nX followed by labial becomes m
         (eq 'nX (car phones))
		 (string-equal (phone_feature (cadr phones) 'cplace) "l"))
    (cons 'm (cons (cadr phones) (indic_nasal_postfixes (cddr phones)))))
   ((and (cdr phones)  ;; all other nX become nB
         (eq 'nX (car phones)))
	(set! replacement 'nB)
	(if (eq lex:language 'Kannada)  ;; Default anuswar in kannada is "m"
		(set! replacement 'm))
    (cons replacement
		  (cons (cadr phones) (indic_nasal_postfixes (cddr phones)))))
   (t
    (cons (car phones) (indic_nasal_postfixes (cdr phones))))))


;; Post-lexical rules to delete medial Schwa
(define (indic_delete_medial_schwa reverse_phones)
  "(indic_delete_medial_schwa reverse_phones)
Delete Medial Schwa from a list of phones, received in reverse order"

  ;; This schwa deletion follows the technique by Narsimhan et al (2001).
  ;; 1. Process input from right to left
  ;; 2. If a schwa is found in a VC_CV context, then delete it.
  ;;
  ;; There are exceptions to this: (i) Phonotactic constraints of
  ;; Hindi not violated, and no (ii) morpheme boundary present on the
  ;; left. But I don't know how to handle these yet. So this will be
  ;; wrong more often than the 11% reported in that paper. -- AUP

  (cond
   ((null reverse_phones) nil)
   ((not (car reverse_phones)) nil)

   ((eq 'A (nth 2 reverse_phones))
	(if (and (indic_is_vowel (nth 0 reverse_phones))
			 (not (indic_is_vowel (nth 1 reverse_phones)))
			 (not (indic_is_vowel (nth 3 reverse_phones)))
			 (indic_is_vowel (nth 4 reverse_phones)))
		(cons (car reverse_phones)
			  (cons (cadr reverse_phones)
					(indic_delete_medial_schwa (cdddr reverse_phones))))
		(cons (car reverse_phones)
			  (indic_delete_medial_schwa (cdr reverse_phones)))))
   (t
	(cons (car reverse_phones) (indic_delete_medial_schwa (cdr reverse_phones))))))

;; Post-lexical rules in Hindi to handle special schwa
(define (hindi_schwa_postfixes phones)
  "(hindi_schwa_postfixes phones)
Change first schwa in words like rahana/rahna => rehana/rehna."
  (if (and (eq (nth 1 phones) 'A)
		   (eq (nth 2 phones) 'hv)
		   (not (indic_is_vowel (car phones)))
		   (or (eq (nth 3 phones) 'A)
			   (not (indic_is_vowel (nth 3 phones)))))
	  (cons (car phones)
			(cons 'E (cddr phones)))
	  phones))

;; Post-lexical rules to handle contextual nasalization
(define (tamil_voicing_postfixes phones)
  "(tamil_voicing_postfixes phones)
Given a phone sequence, apply tamil voicing rules"

  (set! unvoiced_voiced_map
		'(
		  ( k  g  )
		  ( c  s  )
		  ( tr dr )
		  ( tB dB )
		  ( p  b  )
		  ))

  ;; Tamil Pronunciation Rules are defined in Tolkappiyam, and the
  ;; implementation here is based on the following summary

  ;; (1) Stops are voiceless when they occur initially or are germinated (p-p)
  ;; (2) Stops are voiced when they occur intervocalically or after a nasal or voiced consonant
  ;; (3) Word-initial c becomes an s

  ;; LTS for tamil only yield unvoiced stops. So we only need to apply
  ;; rule (2) postlexically
  (cond
   ((null phones) nil)
   ((not (car phones)) nil)
   ((not (cdr phones)) phones)

   ((assoc (cadr phones) unvoiced_voiced_map)
	;; Next phone is a stop that could be mapped.
	(cond
	 ;; Exceptions first:
	 ;; word-initial c becomes s
	 ((eq 'c (car phones))
	  ;; While this is a recursive function, and (car doesn't mean
	  ;; this is word-initial, this function basically processes stops
	  ;; in the cadr, hence if car is c, it's assumed to be
	  ;; word-initial

	  (if (eq 'c (cadr phones))
		  ;; c-c should be left as c-c
		  (cons 'c (cons 'c (tamil_voicing_postfixes (cddr phones))))
		  (cons 's (tamil_voicing_postfixes (cdr phones)))))

	 ;; if next next phone is also the same one (germination, don't change anything.)
	 ((eq (cadr phones) (caddr phones))
	  (cons (car phones) (cons (cadr phones)
							   (cons (caddr phones)
									 (tamil_voicing_postfixes (cdddr phones))))))

	 ;; If current phone is a nasal/voiced stop, add voicing. (But nasals are voiced!)
	 ((and (not (indic_is_vowel (car phones)))
		   (not (member (car phones) '(rr rrh))))
	  (if (and (eq 'c (cadr phones))  ;; n~ c --> n~ J
			   (eq 'n~ (car phones)))
		  (cons (car phones)
				(cons 'J (tamil_voicing_postfixes (cddr phones))))
		  (if (string-equal (phone_feature (car phones) 'cvox) "+")
			  (cons (car phones)
					(cons
					 (cadr (assoc (cadr phones) unvoiced_voiced_map))
					 (tamil_voicing_postfixes (cddr phones))))
			  ;; If current phone is unvoiced stop, leave next unvoiced
			  (cons (car phones)
					(cons (cadr phones)
						  (tamil_voicing_postfixes (cddr phones)))))))
	 ((and (not (indic_is_vowel (car phones)))
		   (member (car phones) '(rr rrh)))
	  ;; Leave voicing intact
	  (cons (car phones)
			(cons (cadr phones)
				  (tamil_voicing_postfixes (cddr phones)))))

	 ;; Stops with schwa on both sides are left unvoiced
	 ((and (eq 'A (car phones))
		   (eq 'A (caddr phones)))
	  (cons (car phones)
			(cons (cadr phones)
				  (cons (caddr phones)
						(tamil_voicing_postfixes (cdddr phones))))))

	 ;; If current phone is a vowel and next.next is also a vowel
	 ;; then add voicing
	 ((indic_is_vowel (car phones))
	  (if (indic_is_vowel (caddr phones))
		  (cons (car phones)
				(cons
				 (cadr (assoc (cadr phones) unvoiced_voiced_map))
				 (tamil_voicing_postfixes (cddr phones))))
		  ;; Otherwise leave unvoiced
		  (cons (car phones)
				(cons (cadr phones)
					  (tamil_voicing_postfixes (cddr phones))))))

	 ;; If current is vowel, but this is last syllable, then leave voicing as it is.
	 ((and (indic_is_vowel (car phones))
		   (not (cddr phones)))
	  phones)

	 (t
	  (format t "Warning. Voicing undetermined for %l in context (%l, %l)"
			  (cadr phones) (car phones) (caddr phones)))))
   (t
	(cons (car phones) (tamil_voicing_postfixes (cdr phones))))))


(define (indic_lex_sylbreak currentsyl remainder)
  "(indic_lex_sylbreak currentsyl remainder)
t if this is a syl break, nil otherwise."

  (cond

   ((not (indic_contains_vowel remainder))
    nil)

   ((not (indic_contains_vowel currentsyl))
    nil)

   ((and (string-equal "n" (phone_feature (car remainder) "ctype"))
		 (not (indic_is_vowel (cadr remainder))))
	nil)

   ((and (indic_is_vowel (car currentsyl))
		 (not (indic_is_vowel (car remainder)))
		 (not (indic_is_vowel (cadr remainder))))
	nil)

   ((and (not (indic_is_vowel (car remainder)))
		 (not (indic_is_vowel (cadr remainder)))
		 (not (indic_is_vowel (caddr remainder))))
	;; We are expecting three consonants in a row. Don't break
	nil)

   ((string-equal (car remainder) (cadr remainder)) ;; like the double t in uttar
	nil)
   (t
	;; Break otherwise.
    t))
)

(define (indic_lex_syllabify_phstress phones)
  (let ((syl nil) (syls nil) (p phones) (stress 0) (sylweights nil))
    (while p
     (set! syl nil)
     (set! stress 0)
     (while (and p (not (indic_lex_sylbreak syl p)))
			(set! syl (cons (car p) syl))
			(set! p (cdr p)))
     (set! syls (cons (list (reverse syl) stress) syls)))
	(set! syls (reverse syls))

	(set! sylweights
		  (mapcar
		   (lambda (s)
			 (set! tmp (reverse (car s)))
			 (set! weight 0)
			 (set! x1 (car tmp)) ; last phone
			 (set! x2 (cadr tmp)) ; second-last phone
			 (set! x3 (caddr tmp)) ; third-last phone
			 (if (indic_is_vowel x1)
				 (if (member x1 '(A i u))
					 (set! weight 1)
					 (set! weight 2))
				 ;;else
				 (if (indic_is_vowel x2)
					 (if (member x2 '(A i u))
						 (set! weight 2)
						 (set! weight 3))
					 (if (indic_is_vowel x3)
						 (set! weight 3))))
			 weight)
		   syls))

	;; (format t "%l\n" sylweights)

	;; The stress is placed on the syllable with the highest weight.
	;; If there is a tie, the last-most syllable with highest weight
	;; is chosen.  However, the last syllable of the word does not
	;; participate in tie-breaking. That is, it is stressed only when
	;; there are no ties. (Hussein 1997)

	(set! best_weight 0)
	(set! stress_position 0)
	(let ((p sylweights) (pos 0))
	  (while p
			 (set! pos (+ pos 1))
			 (cond
			  ((> (car p) best_weight)
			   (set! best_weight (car p))
			   (set! stress_position pos))
			  ((and (eq (car p) best_weight)
					(cdr p))
			   (set! stress_position pos)))
			 (set! p (cdr p))))

	(set! syls
		  (mapcar
		   (lambda (s)
			 (set! stress_position (- stress_position 1))
			 (if (eq 0 stress_position)
				   (list (car s) 1)
				   (list (car s) 0)))
		   syls))
	syls))

;;; CMU	SAMPA	Comments
;;; devnagari
(set! indic_eng_devn_phone_map
'(
(aa 	A:)
(ae	e)	;;No equivalent, so using e. Can use ay too but not always.
(ah	A)	;; Could map this to A hv too but not sure if it'll sound weird
(ao	o)	;;ow is the correct mapping but is going to be very infrequent so use o instead
(aw 	aU)
(ax 	A)
(axr	A)	;;No equivalent
(ay	aI)
(b	b)
(ch	c)
(d	dr)
(dh	dB)
(eh     e)
(er 	E 9r)	;; not sure if this is correct but usage in CMUdict seems to be close to schwa-R
(ey 	ay)
(f	ph) 	;;f is the correct mapping but is going to be very infrequent so use ph instead
(g	g)
(hh	hv)
(ih	i)
(iy	i:)
(jh	J) 
(k	k)
(l	l)
(m	m)
(n	nB)
(nx	nB)
(ng	nB)	;; no direct equivalent so mapping to n g	
(ow	o)
(oy	o j) 	;;ow j is the right mapping but will be infrequent so map to o j instead
(p	p)
(r	9r)
(s	s)
(sh	c}) 
(t	tr) 
(th	tBh)
(uh	u)
(uw	u:)
(v	v)
(w	v) 
(y	j)
(z	s)	;; z is the correct mapping but will be infrequent so mapping to Jh instead
(zh	c})	;; in CMUdict usage this seems to be closer to c} than z so mapping to c}
))

(set! indic_eng_tamil_phone_map
'(
(aa 	A:)
(ae	e:)	;;No equivalent, so using e. Can use ay too but not always.
(ah	A)	;; Could map this to A hv too but not sure if it'll sound weird
(ao	o)	;;ow is the correct mapping but is going to be very infrequent so use o instead
(aw 	aU)
(ax 	A)
(axr	A)	;;No equivalent
(ay	aI)
(b	b)
(ch	c)
(d	dr)
(dh	dB)
(eh     e)
(er 	A 9r)	;; not sure if this is correct but usage in CMUdict seems to be close to schwa-R
(ey 	e:)
(f	p) 	;;f is the correct mapping but is going to be very infrequent so use ph instead
(g	k)
(hh	hv)
(ih	i)
(iy	i:)
(jh	J) 
(k	k)
(l	l)
(m	m)
(n	nB)
(nx	nB)
(ng	N)	;; no direct equivalent so mapping to n g	
(ow	o)
(oy	o j) 	;;ow j is the right mapping but will be infrequent so map to o j instead
(p	p)
(r	9r)
(s	s)
(sh	c}) 
(t	tr) 
(th	tB)
(uh	u)
(uw	u:)
(v	v)
(w	v) 
(y	j)
(z	s)	;; z is the correct mapping but will be infrequent so mapping to Jh instead
(zh	c})	;; in CMUdict usage this seems to be closer to c} than z so mapping to c}
))

(set! indic_eng_telugu_phone_map
'(
(aa 	A:)
(ae	e)	;;No equivalent, so using e. Can use ay too but not always.
(ah	A)	;; Could map this to A hv too but not sure if it'll sound weird
(ao	o)	;;ow is the correct mapping but is going to be very infrequent so use o instead
(aw 	aU)
(ax 	A)
(axr	A)	;;No equivalent
(ay	aI)
(b	b)
(ch	c)
(d	dr)
(dh	dB)
(eh     e)
(er 	A 9r)	;; not sure if this is correct but usage in CMUdict seems to be close to schwa-R
(ey 	e:)
(f	ph) 	;;f is the correct mapping but is going to be very infrequent so use ph instead
(g	g)
(hh	hv)
(ih	i)
(iy	i:)
(jh	J) 
(k	k)
(l	l)
(m	m)
(n	nB)
(nx	nB)
(ng	nB)	;; no direct equivalent so mapping to n g	
(ow	o)
(oy	o j) 	;;ow j is the right mapping but will be infrequent so map to o j instead
(p	p)
(r	9r)
(s	s)
(sh	c}) 
(t	tr) 
(th	tBh)
(uh	u)
(uw	u:)
(v	v)
(w	v) 
(y	j)
(z	s)	;; z is the correct mapping but will be infrequent so mapping to Jh instead
(zh	c})	;; in CMUdict usage this seems to be closer to c} than z so mapping to c}
))


(define (indic_ml_map_eng_phone p)
  (let ((m (assoc_string 
            p 
            (cond
             ((string-equal lex:language 'Tamil)
              indic_eng_tamil_phone_map)
             ((string-equal lex:language 'Telugu)
              indic_eng_telugu_phone_map)
             (t
              ;; Note its really phone base not script based
              indic_eng_devn_phone_map)))))
    (cond
     (m (cdr m))
     (t ;; its not there ??
      (list p))))
)

(define (indic_ml_lts_function word features)
  ;; Deals with romanized words (treats them as English)
  (cond
   ((string-matches word "^[0-9a-zA-Z/:_-]+$")
    (let ((eng_entry) (nnn_entry))
      (lex.select "cmu") ;; English lexicon
      (set! eng_entry (lex.lookup word features))
      (lex.select "cmu_indic")    
      (set! nnn_entry (list
       (car eng_entry)
       (cadr eng_entry)
       (mapcar
        (lambda (syl)
          (list 
           (apply append  ;; might be list of phones in the mapping
           (mapcar
            (lambda (seg) (indic_ml_map_eng_phone seg))
            (car syl)))
           (cadr syl) ;; stress
           ))
        (car (cddr eng_entry)))))
;      (format t "%l\n" nnn_entry)
      nnn_entry))
   (t 
    (indic_lts_function word features))))

;; Put it all together and define the functions to use for LTS
(define (indic_lts_function word features)
  "(indic_lts_function WORD FEATURES)
Return pronunciation of word not in lexicon."
  (let ((dword word) (syls) (phones))
	(set! phones (indic_lts dword))
	(set! phones (indic_nasal_postfixes phones))

	(if (delete_medial_schwa)
		(set! phones (reverse (indic_delete_medial_schwa (reverse phones)))))

	(if (eq lex:language 'Hindi)
		(set! phones (hindi_schwa_postfixes phones)))

	(if (eq lex:language 'Tamil)
		(set! phones (tamil_voicing_postfixes phones)))

	(set! syls (indic_lex_syllabify_phstress phones))
	(list word features syls)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;; Lexicon definition
;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(lex.create "cmu_indic")
(lex.set.phoneset "cmu_indic")
;; We deal with English words too, and use the English lexicon to
;; get a pronunciation then map these to the indic phones as necessary
(lex.set.lts.method 'indic_ml_lts_function)

(define (cmu_indic::select_lexicon)
  "(cmu_indic::select_lexicon)
Set up the lexicon for cmu_indic."
  (lex.select "cmu_indic"))

(define (cmu_indic::reset_lexicon)
  "(cmu_indic::reset_lexicon)
Reset lexicon information."
  t)

(provide 'indic_lexicon)
