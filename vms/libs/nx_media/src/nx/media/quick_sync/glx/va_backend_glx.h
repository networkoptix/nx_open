/*
 * Copyright (C) 2009 Splitted-Desktop Systems. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef VA_BACKEND_GLX_H
#define VA_BACKEND_GLX_H

struct VADriverContext;

struct VADriverVTableGLX {
    /* Optional: create a surface used for display to OpenGL */
    VAStatus (*vaCreateSurfaceGLX)(
        struct VADriverContext *ctx,
        unsigned int            gl_target,
        unsigned int            gl_texture,
        int                     src_width,
        int                     src_height,
        void                  **gl_surface
    );

    /* Optional: destroy a VA/GLX surface */
    VAStatus (*vaDestroySurfaceGLX)(
        struct VADriverContext *ctx,
        void                   *gl_surface
    );

    /* Optional: copy a VA surface to a VA/GLX surface */
    VAStatus (*vaCopySurfaceGLX)(
        struct VADriverContext *ctx,
        void                   *gl_surface,
        VASurfaceID             surface,
        unsigned int            flags
    );
};

#endif /* VA_BACKEND_GLX_H */
