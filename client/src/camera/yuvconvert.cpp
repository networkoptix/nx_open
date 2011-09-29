#include <stdio.h>
#include <stdlib.h>
#include <QtGlobal>

/* hope these constant values are cache line aligned */

#ifdef Q_CC_MSVC
#define USED_U64(foo) static volatile const unsigned long long foo
#elif __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)
#define USED_U64(foo) \
    static const quint64 foo __asm__ (#foo) __attribute__((used))
#else
#define USED_U64(foo) \
    static const uint64_t foo __asm__ (#foo) __attribute__((used))
#endif


/* hope these constant values are cache line aligned */

USED_U64(mmx_alpha)   = 0xff000000ff000000ULL;

USED_U64(mmx_80w)     = 0x0080008000800080ULL;

USED_U64(mmx_10w)     = 0x1010101010101010ull;
USED_U64(mmx_00ffw)   = 0x00ff00ff00ff00ffull;
USED_U64(mmx_Y_coeff) = 0x253f253f253f253full;

/* hope these constant values are cache line aligned */
USED_U64(mmx_U_green) = 0xf37df37df37df37dull;
USED_U64(mmx_U_blue)  = 0x4093409340934093ull;
USED_U64(mmx_V_red)   = 0x3312331233123312ull;
USED_U64(mmx_V_green) = 0xe5fce5fce5fce5fcull;

/* hope these constant values are cache line aligned */
USED_U64(mmx_redmask) = 0xf8f8f8f8f8f8f8f8ull;
USED_U64(mmx_grnmask) = 0xfcfcfcfcfcfcfcfcull;
USED_U64(mmx_grnshift)   = 0x03ull;
USED_U64(mmx_blueshift)  = 0x03ull;

#undef USED_U64

#ifdef Q_CC_MSVC
void yuv420_argb32_mmx(unsigned char * image, const unsigned char * py,
			      const unsigned char * pu, const unsigned char * pv,
			      const unsigned int h_size, const unsigned int v_size,
			      const unsigned int image_stride, const unsigned int y_stride,
			      const unsigned int uv_stride)
{
    unsigned int even = 0;
    unsigned int x = 0, y = 0;

	__asm {
		mov eax, [pu]
		mov ebx, [pv]
		mov esi, [py]
		mov edi, [image]

	     .align 8
	     movd      mm0, [eax] ; pu
	     mov      [edi], 0 ; image

	     movd mm1, [ebx] ; pv
	     pxor mm4, mm4

	     movq  mm6, [esi] ; py
	     //: : "r" (py), "r" (pu), "r" (pv), "r" (image));
	}
    do {
	do {
	    /* this mmx assembly code deals with SINGLE scan line at a time, it convert 8
	       pixels in each iteration */
		__asm {
			;push eax
			;push ebx
			;push esi
			;push edi

			mov eax, [pu]
			mov ebx, [pv]
			mov esi, [py]
			mov edi, [image]
		     .align 8;
		     /* Do the multiply part of the conversion for even and odd pixels,
			register usage:
			mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
			mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels,
			mm6 -> Y even, mm7 -> Y odd */
		     /* convert the chroma part */
		     punpcklbw mm0, mm4
		     punpcklbw mm1, mm4

		     psubsw    mm0, mmx_80w
		     psubsw    mm1, mmx_80w

		     psllw     mm0, 3
		     psllw     mm1, 3

		     movq      mm2, mm0
		     movq      mm3,mm1

		     pmulhw    mm2, mmx_U_green
		     pmulhw    mm3, mmx_V_green

		     pmulhw    mm0, mmx_U_blue
		     pmulhw    mm1, mmx_V_red

		     paddsw    mm2, mm3

		     /* convert the luma part */
		     psubusb   mm6, mmx_10w

		     movq      mm7, mm6
		     pand      mm6, mmx_00ffw

		     psrlw     mm7, 8

		     psllw     mm6, 3
		     psllw     mm7, 3

		     pmulhw    mm6, mmx_Y_coeff
		     pmulhw    mm7, mmx_Y_coeff

		     /* Do the addition part of the conversion for even and odd pixels,
			register usage:
			mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
			mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels,
			mm6 -> Y even, mm7 -> Y odd */
		     movq      mm3, mm0
		     movq      mm4, mm1
		     movq      mm5, mm2

		     paddsw    mm0, mm6
		     paddsw    mm3, mm7

		     paddsw    mm1, mm6
		     paddsw    mm4, mm7

		     paddsw    mm2, mm6
		     paddsw    mm5, mm7

		     /* Limit RGB even to 0..255 */
		     packuswb  mm0, mm0
		     packuswb  mm1, mm1
		     packuswb  mm2, mm2

		     /* Limit RGB odd to 0..255 */
		     packuswb  mm3, mm3
		     packuswb  mm4, mm4      
		     packuswb  mm5, mm5      

		     /* Interleave RGB even and odd */
		     punpcklbw mm0, mm3
		     punpcklbw mm1, mm4
		     punpcklbw mm2, mm5

		     /* convert RGB plane to RGB packed format, 
			mm0 -> B, mm1 -> R, mm2 -> G, mm3 -> 0,
			mm4 -> GB, mm5 -> AR pixel 4-7,
			mm6 -> GB, mm7 -> AR pixel 0-3 */
		     pxor      mm3, mm3

		     movq      mm6, mm0
		     movq      mm7, mm1

		     movq      mm4, mm0
		     movq      mm5, mm1

		     punpcklbw mm6, mm2
		     punpcklbw mm7, mm3

		     punpcklwd mm6, mm7  
			 por mm6, mmx_alpha ; my code
		     movq      [edi], mm6

		     movq      mm6, mm0
		     punpcklbw mm6, mm2

		     punpckhwd mm6, mm7
			 por mm6, mmx_alpha ; my code
		     movq      [edi+8],mm6

		     punpckhbw mm4, mm2
		     punpckhbw mm5, mm3
		     
		     punpcklwd mm4, mm5
			 por mm4, mmx_alpha ; my code
		     movq      [edi + 16], mm4

		     movq      mm4, mm0
		     punpckhbw mm4, mm2

		     punpckhwd mm4, mm5
			 por mm4, mmx_alpha ; my code
		     movq      [edi + 24], mm4

		     movd mm0, [eax+4]
		     movd mm1, [ebx+4]

		     pxor      mm4, mm4
		     movq  mm6, [esi + 8]

			;pop edi
			;pop esi
			;pop ebx
			;pop eax
		}
		     //: : "r" (py), "r" (pu), "r" (pv), "r" (image));

	    py      += 8;
	    pu      += 4;
	    pv      += 4;
	    image   += 32;
	    x       += 8;
	} while (x < h_size) ;

	if (even == 0) {
	    pu  -= h_size/2;
	    pv  -= h_size/2;
	} else  {
	    pu  += (uv_stride - h_size/2);
	    pv  += (uv_stride - h_size/2);
	}
	py      += (y_stride - h_size);
	image   += (image_stride - 4*h_size);

	/* load data for start of next scan line */
	__asm {
		mov esi, [py]
		mov edi, [image]
		mov eax, [pu]
		mov ebx, [pv]
		 .align 8
		 movd mm0,[eax]
		 movd mm1,[ebx]
		 mov [edi], 0
		 movq mm6, [esi]
		 //: : "r" (py), "r" (pu), "r" (pv), "r" (image));
	}

	x        = 0;
	y       += 1;
	even = (!even);
    } while ( y < v_size) ;
	__asm { emms }
}

void yuv422_argb32_mmx(unsigned char * image, const unsigned char * py,
			      const unsigned char * pu, const unsigned char * pv,
			      const unsigned int h_size, const unsigned int v_size,
			      const unsigned int image_stride, const unsigned int y_stride,
			      const unsigned int uv_stride)
{
    unsigned int x = 0, y = 0;

	__asm {
		mov eax, [pu]
		mov ebx, [pv]
		mov esi, [py]
		mov edi, [image]

	     .align 8
	     movd      mm0, [eax] ; pu
	     mov      [edi], 0 ; image

	     movd mm1, [ebx] ; pv
	     pxor mm4, mm4

	     movq  mm6, [esi] ; py
	     //: : "r" (py), "r" (pu), "r" (pv), "r" (image));
	}
    do {
	do {
	    /* this mmx assembly code deals with SINGLE scan line at a time, it convert 8
	       pixels in each iteration */
		__asm {
			;push eax
			;push ebx
			;push esi
			;push edi

			mov eax, [pu]
			mov ebx, [pv]
			mov esi, [py]
			mov edi, [image]
		     .align 8;
		     /* Do the multiply part of the conversion for even and odd pixels,
			register usage:
			mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
			mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels,
			mm6 -> Y even, mm7 -> Y odd */
		     /* convert the chroma part */
		     punpcklbw mm0, mm4
		     punpcklbw mm1, mm4

		     psubsw    mm0, mmx_80w
		     psubsw    mm1, mmx_80w

		     psllw     mm0, 3
		     psllw     mm1, 3

		     movq      mm2, mm0
		     movq      mm3,mm1

		     pmulhw    mm2, mmx_U_green
		     pmulhw    mm3, mmx_V_green

		     pmulhw    mm0, mmx_U_blue
		     pmulhw    mm1, mmx_V_red

		     paddsw    mm2, mm3

		     /* convert the luma part */
		     psubusb   mm6, mmx_10w

		     movq      mm7, mm6
		     pand      mm6, mmx_00ffw

		     psrlw     mm7, 8

		     psllw     mm6, 3
		     psllw     mm7, 3

		     pmulhw    mm6, mmx_Y_coeff
		     pmulhw    mm7, mmx_Y_coeff

		     /* Do the addition part of the conversion for even and odd pixels,
			register usage:
			mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
			mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels,
			mm6 -> Y even, mm7 -> Y odd */
		     movq      mm3, mm0
		     movq      mm4, mm1
		     movq      mm5, mm2

		     paddsw    mm0, mm6
		     paddsw    mm3, mm7

		     paddsw    mm1, mm6
		     paddsw    mm4, mm7

		     paddsw    mm2, mm6
		     paddsw    mm5, mm7

		     /* Limit RGB even to 0..255 */
		     packuswb  mm0, mm0
		     packuswb  mm1, mm1
		     packuswb  mm2, mm2

		     /* Limit RGB odd to 0..255 */
		     packuswb  mm3, mm3
		     packuswb  mm4, mm4      
		     packuswb  mm5, mm5      

		     /* Interleave RGB even and odd */
		     punpcklbw mm0, mm3
		     punpcklbw mm1, mm4
		     punpcklbw mm2, mm5

		     /* convert RGB plane to RGB packed format, 
			mm0 -> B, mm1 -> R, mm2 -> G, mm3 -> 0,
			mm4 -> GB, mm5 -> AR pixel 4-7,
			mm6 -> GB, mm7 -> AR pixel 0-3 */
		     pxor      mm3, mm3

		     movq      mm6, mm0
		     movq      mm7, mm1

		     movq      mm4, mm0
		     movq      mm5, mm1

		     punpcklbw mm6, mm2
		     punpcklbw mm7, mm3

		     punpcklwd mm6, mm7  
			 por mm6, mmx_alpha ; my code
		     movq      [edi], mm6

		     movq      mm6, mm0
		     punpcklbw mm6, mm2

		     punpckhwd mm6, mm7
			 por mm6, mmx_alpha ; my code
		     movq      [edi+8],mm6

		     punpckhbw mm4, mm2
		     punpckhbw mm5, mm3
		     
		     punpcklwd mm4, mm5
			 por mm4, mmx_alpha ; my code
		     movq      [edi + 16], mm4

		     movq      mm4, mm0
		     punpckhbw mm4, mm2

		     punpckhwd mm4, mm5
			 por mm4, mmx_alpha ; my code
		     movq      [edi + 24], mm4

		     movd mm0, [eax+4]
		     movd mm1, [ebx+4]

		     pxor      mm4, mm4
		     movq  mm6, [esi + 8]

			;pop edi
			;pop esi
			;pop ebx
			;pop eax
		}
		     //: : "r" (py), "r" (pu), "r" (pv), "r" (image));

	    py      += 8;
	    pu      += 4;
	    pv      += 4;
	    image   += 32;
	    x       += 8;
	} while (x < h_size) ;

    pu  += (uv_stride - h_size/2);
    pv  += (uv_stride - h_size/2);
	py      += (y_stride - h_size);
	image   += (image_stride - 4*h_size);

	/* load data for start of next scan line */
	__asm {
		mov esi, [py]
		mov edi, [image]
		mov eax, [pu]
		mov ebx, [pv]
		 .align 8
		 movd mm0,[eax]
		 movd mm1,[ebx]
		 mov [edi], 0
		 movq mm6, [esi]
		 //: : "r" (py), "r" (pu), "r" (pv), "r" (image));
	}

	x        = 0;
	y       += 1;
    } while ( y < v_size) ;
	__asm { emms }
}

void yuv444_argb32_mmx(unsigned char * image, const unsigned char * py,
			      const unsigned char * pu, const unsigned char * pv,
			      const unsigned int h_size, const unsigned int v_size,
			      const unsigned int image_stride, const unsigned int y_stride,
			      const unsigned int uv_stride)
{
    unsigned int x = 0, y = 0;

	__asm {
		mov eax, [pu]
		mov ebx, [pv]
		mov esi, [py]
		mov edi, [image]

	     .align 8
	     movq      mm0, [eax] ; pu (was movd)
	     mov      [edi], 0 ; image

	     movq mm1, [ebx] ; pv  (was movd)
	     pxor mm4, mm4

	     movq  mm6, [esi] ; py
	     //: : "r" (py), "r" (pu), "r" (pv), "r" (image));
	}
    do {
	do {
	    // this mmx assembly code deals with SINGLE scan line at a time, it convert 8
	       // pixels in each iteration 
		__asm {
			;push eax
			;push ebx
			;push esi
			;push edi

			mov eax, [pu]
			mov ebx, [pv]
			mov esi, [py]
			mov edi, [image]
		     .align 8;
		     // Do the multiply part of the conversion for even and odd pixels,
			//register usage:
			//mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
			//mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels,
			//mm6 -> Y even, mm7 -> Y odd 
		     // convert the chroma part 
			//punpcklbw mm0, mm4
			//punpcklbw mm1, mm4

		   // (U - 128), (V - 128)
			//psubsb    mm0, mmx_80w_full // psubsw    mm0, mmx_80w (U)
			//psubsb    mm1, mmx_80w_full // psubsw    mm1, mmx_80w (V)


			// prepare pair like Y in mm6, mm7
			movq      mm3, mm0
			movq      mm4,mm1

			pand      mm0, mmx_00ffw
			pand      mm1, mmx_00ffw

			psrlw     mm3, 8
			psrlw     mm4, 8

			// ????????????????
			psubsw    mm0, mmx_80w
			psubsw    mm3, mmx_80w
			psubsw    mm1, mmx_80w
			psubsw    mm4, mmx_80w
			//

			psllw     mm0, 3
			psllw     mm3, 3
			psllw     mm1, 3
			psllw     mm4, 3

			// mul
			pxor	mm2, mm2
			pxor	mm5, mm5

			//psubsw      mm2,mm0
			//psubsw      mm5,mm3
			paddsw      mm2,mm0
			paddsw      mm5,mm3

			pmulhw    mm2, mmx_U_green
			pmulhw    mm5, mmx_U_green

			movq      mm6,mm1
			movq      mm7,mm4
			pmulhw    mm6, mmx_V_green
			pmulhw    mm7, mmx_V_green

			//psubsw    mm2, mm6  // mm2, mm5 - green is ready
			//psubsw    mm5, mm7
			paddsw    mm2, mm6  // mm2, mm5 - green is ready
			paddsw    mm5, mm7

			pmulhw    mm0, mmx_U_blue // mm0, mm2 - blue is ready
			pmulhw    mm3, mmx_U_blue

			pmulhw    mm1, mmx_V_red  // mm1, mm3 - red is ready
			pmulhw    mm4, mmx_V_red

		     // convert the luma part 
			movq  mm6, [esi] ; py

		     psubusb   mm6, mmx_10w

		     movq      mm7, mm6
		     pand      mm6, mmx_00ffw

		     psrlw     mm7, 8

		     psllw     mm6, 3
		     psllw     mm7, 3

		     pmulhw    mm6, mmx_Y_coeff
		     pmulhw    mm7, mmx_Y_coeff

		     // Do the addition part of the conversion for even and odd pixels,
			//register usage:
			//mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
			//mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels,
			//mm6 -> Y even, mm7 -> Y odd 

			// add  1.164(Y - 16) to eath component
		     paddsw    mm0, mm6
		     paddsw    mm3, mm7

		     paddsw    mm1, mm6
		     paddsw    mm4, mm7

		     paddsw    mm2, mm6
		     paddsw    mm5, mm7
			

		     // Limit RGB even to 0..255 
		     packuswb  mm0, mm0
		     packuswb  mm1, mm1
		     packuswb  mm2, mm2

		     // Limit RGB odd to 0..255 
		     packuswb  mm3, mm3
		     packuswb  mm4, mm4      
		     packuswb  mm5, mm5      

		     // Interleave RGB even and odd 
		     punpcklbw mm0, mm3
		     punpcklbw mm1, mm4
		     punpcklbw mm2, mm5

			 // remove 3 lines after for change register

		     // convert RGB plane to RGB packed format, 
			//mm0 -> B, mm1 -> R, mm2 -> G, mm3 -> 0,
			//mm4 -> GB, mm5 -> AR pixel 4-7,
			//mm6 -> GB, mm7 -> AR pixel 0-3 
		     pxor      mm3, mm3

		     movq      mm6, mm0
		     movq      mm7, mm1

		     movq      mm4, mm0
		     movq      mm5, mm1

		     punpcklbw mm6, mm2
		     punpcklbw mm7, mm3

		     punpcklwd mm6, mm7  
			 por mm6, mmx_alpha ; my code
		     movq      [edi], mm6

		     movq      mm6, mm0
		     punpcklbw mm6, mm2

		     punpckhwd mm6, mm7
			 por mm6, mmx_alpha ; my code
		     movq      [edi+8],mm6

		     punpckhbw mm4, mm2
		     punpckhbw mm5, mm3
		     
		     punpcklwd mm4, mm5
			 por mm4, mmx_alpha ; my code
		     movq      [edi + 16], mm4

		     movq      mm4, mm0
		     punpckhbw mm4, mm2

		     punpckhwd mm4, mm5
			 por mm4, mmx_alpha ; my code
		     movq      [edi + 24], mm4

		     movq mm0, [eax+8]
		     movq mm1, [ebx+8]

		     pxor      mm4, mm4
		     movq  mm6, [esi + 8]

			;pop edi
			;pop esi
			;pop ebx
			;pop eax
		}
		     //: : "r" (py), "r" (pu), "r" (pv), "r" (image));

	    py      += 8;
	    pu      += 8;
	    pv      += 8;
	    image   += 32;
	    x       += 8;
	} while (x < h_size) ;

    pu  += (uv_stride - h_size);
    pv  += (uv_stride - h_size);
	py      += (y_stride - h_size);

	image   += (image_stride - 4*h_size);

	// load data for start of next scan line 
	__asm {
		mov esi, [py]
		mov edi, [image]
		mov eax, [pu]
		mov ebx, [pv]
		 .align 8
		 movq mm0,[eax]
		 movq mm1,[ebx]
		 mov [edi], 0
		 movq mm6, [esi]
		 //: : "r" (py), "r" (pu), "r" (pv), "r" (image));
	}

	x        = 0;
	y       += 1;
    } while ( y < v_size) ;
	__asm { emms }
}

#else
void yuv420_argb32_mmx(unsigned char * image, const unsigned char * py,
			      const unsigned char * pu, const unsigned char * pv,
			      const unsigned int h_size, const unsigned int v_size,
			      const unsigned int image_stride, const unsigned int y_stride,
			      const unsigned int uv_stride)
{
    unsigned int even = 0;
    unsigned int x = 0, y = 0;

    __asm__ (
	     ".align 8 \n\t"
	     "movd      (%1), %%mm0       # Load 4 Cb       00 00 00 00 u3 u2 u1 u0\n\t"
	     "movl      $0, (%3)          # cache preload for image\n\t"

	     "movd      (%2), %%mm1       # Load 4 Cr       00 00 00 00 v3 v2 v1 v0\n\t"
	     "pxor      %%mm4, %%mm4      # zero mm4\n\t"

	     "movq      (%0), %%mm6       # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"
	     : : "r" (py), "r" (pu), "r" (pv), "r" (image));

    do {
	do {
	    /* this mmx assembly code deals with SINGLE scan line at a time, it convert 8
	       pixels in each iteration */
	    __asm__ (
		     ".align 8 \n\t"
		     /* Do the multiply part of the conversion for even and odd pixels,
			register usage:
			mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
			mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels,
			mm6 -> Y even, mm7 -> Y odd */
		     /* convert the chroma part */
		     "punpcklbw %%mm4, %%mm0      # scatter 4 Cb    00 u3 00 u2 00 u1 00 u0\n\t"
		     "punpcklbw %%mm4, %%mm1      # scatter 4 Cr    00 v3 00 v2 00 v1 00 v0\n\t"

		     "psubsw    mmx_80w, %%mm0    # Cb -= 128\n\t"
		     "psubsw    mmx_80w, %%mm1    # Cr -= 128\n\t"

		     "psllw     $3, %%mm0         # Promote precision\n\t"
		     "psllw     $3, %%mm1         # Promote precision\n\t"

		     "movq      %%mm0, %%mm2      # Copy 4 Cb       00 u3 00 u2 00 u1 00 u0\n\t"
		     "movq      %%mm1, %%mm3      # Copy 4 Cr       00 v3 00 v2 00 v1 00 v0\n\t"

		     "pmulhw    mmx_U_green, %%mm2# Mul Cb with green coeff -> Cb green\n\t"
		     "pmulhw    mmx_V_green, %%mm3# Mul Cr with green coeff -> Cr green\n\t"

		     "pmulhw    mmx_U_blue, %%mm0 # Mul Cb -> Cblue 00 b3 00 b2 00 b1 00 b0\n\t"
		     "pmulhw    mmx_V_red, %%mm1  # Mul Cr -> Cred  00 r3 00 r2 00 r1 00 r0\n\t"

		     "paddsw    %%mm3, %%mm2      # Cb green + Cr green -> Cgreen\n\t"

		     /* convert the luma part */
		     "psubusb   mmx_10w, %%mm6    # Y -= 16\n\t"

		     "movq      %%mm6, %%mm7      # Copy 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"
		     "pand      mmx_00ffw, %%mm6  # get Y even      00 Y6 00 Y4 00 Y2 00 Y0\n\t"

		     "psrlw     $8, %%mm7         # get Y odd       00 Y7 00 Y5 00 Y3 00 Y1\n\t"

		     "psllw     $3, %%mm6         # Promote precision\n\t"
		     "psllw     $3, %%mm7         # Promote precision\n\t"

		     "pmulhw    mmx_Y_coeff, %%mm6# Mul 4 Y even    00 y6 00 y4 00 y2 00 y0\n\t"
		     "pmulhw    mmx_Y_coeff, %%mm7# Mul 4 Y odd     00 y7 00 y5 00 y3 00 y1\n\t"

		     /* Do the addition part of the conversion for even and odd pixels,
			register usage:
			mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
			mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels,
			mm6 -> Y even, mm7 -> Y odd */
		     "movq      %%mm0, %%mm3      # Copy Cblue\n\t"
		     "movq      %%mm1, %%mm4      # Copy Cred\n\t"
		     "movq      %%mm2, %%mm5      # Copy Cgreen\n\t"

		     "paddsw    %%mm6, %%mm0      # Y even + Cblue  00 B6 00 B4 00 B2 00 B0\n\t"
		     "paddsw    %%mm7, %%mm3      # Y odd  + Cblue  00 B7 00 B5 00 B3 00 B1\n\t"

		     "paddsw    %%mm6, %%mm1      # Y even + Cred   00 R6 00 R4 00 R2 00 R0\n\t"
		     "paddsw    %%mm7, %%mm4      # Y odd  + Cred   00 R7 00 R5 00 R3 00 R1\n\t"

		     "paddsw    %%mm6, %%mm2      # Y even + Cgreen 00 G6 00 G4 00 G2 00 G0\n\t"
		     "paddsw    %%mm7, %%mm5      # Y odd  + Cgreen 00 G7 00 G5 00 G3 00 G1\n\t"

		     /* Limit RGB even to 0..255 */
		     "packuswb  %%mm0, %%mm0      #                 B6 B4 B2 B0 B6 B4 B2 B0\n\t"
		     "packuswb  %%mm1, %%mm1      #                 R6 R4 R2 R0 R6 R4 R2 R0\n\t"
		     "packuswb  %%mm2, %%mm2      #                 G6 G4 G2 G0 G6 G4 G2 G0\n\t"

		     /* Limit RGB odd to 0..255 */
		     "packuswb  %%mm3, %%mm3      #                 B7 B5 B3 B1 B7 B5 B3 B1\n\t"
		     "packuswb  %%mm4, %%mm4      #                 R7 R5 R3 R1 R7 R5 R3 R1\n\t"
		     "packuswb  %%mm5, %%mm5      #                 G7 G5 G3 G1 G7 G5 G3 G1\n\t"

		     /* Interleave RGB even and odd */
		     "punpcklbw %%mm3, %%mm0      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "punpcklbw %%mm4, %%mm1      #                 R7 R6 R5 R4 R3 R2 R1 R0\n\t"
		     "punpcklbw %%mm5, %%mm2      #                 G7 G6 G5 G4 G3 G2 G1 G0\n\t"

		     /* convert RGB plane to RGB packed format, 
			mm0 -> B, mm1 -> R, mm2 -> G, mm3 -> 0,
			mm4 -> GB, mm5 -> AR pixel 4-7,
			mm6 -> GB, mm7 -> AR pixel 0-3 */
		     "pxor      %%mm3, %%mm3      # zero mm3\n\t"

		     "movq      %%mm0, %%mm6      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "movq      %%mm1, %%mm7      #                 R7 R6 R5 R4 R3 R2 R1 R0\n\t"

		     "movq      %%mm0, %%mm4      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "movq      %%mm1, %%mm5      #                 R7 R6 R5 R4 R3 R2 R1 R0\n\t"

		     "punpcklbw %%mm2, %%mm6      #                 G3 B3 G2 B2 G1 B1 G0 B0\n\t"
		     "punpcklbw %%mm3, %%mm7      #                 00 R3 00 R2 00 R1 00 R0\n\t"

		     "punpcklwd %%mm7, %%mm6      #                 00 R1 B1 G1 00 R0 B0 G0\n\t"
			 "por mmx_alpha, %%mm6        # Set alpha channel\n\t"
		     "movq      %%mm6, (%3)       # Store ARGB1 ARGB0\n\t"

		     "movq      %%mm0, %%mm6      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "punpcklbw %%mm2, %%mm6      #                 G3 B3 G2 B2 G1 B1 G0 B0\n\t"

		     "punpckhwd %%mm7, %%mm6      #                 00 R3 G3 B3 00 R2 B3 G2\n\t"
			 "por mmx_alpha, %%mm6        # Set alpha channel\n\t"
		     "movq      %%mm6, 8(%3)      # Store ARGB3 ARGB2\n\t"

		     "punpckhbw %%mm2, %%mm4      #                 G7 B7 G6 B6 G5 B5 G4 B4\n\t"
		     "punpckhbw %%mm3, %%mm5      #                 00 R7 00 R6 00 R5 00 R4\n\t"
		     
		     "punpcklwd %%mm5, %%mm4      #                 00 R5 B5 G5 00 R4 B4 G4\n\t"
			 "por mmx_alpha, %%mm4        # Set alpha channel\n\t"
		     "movq      %%mm4, 16(%3)     # Store ARGB5 ARGB4\n\t"

		     "movq      %%mm0, %%mm4      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "punpckhbw %%mm2, %%mm4      #                 G7 B7 G6 B6 G5 B5 G4 B4\n\t"

		     "punpckhwd %%mm5, %%mm4      #                 00 R7 G7 B7 00 R6 B6 G6\n\t"
			 "por mmx_alpha, %%mm4        # Set alpha channel\n\t"
		     "movq      %%mm4, 24(%3)     # Store ARGB7 ARGB6\n\t"

		     "movd      4(%1), %%mm0      # Load 4 Cb       00 00 00 00 u3 u2 u1 u0\n\t"
		     "movd      4(%2), %%mm1      # Load 4 Cr       00 00 00 00 v3 v2 v1 v0\n\t"

		     "pxor      %%mm4, %%mm4      # zero mm4\n\t"
		     "movq      8(%0), %%mm6      # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"

		     : : "r" (py), "r" (pu), "r" (pv), "r" (image));

	    py      += 8;
	    pu      += 4;
	    pv      += 4;
	    image   += 32;
	    x       += 8;
	} while (x < h_size) ;

	if (even == 0) {
	    pu  -= h_size/2;
	    pv  -= h_size/2;
	} else  {
	    pu  += (uv_stride - h_size/2);
	    pv  += (uv_stride - h_size/2);
	}
	py      += (y_stride - h_size);
	image   += (image_stride - 4*h_size);

	/* load data for start of next scan line */
	__asm__ (
		 ".align 8 \n\t"
		 "movd      (%1), %%mm0       # Load 4 Cb       00 00 00 00 u3 u2 u1 u0\n\t"
		 "movd      (%2), %%mm1       # Load 4 Cr       00 00 00 00 v3 v2 v1 v0\n\t"

		 "movl      $0, (%3)          # cache preload for image\n\t"
		 "movq      (%0), %%mm6       # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"
		 : : "r" (py), "r" (pu), "r" (pv), "r" (image));


	x        = 0;
	y       += 1;
	even = (!even);
    } while ( y < v_size) ;
    __asm__ ("emms\n\t");
}

void yuv422_argb32_mmx(unsigned char * image, const unsigned char * py,
			      const unsigned char * pu, const unsigned char * pv,
			      const unsigned int h_size, const unsigned int v_size,
			      const unsigned int image_stride, const unsigned int y_stride,
			      const unsigned int uv_stride)
{
    unsigned int x = 0, y = 0;

    __asm__ (
	     ".align 8 \n\t"
	     "movd      (%1), %%mm0       # Load 4 Cb       00 00 00 00 u3 u2 u1 u0\n\t"
	     "movl      $0, (%3)          # cache preload for image\n\t"

	     "movd      (%2), %%mm1       # Load 4 Cr       00 00 00 00 v3 v2 v1 v0\n\t"
	     "pxor      %%mm4, %%mm4      # zero mm4\n\t"

	     "movq      (%0), %%mm6       # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"
	     : : "r" (py), "r" (pu), "r" (pv), "r" (image));

    do {
	do {
	    /* this mmx assembly code deals with SINGLE scan line at a time, it convert 8
	       pixels in each iteration */
	    __asm__ (
		     ".align 8 \n\t"
		     /* Do the multiply part of the conversion for even and odd pixels,
			register usage:
			mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
			mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels,
			mm6 -> Y even, mm7 -> Y odd */
		     /* convert the chroma part */
		     "punpcklbw %%mm4, %%mm0      # scatter 4 Cb    00 u3 00 u2 00 u1 00 u0\n\t"
		     "punpcklbw %%mm4, %%mm1      # scatter 4 Cr    00 v3 00 v2 00 v1 00 v0\n\t"

		     "psubsw    mmx_80w, %%mm0    # Cb -= 128\n\t"
		     "psubsw    mmx_80w, %%mm1    # Cr -= 128\n\t"

		     "psllw     $3, %%mm0         # Promote precision\n\t"
		     "psllw     $3, %%mm1         # Promote precision\n\t"

		     "movq      %%mm0, %%mm2      # Copy 4 Cb       00 u3 00 u2 00 u1 00 u0\n\t"
		     "movq      %%mm1, %%mm3      # Copy 4 Cr       00 v3 00 v2 00 v1 00 v0\n\t"

		     "pmulhw    mmx_U_green, %%mm2# Mul Cb with green coeff -> Cb green\n\t"
		     "pmulhw    mmx_V_green, %%mm3# Mul Cr with green coeff -> Cr green\n\t"

		     "pmulhw    mmx_U_blue, %%mm0 # Mul Cb -> Cblue 00 b3 00 b2 00 b1 00 b0\n\t"
		     "pmulhw    mmx_V_red, %%mm1  # Mul Cr -> Cred  00 r3 00 r2 00 r1 00 r0\n\t"

		     "paddsw    %%mm3, %%mm2      # Cb green + Cr green -> Cgreen\n\t"

		     /* convert the luma part */
		     "psubusb   mmx_10w, %%mm6    # Y -= 16\n\t"

		     "movq      %%mm6, %%mm7      # Copy 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"
		     "pand      mmx_00ffw, %%mm6  # get Y even      00 Y6 00 Y4 00 Y2 00 Y0\n\t"

		     "psrlw     $8, %%mm7         # get Y odd       00 Y7 00 Y5 00 Y3 00 Y1\n\t"

		     "psllw     $3, %%mm6         # Promote precision\n\t"
		     "psllw     $3, %%mm7         # Promote precision\n\t"

		     "pmulhw    mmx_Y_coeff, %%mm6# Mul 4 Y even    00 y6 00 y4 00 y2 00 y0\n\t"
		     "pmulhw    mmx_Y_coeff, %%mm7# Mul 4 Y odd     00 y7 00 y5 00 y3 00 y1\n\t"

		     /* Do the addition part of the conversion for even and odd pixels,
			register usage:
			mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
			mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels,
			mm6 -> Y even, mm7 -> Y odd */
		     "movq      %%mm0, %%mm3      # Copy Cblue\n\t"
		     "movq      %%mm1, %%mm4      # Copy Cred\n\t"
		     "movq      %%mm2, %%mm5      # Copy Cgreen\n\t"

		     "paddsw    %%mm6, %%mm0      # Y even + Cblue  00 B6 00 B4 00 B2 00 B0\n\t"
		     "paddsw    %%mm7, %%mm3      # Y odd  + Cblue  00 B7 00 B5 00 B3 00 B1\n\t"

		     "paddsw    %%mm6, %%mm1      # Y even + Cred   00 R6 00 R4 00 R2 00 R0\n\t"
		     "paddsw    %%mm7, %%mm4      # Y odd  + Cred   00 R7 00 R5 00 R3 00 R1\n\t"

		     "paddsw    %%mm6, %%mm2      # Y even + Cgreen 00 G6 00 G4 00 G2 00 G0\n\t"
		     "paddsw    %%mm7, %%mm5      # Y odd  + Cgreen 00 G7 00 G5 00 G3 00 G1\n\t"

		     /* Limit RGB even to 0..255 */
		     "packuswb  %%mm0, %%mm0      #                 B6 B4 B2 B0 B6 B4 B2 B0\n\t"
		     "packuswb  %%mm1, %%mm1      #                 R6 R4 R2 R0 R6 R4 R2 R0\n\t"
		     "packuswb  %%mm2, %%mm2      #                 G6 G4 G2 G0 G6 G4 G2 G0\n\t"

		     /* Limit RGB odd to 0..255 */
		     "packuswb  %%mm3, %%mm3      #                 B7 B5 B3 B1 B7 B5 B3 B1\n\t"
		     "packuswb  %%mm4, %%mm4      #                 R7 R5 R3 R1 R7 R5 R3 R1\n\t"
		     "packuswb  %%mm5, %%mm5      #                 G7 G5 G3 G1 G7 G5 G3 G1\n\t"

		     /* Interleave RGB even and odd */
		     "punpcklbw %%mm3, %%mm0      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "punpcklbw %%mm4, %%mm1      #                 R7 R6 R5 R4 R3 R2 R1 R0\n\t"
		     "punpcklbw %%mm5, %%mm2      #                 G7 G6 G5 G4 G3 G2 G1 G0\n\t"

		     /* convert RGB plane to RGB packed format, 
			mm0 -> B, mm1 -> R, mm2 -> G, mm3 -> 0,
			mm4 -> GB, mm5 -> AR pixel 4-7,
			mm6 -> GB, mm7 -> AR pixel 0-3 */
		     "pxor      %%mm3, %%mm3      # zero mm3\n\t"

		     "movq      %%mm0, %%mm6      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "movq      %%mm1, %%mm7      #                 R7 R6 R5 R4 R3 R2 R1 R0\n\t"

		     "movq      %%mm0, %%mm4      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "movq      %%mm1, %%mm5      #                 R7 R6 R5 R4 R3 R2 R1 R0\n\t"

		     "punpcklbw %%mm2, %%mm6      #                 G3 B3 G2 B2 G1 B1 G0 B0\n\t"
		     "punpcklbw %%mm3, %%mm7      #                 00 R3 00 R2 00 R1 00 R0\n\t"

		     "punpcklwd %%mm7, %%mm6      #                 00 R1 B1 G1 00 R0 B0 G0\n\t"
			 "por mmx_alpha, %%mm6        # Set alpha channel\n\t"
		     "movq      %%mm6, (%3)       # Store ARGB1 ARGB0\n\t"

		     "movq      %%mm0, %%mm6      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "punpcklbw %%mm2, %%mm6      #                 G3 B3 G2 B2 G1 B1 G0 B0\n\t"

		     "punpckhwd %%mm7, %%mm6      #                 00 R3 G3 B3 00 R2 B3 G2\n\t"
			 "por mmx_alpha, %%mm6        # Set alpha channel\n\t"
		     "movq      %%mm6, 8(%3)      # Store ARGB3 ARGB2\n\t"

		     "punpckhbw %%mm2, %%mm4      #                 G7 B7 G6 B6 G5 B5 G4 B4\n\t"
		     "punpckhbw %%mm3, %%mm5      #                 00 R7 00 R6 00 R5 00 R4\n\t"
		     
		     "punpcklwd %%mm5, %%mm4      #                 00 R5 B5 G5 00 R4 B4 G4\n\t"
			 "por mmx_alpha, %%mm4        # Set alpha channel\n\t"
		     "movq      %%mm4, 16(%3)     # Store ARGB5 ARGB4\n\t"

		     "movq      %%mm0, %%mm4      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "punpckhbw %%mm2, %%mm4      #                 G7 B7 G6 B6 G5 B5 G4 B4\n\t"

		     "punpckhwd %%mm5, %%mm4      #                 00 R7 G7 B7 00 R6 B6 G6\n\t"
			 "por mmx_alpha, %%mm4        # Set alpha channel\n\t"
		     "movq      %%mm4, 24(%3)     # Store ARGB7 ARGB6\n\t"

		     "movd      4(%1), %%mm0      # Load 4 Cb       00 00 00 00 u3 u2 u1 u0\n\t"
		     "movd      4(%2), %%mm1      # Load 4 Cr       00 00 00 00 v3 v2 v1 v0\n\t"

		     "pxor      %%mm4, %%mm4      # zero mm4\n\t"
		     "movq      8(%0), %%mm6      # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"

		     : : "r" (py), "r" (pu), "r" (pv), "r" (image));

	    py      += 8;
	    pu      += 4;
	    pv      += 4;
	    image   += 32;
	    x       += 8;
	} while (x < h_size) ;

    pu  += (uv_stride - h_size/2);
    pv  += (uv_stride - h_size/2);
	py      += (y_stride - h_size);
	image   += (image_stride - 4*h_size);

	/* load data for start of next scan line */
	__asm__ (
		 ".align 8 \n\t"
		 "movd      (%1), %%mm0       # Load 4 Cb       00 00 00 00 u3 u2 u1 u0\n\t"
		 "movd      (%2), %%mm1       # Load 4 Cr       00 00 00 00 v3 v2 v1 v0\n\t"

		 "movl      $0, (%3)          # cache preload for image\n\t"
		 "movq      (%0), %%mm6       # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"
		 : : "r" (py), "r" (pu), "r" (pv), "r" (image));


	x        = 0;
	y       += 1;
    } while ( y < v_size) ;
    __asm__ ("emms\n\t");
}

void yuv444_argb32_mmx(unsigned char * image, const unsigned char * py,
			      const unsigned char * pu, const unsigned char * pv,
			      const unsigned int h_size, const unsigned int v_size,
			      const unsigned int image_stride, const unsigned int y_stride,
			      const unsigned int uv_stride)
{
    unsigned int x = 0, y = 0;

    __asm__ (
	     ".align 8 \n\t"
	     "movq      (%1), %%mm0       # Load 8 Cb       00 00 00 00 u3 u2 u1 u0\n\t"
	     "movl      $0, (%3)          # cache preload for image\n\t"

	     "movq      (%2), %%mm1       # Load 8 Cr       00 00 00 00 v3 v2 v1 v0\n\t"
	     "pxor      %%mm4, %%mm4      # zero mm4\n\t"

	     "movq      (%0), %%mm6       # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"
	     : : "r" (py), "r" (pu), "r" (pv), "r" (image));

    do {
	do {
	    /* this mmx assembly code deals with SINGLE scan line at a time, it convert 8
	       pixels in each iteration */
	    __asm__ (
		     ".align 8 \n\t"
		     /* Do the multiply part of the conversion for even and odd pixels,
			register usage:
			mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
			mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels,
			mm6 -> Y even, mm7 -> Y odd */
		     /* convert the chroma part */


		     "movq      %%mm0, %%mm3      # Copy 4 Cb       00 u3 00 u2 00 u1 00 u0\n\t"
		     "movq      %%mm1, %%mm4      # Copy 4 Cr       00 v3 00 v2 00 v1 00 v0\n\t"

			"pand      mmx_00ffw, %%mm0 \n\t"
			"pand      mmx_00ffw, %%mm1 \n\t"

			"psrlw     $8, %%mm3 \n\t"
			"psrlw     $8, %%mm4 \n\t"

			"psubsw    mmx_80w, %%mm0 \n\t"
			"psubsw    mmx_80w, %%mm3 \n\t"
			"psubsw    mmx_80w, %%mm1 \n\t"
			"psubsw    mmx_80w, %%mm4 \n\t"

			"psllw     $3, %%mm0 \n\t"
			"psllw     $3, %%mm3 \n\t"
			"psllw     $3, %%mm1 \n\t"
			"psllw     $3, %%mm4 \n\t"

			// mul
			"pxor	%%mm2, %%mm2 \n\t"
			"pxor	%%mm5, %%mm5 \n\t"

			"paddsw      %%mm0, %%mm2 \n\t"
			"paddsw      %%mm3, %%mm5 \n\t"

			"pmulhw    mmx_U_green, %%mm2 \n\t"
			"pmulhw    mmx_U_green, %%mm5 \n\t"

			"movq      %%mm1, %%mm6 \n\t"
			"movq      %%mm4, %%mm7 \n\t"
			"pmulhw    mmx_V_green, %%mm6 \n\t"
			"pmulhw    mmx_V_green, %%mm7 \n\t"

			"paddsw    %%mm6, %%mm2 \n\t"
			"paddsw    %%mm7, %%mm5 \n\t"

			"pmulhw    mmx_U_blue, %%mm0 \n\t"
			"pmulhw    mmx_U_blue, %%mm3 \n\t"

			"pmulhw    mmx_V_red, %%mm1 \n\t"
			"pmulhw    mmx_V_red, %%mm4 \n\t"

		     /* convert the luma part */
			 "movq      (%0), %%mm6       # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"
		     "psubusb   mmx_10w, %%mm6    # Y -= 16\n\t"

		     "movq      %%mm6, %%mm7      # Copy 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"
		     "pand      mmx_00ffw, %%mm6  # get Y even      00 Y6 00 Y4 00 Y2 00 Y0\n\t"

		     "psrlw     $8, %%mm7         # get Y odd       00 Y7 00 Y5 00 Y3 00 Y1\n\t"

		     "psllw     $3, %%mm6         # Promote precision\n\t"
		     "psllw     $3, %%mm7         # Promote precision\n\t"

		     "pmulhw    mmx_Y_coeff, %%mm6# Mul 4 Y even    00 y6 00 y4 00 y2 00 y0\n\t"
		     "pmulhw    mmx_Y_coeff, %%mm7# Mul 4 Y odd     00 y7 00 y5 00 y3 00 y1\n\t"

		     /* Do the addition part of the conversion for even and odd pixels,
			register usage:
			mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
			mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels,
			mm6 -> Y even, mm7 -> Y odd */
		     "movq      %%mm0, %%mm3      # Copy Cblue\n\t"
		     "movq      %%mm1, %%mm4      # Copy Cred\n\t"
		     "movq      %%mm2, %%mm5      # Copy Cgreen\n\t"

		     "paddsw    %%mm6, %%mm0      # Y even + Cblue  00 B6 00 B4 00 B2 00 B0\n\t"
		     "paddsw    %%mm7, %%mm3      # Y odd  + Cblue  00 B7 00 B5 00 B3 00 B1\n\t"

		     "paddsw    %%mm6, %%mm1      # Y even + Cred   00 R6 00 R4 00 R2 00 R0\n\t"
		     "paddsw    %%mm7, %%mm4      # Y odd  + Cred   00 R7 00 R5 00 R3 00 R1\n\t"

		     "paddsw    %%mm6, %%mm2      # Y even + Cgreen 00 G6 00 G4 00 G2 00 G0\n\t"
		     "paddsw    %%mm7, %%mm5      # Y odd  + Cgreen 00 G7 00 G5 00 G3 00 G1\n\t"

		     /* Limit RGB even to 0..255 */
		     "packuswb  %%mm0, %%mm0      #                 B6 B4 B2 B0 B6 B4 B2 B0\n\t"
		     "packuswb  %%mm1, %%mm1      #                 R6 R4 R2 R0 R6 R4 R2 R0\n\t"
		     "packuswb  %%mm2, %%mm2      #                 G6 G4 G2 G0 G6 G4 G2 G0\n\t"

		     /* Limit RGB odd to 0..255 */
		     "packuswb  %%mm3, %%mm3      #                 B7 B5 B3 B1 B7 B5 B3 B1\n\t"
		     "packuswb  %%mm4, %%mm4      #                 R7 R5 R3 R1 R7 R5 R3 R1\n\t"
		     "packuswb  %%mm5, %%mm5      #                 G7 G5 G3 G1 G7 G5 G3 G1\n\t"

		     /* Interleave RGB even and odd */
		     "punpcklbw %%mm3, %%mm0      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "punpcklbw %%mm4, %%mm1      #                 R7 R6 R5 R4 R3 R2 R1 R0\n\t"
		     "punpcklbw %%mm5, %%mm2      #                 G7 G6 G5 G4 G3 G2 G1 G0\n\t"

		     /* convert RGB plane to RGB packed format, 
			mm0 -> B, mm1 -> R, mm2 -> G, mm3 -> 0,
			mm4 -> GB, mm5 -> AR pixel 4-7,
			mm6 -> GB, mm7 -> AR pixel 0-3 */
		     "pxor      %%mm3, %%mm3      # zero mm3\n\t"

		     "movq      %%mm0, %%mm6      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "movq      %%mm1, %%mm7      #                 R7 R6 R5 R4 R3 R2 R1 R0\n\t"

		     "movq      %%mm0, %%mm4      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "movq      %%mm1, %%mm5      #                 R7 R6 R5 R4 R3 R2 R1 R0\n\t"

		     "punpcklbw %%mm2, %%mm6      #                 G3 B3 G2 B2 G1 B1 G0 B0\n\t"
		     "punpcklbw %%mm3, %%mm7      #                 00 R3 00 R2 00 R1 00 R0\n\t"

		     "punpcklwd %%mm7, %%mm6      #                 00 R1 B1 G1 00 R0 B0 G0\n\t"
			 "por mmx_alpha, %%mm6        # Set alpha channel\n\t"
		     "movq      %%mm6, (%3)       # Store ARGB1 ARGB0\n\t"

		     "movq      %%mm0, %%mm6      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "punpcklbw %%mm2, %%mm6      #                 G3 B3 G2 B2 G1 B1 G0 B0\n\t"

		     "punpckhwd %%mm7, %%mm6      #                 00 R3 G3 B3 00 R2 B3 G2\n\t"
			 "por mmx_alpha, %%mm6        # Set alpha channel\n\t"
		     "movq      %%mm6, 8(%3)      # Store ARGB3 ARGB2\n\t"

		     "punpckhbw %%mm2, %%mm4      #                 G7 B7 G6 B6 G5 B5 G4 B4\n\t"
		     "punpckhbw %%mm3, %%mm5      #                 00 R7 00 R6 00 R5 00 R4\n\t"
		     
		     "punpcklwd %%mm5, %%mm4      #                 00 R5 B5 G5 00 R4 B4 G4\n\t"
			 "por mmx_alpha, %%mm4        # Set alpha channel\n\t"
		     "movq      %%mm4, 16(%3)     # Store ARGB5 ARGB4\n\t"

		     "movq      %%mm0, %%mm4      #                 B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "punpckhbw %%mm2, %%mm4      #                 G7 B7 G6 B6 G5 B5 G4 B4\n\t"

		     "punpckhwd %%mm5, %%mm4      #                 00 R7 G7 B7 00 R6 B6 G6\n\t"
			 "por mmx_alpha, %%mm4        # Set alpha channel\n\t"
		     "movq      %%mm4, 24(%3)     # Store ARGB7 ARGB6\n\t"

		     "movq      8(%1), %%mm0      # Load 4 Cb       00 00 00 00 u3 u2 u1 u0\n\t"
		     "movq      8(%2), %%mm1      # Load 4 Cr       00 00 00 00 v3 v2 v1 v0\n\t"

		     "pxor      %%mm4, %%mm4      # zero mm4\n\t"
		     "movq      8(%0), %%mm6      # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"

		     : : "r" (py), "r" (pu), "r" (pv), "r" (image));

	    py      += 8;
	    pu      += 8;
	    pv      += 8;
	    image   += 32;
	    x       += 8;
	} while (x < h_size) ;

    pu  += (uv_stride - h_size);
    pv  += (uv_stride - h_size);
	py      += (y_stride - h_size);
	image   += (image_stride - 4*h_size);

	/* load data for start of next scan line */
	__asm__ (
		 ".align 8 \n\t"
		 "movq      (%1), %%mm0       # Load 4 Cb       00 00 00 00 u3 u2 u1 u0\n\t"
		 "movq      (%2), %%mm1       # Load 4 Cr       00 00 00 00 v3 v2 v1 v0\n\t"

		 "movl      $0, (%3)          # cache preload for image\n\t"
		 "movq      (%0), %%mm6       # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"
		 : : "r" (py), "r" (pu), "r" (pv), "r" (image));


	x        = 0;
	y       += 1;
    } while ( y < v_size) ;
    __asm__ ("emms\n\t");
}

#endif
