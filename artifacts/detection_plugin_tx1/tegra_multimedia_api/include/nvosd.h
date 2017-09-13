/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *  3. The names of its contributors may not be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/**
* @file nvosd.h
* @brief NVOSD library to be used to draw rectangles and text over the frame
* for given parameters.
*/

#ifndef __NVOSD_DEFS__
#define __NVOSD_DEFS__

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Holds the color parameters of the box or text to be overlayed.
 */
typedef struct _NvOSD_ColorParams {
    double red;                 /**< Holds red component of color.
                                     Value should be in the range 0-1. */

    double green;               /**< Holds green component of color.
                                     Value should be in the range 0-1.*/

    double blue;                /**< Holds blue component of color.
                                     Value should be in the range 0-1.*/

    double alpha;               /**< Holds alpha component of color.
                                     Value should be in the range 0-1.*/
}NvOSD_ColorParams;


/**
 * Holds the font parameters of the text to be overlayed.
 */
typedef struct _NvOSD_FontParams {
    char * font_name;            /**< Holds pointer to the string containing
                                      font name. */

    unsigned int font_size;         /**< Holds size of the font. */

    NvOSD_ColorParams font_color;   /**< Holds font color. */
}NvOSD_FontParams;


/**
 * Holds the text parameters of the text to be overlayed.
 */

typedef struct _NvOSD_TextParams {
    char * display_text; /**< Holds the text to be overlayed. */

    unsigned int x_offset; /**< Holds horizontal offset w.r.t top left pixel of
                             the frame. */
    unsigned int y_offset; /**< Holds vertical offset w.r.t top left pixel of
                             the frame. */

    NvOSD_FontParams font_params;/**< font_params. */
}NvOSD_TextParams;


/**
 * Holds the box parameters of the box to be overlayed.
 */
typedef struct _NvOSD_RectParams {
    unsigned int left;   /**< Holds left coordinate of the box in pixels. */

    unsigned int top;    /**< Holds top coordinate of the box in pixels. */

    unsigned int width;  /**< Holds width of the box in pixels. */

    unsigned int height; /**< Holds height of the box in pixels. */

    unsigned int border_width; /**< Holds border_width of the box in pixels. */

    NvOSD_ColorParams border_color; /**< Holds color params of the border
                                      of the box. */

    unsigned int has_bg_color;  /**< Holds boolean value indicating whether box
                                    has background color. */

    unsigned int reserved; /**< reserved field for future usage.
                             For internal purpose only */

    NvOSD_ColorParams bg_color; /**< Holds background color of the box. */
}NvOSD_RectParams;

/**
 * List modes used to overlay boxes and text
 */
typedef enum{
    MODE_CPU, /**< Selects CPU for OSD processing.
                Works with RGBA data only */
    MODE_GPU, /**< Selects GPU for OSD processing.
                Yet to be implemented */
    MODE_HW   /**< Selects NV HW engine for rectangle draw and mask.
                   This mode works with YUV and RGB data both.
                   It does not consider alpha parameter.
                   Not applicable for drawing text. */
} NvOSD_Mode;

/**
 * Create nvosd context
 *
 * @param[in] mode Mode in which nvosd should process the data,
 *            one of NvOSD_Mode
 * @returns A pointer to NvOSD context, NULL in case of failure.
 */
void *nvosd_create_context(void);

/**
 * Destroy nvosd context
 *
 * @param[in] nvosd_ctx A pointer to NvOSD context.
 */

void nvosd_destroy_context(void *nvosd_ctx);

/**
 * Set clock parameters for the given context.
 *
 * The clock is overlayed when nvosd_put_text() is called.
 * If no other text is to be overlayed nvosd_put_text should be called with
 * @a num_strings as 0 and @a text_params_list as NULL.
 *
 * @param[in] nvosd_ctx A pointer to NvOSD context
 * @param[in] clk_params A pointer to NvOSD_TextParams structure for the clock
 *            to be overlayed, NULL to disable the clock.
 */
void nvosd_set_clock_params(void *nvosd_ctx, NvOSD_TextParams *clk_params);


/**
 * Overlay clock and given text at given location on a buffer.
 *
 * To overlay the clock user needs to set clock params using
 * nvosd_set_clock_params().
 * User should ensure that the length of @a text_params_list should be at least
 * @a num_strings.
 *
 * @note Currently only #MODE_CPU is supported. Specifying other modes wil have
 * no effect.
 *
 * @param[in] nvosd_ctx A pointer to NvOSD context.
 * @param[in] mode Mode selection to draw the text.
 * @param[in] fd DMABUF FD of buffer on which text is to be overlayed.
 * @param[in] num_strings Number of strings to be overlayed.
 * @param[in] text_params_list A pointer to an array of NvOSD_TextParams
 *            structure for the clock and text to be overlayed.
 *
 * @returns 0 for success, -1 for failure.
 */
int nvosd_put_text(void *nvosd_ctx, NvOSD_Mode mode, int fd, int num_strings,
        NvOSD_TextParams *text_params_list);


/**
 * Overlay boxes at given location on a buffer.
 *
 * Boxes can be configured with
 * a. only border
 *    To draw boxes with only border user needs to set @a border_width and set
 *    @a has_bg_color to 0 for the given box.
 * b. border and background color
 *    To draw boxes with border and background color user needs to set @a
 *    border_width and set @a has_bg_color to 1 and specify background color
 *    parameters for the given box.
 * c. Solid fill acting as mask region.
 *    To draw boxes with Solid fill acting as mask region user needs to set @a
 *    border_width to 0. @a has_bg_color to 1 for the given box.
 *
 *
 * User should ensure that the length of @a rect_params_list should be at least
 * @a num_rects.
 *
 * @param[in] nvosd_ctx A pointer to NvOSD context.
 * @param[in] mode Mode selection to draw the boxes.
 * @param[in] fd DMABUF FD of buffer on which boxes are to be overlayed.
 * @param[in] num_rects Number of boxes to be overlayed.
 * @param[in] rect_params_list A pointer to an array of NvOSD_TextParams
 *            structure for the clock and text to be overlayed.
 *
 * @returns 0 for success, -1 for failure.
 */
int nvosd_draw_rectangles(void *nvosd_ctx, NvOSD_Mode mode, int fd,
        int num_rects, NvOSD_RectParams *rect_params_list);

#ifdef __cplusplus
}
#endif

#endif
