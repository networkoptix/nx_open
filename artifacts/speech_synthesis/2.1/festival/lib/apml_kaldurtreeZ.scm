;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;                                                                       ;;
;;;                Centre for Speech Technology Research                  ;;
;;;                     University of Edinburgh, UK                       ;;
;;;                       Copyright (c) 1996,1997                         ;;
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
;;;  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        ;;
;;;  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      ;;
;;;  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   ;;
;;;  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     ;;
;;;  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    ;;
;;;  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   ;;
;;;  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          ;;
;;;  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       ;;
;;;  THIS SOFTWARE.                                                       ;;
;;;                                                                       ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;;   A tree to predict zcore durations build from f2b
;;;   doesn't use actual phonemes so it can have better generalizations
;;;
;;;   Basically copied from ked
;;;

(set! apml_kal_durs
'(
  (uh 0.067 0.025)
  (hh 0.061 0.028)
  (ao 0.138 0.046)
  (hv 0.053 0.020)
  (v 0.051 0.019)
  (ih 0.058 0.023)
  (el 0.111 0.043)
  (ey 0.132 0.042)
  (em 0.080 0.033)
  (jh 0.094 0.024)
  (w 0.054 0.023)
  (uw 0.107 0.044)
  (ae 0.120 0.036)
  (en 0.117 0.056)
  (k 0.089 0.034)
  (y 0.048 0.025)
  (axr 0.147 0.035)
;  (l 0.056 0.026)
  (l 0.066 0.026)
  (ng 0.064 0.024)
  (zh 0.071 0.030)
  (z 0.079 0.034)
  (brth 0.246 0.046)
  (m 0.069 0.028)
  (iy 0.097 0.041)
  (n 0.059 0.025)
  (ah 0.087 0.031)
  (er 0.086 0.010)
  (b 0.069 0.024)
  (pau 0.200 0.1)
  (aw 0.166 0.053)
  (p 0.088 0.030)
  (ch 0.115 0.025)
  (ow 0.134 0.039)
  (dh 0.031 0.016)
  (nx 0.049 0.100)
  (d 0.048 0.021)
  (ax 0.046 0.024)
  (h# 0.060 0.083)
  (r 0.053 0.031)
  (eh 0.095 0.036)
  (ay 0.137 0.047)
  (oy 0.183 0.050)
  (f 0.095 0.033)
  (sh 0.108 0.031)
  (s 0.102 0.037)
  (g 0.064 0.021)
  (dx 0.031 0.016)
  (th 0.093 0.050)
  (aa 0.094 0.037)
  (t 0.070 0.020)
)
)

(set! apml_kal_duration_cart_tree
'
((name is pau)
 ((emph_sil is +)
  ((0.0 -0.5))
  ((p.R:SylStructure.parent.parent.lisp_apml_pause = 0.2)
   ((0.0 0.0))
   ((p.R:SylStructure.parent.parent.lisp_apml_pause = 0.4)
    ((0.0 2.0))
    ((p.R:SylStructure.parent.parent.lisp_apml_pause = 0.6)
     ((0.0 4.0))
     ((p.R:SylStructure.parent.parent.lisp_apml_pause = 0.8)
      ((0.0 6.0))
      ((p.R:SylStructure.parent.parent.lisp_apml_pause = 1.0)
       ((0.0 8.0))
       ((p.R:SylStructure.parent.parent.lisp_apml_pause = 1.5)
	((0.0 13.0))
	((p.R:SylStructure.parent.parent.lisp_apml_pause = 2.0)
	 ((0.0 18.0))
	 ((p.R:SylStructure.parent.parent.lisp_apml_pause = 2.5)
	  ((0.0 23.0))
	  ((p.R:SylStructure.parent.parent.lisp_apml_pause = 3.0)
	   ((0.0 28.0))
	   ((p.R:SylStructure.parent.parent.pbreak is BB)
	    ((0.0 2.0))  
	    ((0.0 0.0)))))))))))))
 ((R:SylStructure.parent.accented is 0)
  ((n.ph_ctype is 0)
   ((p.ph_vlng is 0)
    ((R:SylStructure.parent.syl_codasize < 1.5)
     ((p.ph_ctype is n)
      ((ph_ctype is f)
       ((0.559208 -0.783163))
       ((1.05215 -0.222704)))
      ((ph_ctype is s)
       ((R:SylStructure.parent.syl_break is 2)
	((0.589948 0.764459))
	((R:SylStructure.parent.asyl_in < 0.7)
	 ((1.06385 0.567944))
	 ((0.691943 0.0530272))))
       ((ph_vlng is l)
	((pp.ph_vfront is 1)
	 ((1.06991 0.766486))
	 ((R:SylStructure.parent.syl_break is 1)
	  ((0.69665 0.279248))
	  ((0.670353 0.0567774))))
	((p.ph_ctype is s)
	 ((seg_onsetcoda is coda)
	  ((0.828638 -0.038356))
	  ((ph_ctype is f)
	   ((0.7631 -0.545853))
	   ((0.49329 -0.765994))))
	 ((R:SylStructure.parent.parent.gpos is det)
	  ((R:SylStructure.parent.last_accent < 0.3)
	   ((R:SylStructure.parent.sub_phrases < 1)
	    ((0.811686 0.160195))
	    ((0.799015 0.713958)))
	   ((0.731599 -0.215472)))
	  ((ph_ctype is r)
	   ((0.673487 0.092772))
          ((R:SylStructure.parent.asyl_in < 1)
           ((0.745273 0.00132813))
           ((0.75457 -0.334898)))))))))
    ((pos_in_syl < 0.5)
     ((R:SylStructure.parent.R:Syllable.p.syl_break is 2)
      ((R:SylStructure.parent.R:Syllable.n.syl_onsetsize < 0.2)
       ((0.902446 -0.041618))
       ((R:SylStructure.parent.sub_phrases < 2.3)
        ((0.900629 0.262952))
        ((1.18474 0.594794))))
      ((seg_onset_stop is 0)
       ((R:SylStructure.parent.position_type is mid)
        ((0.512323 -0.760444))
        ((R:SylStructure.parent.syl_out < 6.8)
         ((pp.ph_vlng is a)
          ((0.640575 -0.450449))
          ((ph_ctype is f)
           ((R:SylStructure.parent.sub_phrases < 1.3)
            ((0.862876 -0.296956))
            ((R:SylStructure.parent.syl_out < 2.4)
             ((0.803215 0.0422868))
             ((0.877856 -0.154465))))
           ((R:SylStructure.parent.syl_out < 3.6)
            ((R:SylStructure.parent.syl_out < 1.2)
             ((0.567081 -0.264199))
             ((0.598043 -0.541738)))
            ((0.676843 -0.166623)))))
         ((0.691678 -0.57173))))
       ((R:SylStructure.parent.parent.gpos is cc)
        ((1.15995 0.313289))
        ((pp.ph_vfront is 1)
         ((0.555993 0.0695819))
         ((R:SylStructure.parent.asyl_in < 1.2)
          ((R:SylStructure.parent.sub_phrases < 2.7)
           ((0.721635 -0.367088))
           ((0.71919 -0.194887)))
          ((0.547052 -0.0637491)))))))
     ((ph_ctype is s)
      ((R:SylStructure.parent.syl_break is 0)
       ((R:SylStructure.parent.R:Syllable.p.syl_break is 1)
        ((0.650007 -0.333421))
        ((0.846301 -0.165383)))
       ((0.527756 -0.516332)))
      ((R:SylStructure.parent.syl_break is 0)
       ((p.ph_ctype is s)
        ((0.504414 -0.779112))
        ((0.812498 -0.337611)))
       ((pos_in_syl < 1.4)
        ((0.513041 -0.745807))
        ((p.ph_ctype is s)
         ((0.350582 -1.04907))
         ((0.362 -0.914974))))))))
   ((R:SylStructure.parent.syl_break is 0)
    ((ph_ctype is n)
     ((R:SylStructure.parent.position_type is initial)
      ((pos_in_syl < 1.2)
       ((0.580485 0.172658))
       ((0.630973 -0.101423)))
      ((0.577937 -0.360092)))
     ((R:SylStructure.parent.syl_out < 2.9)
      ((R:SylStructure.parent.syl_out < 1.1)
       ((R:SylStructure.parent.position_type is initial)
        ((0.896092 0.764189))
        ((R:SylStructure.parent.sub_phrases < 3.6)
         ((ph_ctype is s)
          ((0.877362 0.555132))
          ((0.604511 0.369882)))
         ((0.799982 0.666966))))
       ((seg_onsetcoda is coda)
        ((p.ph_vlng is a)
         ((R:SylStructure.parent.last_accent < 0.4)
          ((0.800736 0.240634))
          ((0.720606 0.486176)))
         ((1.18173 0.573811)))
        ((0.607147 0.194468))))
      ((ph_ctype is r)
       ((0.88377 0.499383))
       ((R:SylStructure.parent.last_accent < 0.5)
        ((R:SylStructure.parent.position_type is initial)
         ((R:SylStructure.parent.parent.word_numsyls < 2.4)
          ((0.62798 0.0737318))
          ((0.787334 0.331014)))
         ((ph_ctype is s)
          ((0.808368 0.0929299))
          ((0.527948 -0.0443271))))
        ((seg_coda_fric is 0)
         ((p.ph_vlng is a)
          ((0.679745 0.517681))
          ((R:SylStructure.parent.sub_phrases < 1.1)
           ((0.759979 0.128316))
           ((0.775233 0.361383))))
         ((R:SylStructure.parent.last_accent < 1.3)
          ((0.696255 0.054136))
          ((0.632425 0.246742))))))))
    ((pos_in_syl < 0.3)
     ((R:SylStructure.parent.R:Syllable.p.syl_break is 2)
      ((0.847602 0.621547))
      ((ph_ctype is s)
       ((0.880645 0.501679))
       ((R:SylStructure.parent.sub_phrases < 3.3)
        ((R:SylStructure.parent.sub_phrases < 0.3)
         ((0.901014 -0.042049))
         ((0.657493 0.183226)))
        ((0.680126 0.284799)))))
     ((ph_ctype is s)
      ((p.ph_vlng is s)
       ((0.670033 -0.820934))
       ((0.863306 -0.348735)))
      ((ph_ctype is n)
       ((R:SylStructure.parent.asyl_in < 1.2)
        ((0.656966 -0.40092))
        ((0.530966 -0.639366)))
       ((seg_coda_fric is 0)
        ((1.04153 0.364857))
        ((pos_in_syl < 1.2)
         ((R:SylStructure.parent.syl_out < 3.4)
          ((0.81503 -0.00768613))
          ((0.602665 -0.197753)))
         ((0.601844 -0.394632)))))))))
  ((n.ph_ctype is f)
   ((pos_in_syl < 1.5)
    ((R:SylStructure.parent.R:Syllable.p.syl_break is 2)
     ((pos_in_syl < 0.1)
      ((1.63863 0.938841))
      ((R:SylStructure.parent.position_type is initial)
       ((0.897722 -0.0796637))
       ((nn.ph_vheight is 0)
        ((0.781081 0.480026))
        ((0.779711 0.127175)))))
     ((ph_ctype is r)
      ((p.ph_ctype is s)
       ((0.581329 -0.708767))
       ((0.564366 -0.236212)))
      ((ph_vlng is a)
       ((p.ph_ctype is r)
        ((0.70992 -0.273389))
        ((R:SylStructure.parent.parent.gpos is in)
         ((0.764696 0.0581338))
         ((nn.ph_vheight is 0)
          ((0.977737 0.721904))
          ((R:SylStructure.parent.sub_phrases < 2.2)
           ((pp.ph_vfront is 0)
            ((0.586708 0.0161206))
            ((0.619949 0.227372)))
           ((0.707285 0.445569))))))
       ((ph_ctype is n)
        ((R:SylStructure.parent.syl_break is 1)
         ((nn.ph_vfront is 2)
          ((0.430295 -0.120097))
          ((0.741371 0.219042)))
         ((0.587492 0.321245)))
        ((p.ph_ctype is n)
         ((0.871586 0.134075))
         ((p.ph_ctype is r)
          ((0.490751 -0.466418))
          ((R:SylStructure.parent.syl_codasize < 1.3)
           ((R:SylStructure.parent.sub_phrases < 2.2)
            ((p.ph_ctype is s)
             ((0.407452 -0.425925))
             ((0.644771 -0.542809)))
            ((0.688772 -0.201899)))
           ((ph_vheight is 1)
            ((nn.ph_vheight is 0)
             ((0.692018 0.209018))
             ((0.751345 -0.178136)))
            ((R:SylStructure.parent.R:Syllable.n.syl_onsetsize < 0.3)
             ((R:SylStructure.parent.asyl_in < 1.5)
              ((0.599633 -0.235593))
              ((0.60042 0.126118)))
             ((p.ph_vlng is a)
              ((0.7148 -0.174812))
              ((R:SylStructure.parent.parent.gpos is content)
               ((0.761296 -0.231509))
               ((0.813081 -0.536405)))))))))))))
    ((ph_ctype is n)
     ((0.898844 0.163343))
     ((p.ph_vlng is s)
      ((seg_coda_fric is 0)
       ((0.752921 -0.45528))
       ((0.890079 -0.0998025)))
      ((ph_ctype is f)
       ((0.729376 -0.930547))
       ((ph_ctype is s)
        ((R:SylStructure.parent.R:Syllable.p.syl_break is 0)
         ((0.745052 -0.634119))
         ((0.521502 -0.760176)))
        ((R:SylStructure.parent.syl_break is 1)
         ((0.766575 -0.121355))
         ((0.795616 -0.557509))))))))
   ((p.ph_vlng is 0)
    ((p.ph_ctype is r)
     ((ph_vlng is 0)
      ((0.733659 -0.402734))
      ((R:SylStructure.parent.sub_phrases < 1.5)
       ((ph_vlng is s)
        ((0.326176 -0.988478))
        ((n.ph_ctype is s)
         ((0.276471 -0.802536))
         ((0.438283 -0.900628))))
       ((nn.ph_vheight is 0)
        ((ph_vheight is 2)
         ((0.521 -0.768992))
         ((0.615436 -0.574918)))
        ((ph_vheight is 1)
         ((0.387376 -0.756359))
         ((pos_in_syl < 0.3)
          ((0.417235 -0.808937))
          ((0.384043 -0.93315)))))))
     ((ph_vlng is a)
      ((ph_ctype is 0)
       ((n.ph_ctype is s)
        ((p.ph_ctype is f)
         ((R:SylStructure.parent.R:Syllable.n.syl_onsetsize < 0.2)
          ((0.415908 -0.428493))
          ((pos_in_syl < 0.1)
           ((0.790441 0.0211071))
           ((0.452465 -0.254485))))
         ((p.ph_ctype is s)
          ((R:SylStructure.parent.R:Syllable.n.syl_onsetsize < 0.2)
           ((0.582447 -0.389966))
           ((0.757648 0.185781)))
          ((R:SylStructure.parent.sub_phrases < 1.4)
           ((0.628965 0.422551))
           ((0.713613 0.145576)))))
        ((seg_onset_stop is 0)
         ((R:SylStructure.parent.R:Syllable.p.syl_break is 0)
          ((pp.ph_vfront is 1)
           ((0.412363 -0.62319))
           ((R:SylStructure.parent.syl_out < 3.6)
            ((0.729259 -0.317324))
            ((0.441633 -0.591051))))
          ((R:SylStructure.parent.syl_break is 1)
           ((R:SylStructure.parent.sub_phrases < 2.7)
            ((0.457728 -0.405607))
            ((0.532411 -0.313148)))
           ((R:SylStructure.parent.last_accent < 0.3)
            ((1.14175 0.159416))
            ((0.616396 -0.254651)))))
         ((R:SylStructure.parent.position_type is initial)
          ((0.264181 -0.799896))
          ((0.439801 -0.551309)))))
       ((R:SylStructure.parent.position_type is final)
        ((0.552027 -0.707084))
        ((0.585661 -0.901874))))
      ((ph_ctype is s)
       ((pos_in_syl < 1.2)
        ((R:SylStructure.parent.R:Syllable.n.syl_onsetsize < 0.2)
         ((pp.ph_vfront is 1)
          ((0.607449 0.196466))
          ((0.599662 0.00382414)))
         ((0.64109 -0.12859)))
        ((pp.ph_vfront is 1)
         ((0.720484 -0.219339))
         ((0.688707 -0.516734))))
       ((ph_vlng is s)
        ((n.ph_ctype is s)
         ((R:SylStructure.parent.parent.gpos is content)
          ((R:SylStructure.parent.position_type is single)
           ((0.659206 0.159445))
           ((R:SylStructure.parent.parent.word_numsyls < 3.5)
            ((R:SylStructure.parent.sub_phrases < 2)
             ((0.447186 -0.419103))
             ((0.631822 -0.0928561)))
            ((0.451623 -0.576116))))
          ((ph_vheight is 3)
           ((0.578626 -0.64583))
           ((0.56636 -0.4665))))
         ((R:SylStructure.parent.parent.gpos is in)
          ((0.771516 -0.217292))
          ((R:SylStructure.parent.R:Syllable.p.syl_break is 2)
           ((0.688571 -0.304382))
           ((R:SylStructure.parent.parent.gpos is content)
            ((R:SylStructure.parent.R:Syllable.p.syl_break is 1)
             ((n.ph_ctype is n)
              ((0.556085 -0.572203))
              ((0.820173 -0.240338)))
             ((R:SylStructure.parent.parent.word_numsyls < 2.2)
              ((0.595398 -0.588171))
              ((0.524737 -0.95797))))
            ((R:SylStructure.parent.sub_phrases < 3.9)
             ((0.371492 -0.959427))
             ((0.440479 -0.845747)))))))
        ((R:SylStructure.parent.R:Syllable.p.syl_break is 0)
         ((p.ph_ctype is f)
          ((0.524088 -0.482247))
          ((nn.ph_vheight is 1)
           ((0.587666 -0.632362))
           ((ph_vlng is l)
            ((R:SylStructure.parent.position_type is final)
             ((0.513286 -0.713117))
             ((0.604613 -0.924308)))
            ((R:SylStructure.parent.syl_codasize < 2.2)
             ((0.577997 -0.891342))
             ((0.659804 -1.15252))))))
         ((pp.ph_vlng is s)
          ((ph_ctype is f)
           ((0.813383 -0.599624))
           ((0.984027 -0.0771909)))
          ((p.ph_ctype is f)
           ((R:SylStructure.parent.parent.gpos is in)
            ((R:SylStructure.parent.R:Syllable.p.syl_break is 1)
             ((0.313572 -1.03242))
             ((0.525854 -0.542799)))
            ((R:SylStructure.parent.syl_out < 2.8)
             ((0.613007 -0.423979))
             ((0.570258 -0.766379))))
           ((R:SylStructure.parent.syl_break is 1)
            ((R:SylStructure.parent.parent.gpos is to)
             ((0.364585 -0.792895))
             ((ph_vlng is l)
              ((0.69143 -0.276816))
              ((0.65673 -0.523721))))
            ((R:SylStructure.parent.syl_out < 3.6)
             ((R:SylStructure.parent.position_type is initial)
              ((0.682096 -0.488102))
              ((0.406364 -0.731758)))
             ((0.584694 -0.822229)))))))))))
    ((n.ph_ctype is r)
     ((R:SylStructure.parent.position_type is initial)
      ((p.ph_vlng is a)
       ((0.797058 1.02334))
       ((ph_ctype is s)
        ((1.0548 0.536277))
        ((0.817253 0.138201))))
      ((R:SylStructure.parent.sub_phrases < 1.1)
       ((R:SylStructure.parent.syl_out < 3.3)
        ((0.884574 -0.23471))
        ((0.772063 -0.525292)))
       ((nn.ph_vfront is 1)
        ((1.25254 0.417485))
        ((0.955557 -0.0781996)))))
     ((pp.ph_vfront is 0)
      ((ph_ctype is f)
       ((n.ph_ctype is s)
        ((R:SylStructure.parent.parent.gpos is content)
         ((R:SylStructure.parent.R:Syllable.p.syl_break is 0)
          ((0.583506 -0.56941))
          ((0.525949 -0.289362)))
         ((0.749316 -0.0921038)))
        ((p.ph_vlng is s)
         ((0.734234 0.139463))
         ((0.680119 -0.0708717))))
       ((ph_vlng is s)
        ((ph_vheight is 1)
         ((0.908712 -0.618971))
         ((0.55344 -0.840495)))
        ((R:SylStructure.parent.R:Syllable.n.syl_onsetsize < 1.2)
         ((pos_in_syl < 1.2)
          ((R:SylStructure.parent.R:Syllable.p.syl_break is 2)
           ((0.838715 0.00913392))
           ((R:SylStructure.parent.R:Syllable.p.syl_break is 1)
            ((ph_vheight is 2)
             ((0.555513 -0.512523))
             ((R:SylStructure.parent.position_type is initial)
              ((0.758711 0.121704))
              ((0.737555 -0.25637))))
            ((R:SylStructure.parent.syl_out < 3.1)
             ((n.ph_ctype is s)
              ((0.611756 -0.474522))
              ((1.05437 -0.247206)))
             ((R:SylStructure.parent.syl_codasize < 2.2)
              ((R:SylStructure.parent.position_type is final)
               ((0.567761 -0.597866))
               ((0.785599 -0.407765)))
              ((0.575598 -0.741256))))))
          ((ph_ctype is s)
           ((n.ph_ctype is s)
            ((0.661069 -1.08426))
            ((0.783184 -0.39789)))
           ((R:SylStructure.parent.R:Syllable.p.syl_break is 1)
            ((R:SylStructure.parent.sub_phrases < 2.6)
             ((0.511323 -0.666011))
             ((0.691878 -0.499492)))
            ((ph_ctype is r)
             ((0.482131 -0.253186))
             ((0.852955 -0.372832))))))
         ((0.854447 -0.0936489)))))
      ((R:SylStructure.parent.position_type is final)
       ((0.685939 -0.249982))
       ((R:SylStructure.parent.syl_out < 3.2)
        ((0.989843 0.18086))
        ((0.686805 -0.0402908)))))))))
 ((R:SylStructure.parent.syl_out < 2.4)
  ((R:SylStructure.parent.syl_out < 0.2)
   ((seg_onsetcoda is coda)
    ((ph_ctype is s)
     ((R:SylStructure.parent.syl_break is 4)
      ((pp.ph_vlng is 0)
       ((0.959737 1.63203))
       ((1.20714 0.994933)))
      ((n.ph_ctype is 0)
       ((R:SylStructure.parent.syl_break is 2)
        ((0.864809 0.214457))
        ((0.874278 0.730381)))
       ((pp.ph_vfront is 0)
        ((seg_coda_fric is 0)
         ((1.20844 -0.336221))
         ((1.01357 0.468302)))
        ((0.658106 -0.799121)))))
     ((n.ph_ctype is f)
      ((ph_ctype is f)
       ((1.26332 0.0300613))
       ((ph_vlng is d)
        ((1.02719 1.1649))
        ((ph_ctype is 0)
         ((R:SylStructure.parent.asyl_in < 1.2)
          ((1.14048 2.2668))
          ((ph_vheight is 1)
           ((1.15528 1.50375))
           ((1.42406 2.07927))))
         ((R:SylStructure.parent.sub_phrases < 1.1)
          ((0.955892 1.10243))
          ((R:SylStructure.parent.syl_break is 2)
           ((1.32682 1.8432))
           ((1.27582 1.59853)))))))
      ((n.ph_ctype is 0)
       ((ph_ctype is n)
        ((R:SylStructure.parent.syl_break is 2)
         ((1.45399 1.12927))
         ((1.05543 0.442376)))
        ((R:SylStructure.parent.syl_break is 4)
         ((R:SylStructure.parent.position_type is final)
          ((ph_ctype is f)
           ((1.46434 1.76508))
           ((0.978055 0.7486)))
          ((1.2395 2.30826)))
         ((ph_ctype is 0)
          ((0.935325 1.69917))
          ((nn.ph_vfront is 1)
           ((1.20456 1.31128))
           ((R:SylStructure.parent.R:Syllable.n.syl_onsetsize < 0.2)
            ((nn.ph_vheight is 0)
             ((1.16907 0.212421))
             ((0.952091 0.653094)))
            ((p.ph_ctype is 0)
             ((1.05502 1.25802))
             ((0.818731 0.777568))))))))
       ((ph_ctype is f)
        ((p.ph_ctype is 0)
         ((1.03918 0.163941))
         ((0.737545 -0.167063)))
        ((R:SylStructure.parent.position_type is final)
         ((n.ph_ctype is n)
          ((R:SylStructure.parent.last_accent < 0.5)
           ((R:SylStructure.parent.sub_phrases < 2.8)
            ((0.826207 -0.000859005))
            ((0.871119 0.273433)))
           ((R:SylStructure.parent.parent.word_numsyls < 2.4)
            ((1.17405 1.05694))
            ((0.858394 0.244916))))
          ((R:SylStructure.parent.syl_codasize < 2.2)
           ((p.ph_ctype is 0)
            ((1.14092 1.21187))
            ((R:SylStructure.parent.syl_break is 2)
             ((1.02653 0.59865))
             ((0.94248 1.1634))))
           ((seg_coda_fric is 0)
            ((1.07441 0.292935))
            ((1.15736 0.92574)))))
         ((ph_vlng is s)
          ((R:SylStructure.parent.syl_break is 2)
           ((1.34638 1.23484))
           ((0.951514 2.02008)))
          ((ph_ctype is 0)
           ((p.ph_ctype is r)
            ((0.806106 0.697089))
            ((R:SylStructure.parent.syl_break is 2)
             ((1.10891 0.992197))
             ((1.04657 1.51093))))
           ((1.18165 0.520952)))))))))
    ((p.ph_vlng is 0)
     ((pos_in_syl < 0.7)
      ((R:SylStructure.parent.position_type is final)
       ((ph_ctype is r)
        ((0.966357 0.185827))
        ((ph_ctype is s)
         ((0.647163 0.0332298))
         ((0.692972 -0.534917))))
       ((ph_ctype is s)
        ((0.881521 0.575107))
        ((p.ph_ctype is f)
         ((0.8223 -0.111275))
         ((R:SylStructure.parent.last_accent < 0.3)
          ((0.969188 0.09447))
          ((0.894438 0.381947))))))
      ((p.ph_ctype is f)
       ((0.479748 -0.490108))
       ((0.813125 -0.201268))))
     ((ph_ctype is s)
      ((0.908566 1.20397))
      ((R:SylStructure.parent.last_accent < 1.2)
       ((0.88078 0.636568))
       ((0.978087 1.07763))))))
   ((pos_in_syl < 1.3)
    ((R:SylStructure.parent.syl_break is 0)
     ((pos_in_syl < 0.1)
      ((R:SylStructure.parent.position_type is initial)
       ((p.ph_ctype is n)
        ((0.801651 -0.0163359))
        ((ph_ctype is s)
         ((n.ph_ctype is r)
          ((0.893307 1.07253))
          ((p.ph_vlng is 0)
           ((0.92651 0.525806))
           ((0.652444 0.952792))))
         ((p.ph_vlng is 0)
          ((seg_onsetcoda is coda)
           ((0.820151 0.469117))
           ((p.ph_ctype is f)
            ((0.747972 -0.0716448))
            ((ph_ctype is f)
             ((0.770882 0.457137))
             ((0.840905 0.102492)))))
          ((R:SylStructure.parent.syl_out < 1.1)
           ((0.667824 0.697337))
           ((0.737967 0.375114))))))
       ((ph_vheight is 1)
        ((0.624353 0.410671))
        ((R:SylStructure.parent.asyl_in < 0.8)
         ((0.647905 -0.331055))
         ((p.ph_ctype is s)
          ((0.629039 -0.240616))
          ((0.749277 -0.0191273))))))
      ((ph_vheight is 3)
       ((p.ph_ctype is s)
        ((0.626922 0.556537))
        ((0.789357 0.153892)))
       ((seg_onsetcoda is coda)
        ((n.ph_ctype is 0)
         ((R:SylStructure.parent.parent.word_numsyls < 3.4)
          ((0.744714 0.123242))
          ((0.742039 0.295753)))
         ((seg_coda_fric is 0)
          ((R:SylStructure.parent.parent.word_numsyls < 2.4)
           ((ph_vheight is 1)
            ((0.549715 -0.341018))
            ((0.573641 -0.00893114)))
           ((nn.ph_vfront is 2)
            ((0.67099 -0.744625))
            ((0.664438 -0.302803))))
          ((p.ph_vlng is 0)
           ((0.630028 0.113815))
           ((0.632794 -0.128733)))))
        ((ph_ctype is r)
         ((0.367169 -0.854509))
         ((0.94334 -0.216179))))))
     ((n.ph_ctype is f)
      ((ph_vlng is 0)
       ((1.3089 0.46195))
       ((R:SylStructure.parent.syl_codasize < 1.3)
        ((1.07673 0.657169))
        ((pp.ph_vlng is 0)
         ((0.972319 1.08222))
         ((1.00038 1.46257)))))
      ((p.ph_vlng is l)
       ((1.03617 0.785204))
       ((p.ph_vlng is a)
        ((R:SylStructure.parent.position_type is final)
         ((1.00681 0.321168))
         ((0.928115 0.950834)))
        ((ph_vlng is 0)
         ((pos_in_syl < 0.1)
          ((R:SylStructure.parent.position_type is final)
           ((0.863682 -0.167374))
           ((nn.ph_vheight is 0)
            ((p.ph_ctype is f)
             ((0.773591 -0.00374425))
             ((R:SylStructure.parent.syl_out < 1.1)
              ((0.951802 0.228448))
              ((1.02282 0.504252))))
            ((1.09721 0.736476))))
          ((R:SylStructure.parent.position_type is final)
           ((1.04302 0.0590974))
           ((0.589208 -0.431535))))
         ((n.ph_ctype is 0)
          ((1.27879 1.00642))
          ((ph_vlng is s)
           ((R:SylStructure.parent.asyl_in < 1.4)
            ((0.935787 0.481652))
            ((0.9887 0.749861)))
           ((R:SylStructure.parent.syl_out < 1.1)
            ((R:SylStructure.parent.position_type is final)
             ((0.921307 0.0696307))
             ((0.83675 0.552212)))
            ((0.810076 -0.0479225))))))))))
    ((ph_ctype is s)
     ((n.ph_ctype is s)
      ((0.706959 -1.0609))
      ((p.ph_ctype is n)
       ((0.850614 -0.59933))
       ((n.ph_ctype is r)
        ((0.665947 0.00698725))
        ((n.ph_ctype is 0)
         ((R:SylStructure.parent.position_type is initial)
          ((0.762889 -0.0649044))
          ((0.723956 -0.248899)))
         ((R:SylStructure.parent.sub_phrases < 1.4)
          ((0.632957 -0.601987))
          ((0.889114 -0.302401)))))))
     ((ph_ctype is f)
      ((R:SylStructure.parent.syl_codasize < 2.2)
       ((R:SylStructure.parent.R:Syllable.n.syl_onsetsize < 0.2)
        ((R:SylStructure.parent.syl_out < 1.1)
         ((0.865267 0.164636))
         ((0.581827 -0.0989051)))
        ((nn.ph_vfront is 2)
         ((0.684459 -0.316836))
         ((0.778854 -0.0961191))))
       ((R:SylStructure.parent.syl_out < 1.1)
        ((p.ph_ctype is s)
         ((0.837964 -0.429437))
         ((0.875304 -0.0652743)))
        ((0.611071 -0.635089))))
      ((p.ph_ctype is r)
       ((R:SylStructure.parent.syl_out < 1.1)
        ((0.762012 0.0139361))
        ((0.567983 -0.454845)))
       ((R:SylStructure.parent.syl_codasize < 2.2)
        ((ph_ctype is l)
         ((1.18845 0.809091))
         ((R:SylStructure.parent.position_type is initial)
          ((ph_ctype is n)
           ((0.773548 -0.277092))
           ((1.01586 0.281001)))
          ((p.ph_ctype is 0)
           ((1.06831 0.699145))
           ((0.924189 0.241873)))))
        ((R:SylStructure.parent.syl_break is 0)
         ((ph_ctype is n)
          ((0.592321 -0.470784))
          ((0.778688 -0.072112)))
         ((n.ph_ctype is s)
          ((1.08848 0.0733489))
          ((1.25674 0.608371))))))))))
  ((pos_in_syl < 0.7)
   ((p.ph_vlng is 0)
    ((R:SylStructure.parent.position_type is mid)
     ((ph_ctype is 0)
      ((ph_vheight is 2)
       ((0.456225 -0.293282))
       ((0.561529 -0.0816115)))
      ((0.6537 -0.504024)))
     ((ph_ctype is s)
      ((R:SylStructure.parent.R:Syllable.p.syl_break is 2)
       ((1.31586 0.98395))
       ((R:SylStructure.parent.position_type is single)
        ((0.816869 0.634789))
        ((R:SylStructure.parent.syl_out < 4.4)
         ((1.05578 0.479029))
         ((R:SylStructure.parent.asyl_in < 0.4)
          ((1.11813 0.143214))
          ((0.87178 0.406834))))))
      ((n.ph_ctype is n)
       ((R:SylStructure.parent.last_accent < 0.6)
        ((0.838154 -0.415599))
        ((0.924024 0.110288)))
       ((seg_onsetcoda is coda)
        ((nn.ph_vfront is 2)
         ((0.670096 0.0314187))
         ((n.ph_ctype is f)
          ((1.00363 0.693893))
          ((R:SylStructure.parent.syl_out < 6)
           ((0.772363 0.215675))
           ((0.920313 0.574068)))))
        ((R:SylStructure.parent.position_type is final)
         ((0.673837 -0.458142))
         ((R:SylStructure.parent.sub_phrases < 2.8)
          ((R:SylStructure.parent.R:Syllable.p.syl_break is 2)
           ((0.894817 0.304628))
           ((ph_ctype is n)
            ((0.787302 -0.23094))
            ((R:SylStructure.parent.asyl_in < 1.2)
             ((ph_ctype is f)
              ((R:SylStructure.parent.last_accent < 0.5)
               ((1.12278 0.326954))
               ((0.802236 -0.100616)))
              ((0.791255 -0.0919132)))
             ((0.95233 0.219053)))))
          ((R:SylStructure.parent.position_type is initial)
           ((ph_ctype is f)
            ((1.0616 0.216118))
            ((0.703216 -0.00834086)))
           ((ph_ctype is f)
            ((1.22277 0.761763))
            ((0.904811 0.332721))))))))))
    ((ph_vheight is 0)
     ((p.ph_vlng is s)
      ((0.873379 0.217178))
      ((n.ph_ctype is r)
       ((0.723915 1.29451))
       ((n.ph_ctype is 0)
        ((R:SylStructure.parent.R:Syllable.p.syl_break is 1)
         ((R:SylStructure.parent.sub_phrases < 4)
          ((seg_coda_fric is 0)
           ((p.ph_vlng is l)
            ((0.849154 0.945261))
            ((0.633261 0.687498)))
           ((0.728546 0.403076)))
          ((0.850962 1.00255)))
         ((0.957999 1.09113)))
        ((0.85771 0.209045)))))
     ((ph_vheight is 2)
      ((0.803401 -0.0544067))
      ((0.681353 0.256045)))))
   ((n.ph_ctype is f)
    ((ph_ctype is s)
     ((p.ph_vlng is 0)
      ((0.479307 -0.9673))
      ((0.700477 -0.351397)))
     ((ph_ctype is f)
      ((0.73467 -0.6233))
      ((R:SylStructure.parent.syl_break is 0)
       ((p.ph_ctype is s)
        ((0.56282 0.266234))
        ((p.ph_ctype is r)
         ((0.446203 -0.302281))
         ((R:SylStructure.parent.sub_phrases < 2.7)
          ((ph_ctype is 0)
           ((0.572016 -0.0102436))
           ((0.497358 -0.274514)))
          ((0.545477 0.0482177)))))
       ((ph_vlng is s)
        ((0.805269 0.888495))
        ((ph_ctype is n)
         ((0.869854 0.653018))
         ((R:SylStructure.parent.sub_phrases < 2.2)
          ((0.735031 0.0612886))
          ((0.771859 0.346637))))))))
    ((R:SylStructure.parent.syl_codasize < 1.4)
     ((R:SylStructure.parent.R:Syllable.n.syl_onsetsize < 0.3)
      ((R:SylStructure.parent.position_type is initial)
       ((0.743458 0.0411808))
       ((1.13068 0.613305)))
      ((pos_in_syl < 1.2)
       ((R:SylStructure.parent.R:Syllable.p.syl_break is 1)
        ((1.11481 0.175467))
        ((0.937893 -0.276407)))
       ((0.74264 -0.550878))))
     ((pos_in_syl < 3.4)
      ((seg_onsetcoda is coda)
       ((ph_ctype is r)
        ((n.ph_ctype is s)
         ((0.714319 -0.240328))
         ((p.ph_ctype is 0)
          ((0.976987 0.330352))
          ((1.1781 -0.0816682))))
        ((ph_ctype is l)
         ((n.ph_ctype is 0)
          ((1.39137 0.383533))
          ((0.725585 -0.324515)))
         ((ph_vheight is 3)
          ((ph_vlng is d)
           ((0.802626 -0.62487))
           ((n.ph_ctype is r)
            ((0.661091 -0.513869))
            ((R:SylStructure.parent.position_type is initial)
             ((R:SylStructure.parent.parent.word_numsyls < 2.4)
              ((0.482285 0.207874))
              ((0.401601 -0.0204711)))
             ((0.733755 0.397372)))))
          ((n.ph_ctype is r)
           ((p.ph_ctype is 0)
            ((pos_in_syl < 1.2)
             ((0.666325 0.271734))
             ((nn.ph_vheight is 0)
              ((0.642401 -0.261466))
              ((0.783684 -0.00956571))))
            ((R:SylStructure.parent.R:Syllable.n.syl_onsetsize < 0.2)
             ((0.692225 -0.381895))
             ((0.741921 -0.0898767))))
           ((nn.ph_vfront is 2)
            ((ph_ctype is s)
             ((0.697527 -1.12626))
             ((n.ph_ctype is s)
              ((ph_vlng is 0)
               ((R:SylStructure.parent.sub_phrases < 2.4)
                ((0.498719 -0.906926))
                ((0.635342 -0.625651)))
               ((0.45886 -0.385089)))
              ((0.848596 -0.359702))))
            ((p.ph_vlng is a)
             ((p.ph_ctype is 0)
              ((0.947278 0.216904))
              ((0.637933 -0.394349)))
             ((p.ph_ctype is r)
              ((R:SylStructure.parent.syl_break is 0)
               ((0.529903 -0.860573))
               ((0.581378 -0.510488)))
              ((ph_vlng is 0)
               ((R:SylStructure.parent.R:Syllable.n.syl_onsetsize < 0.2)
                ((seg_onset_stop is 0)
                 ((R:SylStructure.parent.syl_break is 0)
                  ((p.ph_vlng is d)
                   ((0.768363 0.0108428))
                   ((ph_ctype is s)
                    ((0.835756 -0.035054))
                    ((ph_ctype is f)
                     ((p.ph_vlng is s)
                      ((0.602016 -0.179727))
                      ((0.640126 -0.297341)))
                     ((0.674628 -0.542602)))))
                  ((ph_ctype is s)
                   ((0.662261 -0.60496))
                   ((0.662088 -0.432058))))
                 ((R:SylStructure.parent.syl_out < 4.4)
                  ((0.582448 -0.389079))
                  ((ph_ctype is s)
                   ((0.60413 -0.73564))
                   ((0.567153 -0.605444)))))
                ((R:SylStructure.parent.R:Syllable.p.syl_break is 2)
                 ((0.761115 -0.827377))
                 ((ph_ctype is n)
                  ((0.855183 -0.275338))
                  ((R:SylStructure.parent.syl_break is 0)
                   ((0.788288 -0.802801))
                   ((R:SylStructure.parent.syl_codasize < 2.2)
                    ((0.686134 -0.371234))
                    ((0.840184 -0.772883)))))))
               ((pos_in_syl < 1.2)
                ((R:SylStructure.parent.syl_break is 0)
                 ((n.ph_ctype is n)
                  ((0.423592 -0.655006))
                  ((R:SylStructure.parent.syl_out < 4.4)
                   ((0.595269 -0.303751))
                   ((0.478433 -0.456882))))
                 ((0.688133 -0.133182)))
                ((seg_onset_stop is 0)
                 ((1.27464 0.114442))
                 ((0.406837 -0.167545))))))))))))
       ((ph_ctype is r)
        ((0.462874 -0.87695))
        ((R:SylStructure.parent.R:Syllable.n.syl_onsetsize < 0.2)
         ((0.645442 -0.640572))
         ((0.673717 -0.321322)))))
      ((0.61008 -0.925472))))))))
;; RMSE 0.8085 Correlation is 0.5899 Mean (abs) Error 0.6024 (0.5393)


))

(provide 'apml_kaldurtreeZ)
