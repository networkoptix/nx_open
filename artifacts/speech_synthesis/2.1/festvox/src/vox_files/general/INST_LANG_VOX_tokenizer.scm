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
;;;
;;; Tokenizer for LANG
;;;
;;;  To share this among voices you need to promote this file to
;;;  to say festival/lib/INST_LANG/ so others can use it.
;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;; Load any other required files

;; Punctuation for the particular language
(set! INST_LANG_VOX::token.punctuation "\"'`.,:;!?(){}[]")
(set! INST_LANG_VOX::token.prepunctuation "\"'`({[")
(set! INST_LANG_VOX::token.whitespace " \t\n\r")
(set! INST_LANG_VOX::token.singlecharsymbols "")

;;; Voice/LANG token_to_word rules 
(define (INST_LANG_VOX::token_to_words token name)
  "(INST_LANG_VOX::token_to_words token name)
Specific token to word rules for the voice INST_LANG_VOX.  Returns a list
of words that expand given token with name."
  (cond
   ((string-matches name "[1-9][0-9]+")
    (INST_LANG::number token name))
   (t ;; when no specific rules apply do the general ones
    (list name))))

(define (INST_LANG::number token name)
  "(INST_LANG::number token name)
Return list of words that pronounce this number in LANG."

  (error "INST_LANG::number to be written\n")

)

(define (INST_LANG_VOX::select_tokenizer)
  "(INST_LANG_VOX::select_tokenizer)
Set up tokenizer for LANG."
  (Parameter.set 'Language 'INST_LANG)
  (set! token.punctuation INST_LANG_VOX::token.punctuation)
  (set! token.prepunctuation INST_LANG_VOX::token.prepunctuation)
  (set! token.whitespace INST_LANG_VOX::token.whitespace)
  (set! token.singlecharsymbols INST_LANG_VOX::token.singlecharsymbols)

  (set! token_to_words INST_LANG_VOX::token_to_words)
)

(define (INST_LANG_VOX::reset_tokenizer)
  "(INST_LANG_VOX::reset_tokenizer)
Reset any globals modified for this voice.  Called by 
(INST_LANG_VOX::voice_reset)."
  ;; None

  t
)

(provide 'INST_LANG_VOX_tokenizer)
