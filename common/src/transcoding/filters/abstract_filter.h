#ifndef __ABSTRACT_IMAGE_FILTER_H__
#define __ABSTRACT_IMAGE_FILTER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include "utils/media/frame_info.h"

/**
 * Base class for addition effects during video transcoding
 */

// todo: simplify ffmpegVideoTranscoder and perform crop scale operations as filters

class QnAbstractImageFilter
{
public:
    /**
     * Update video image.
     * 
     * \param frame image to update
     * \param updateRect    image rect to update. Filter MUST not update image outside the rect. Rect in range [0..1]
     */
    virtual void updateImage(CLVideoDecoderOutput* frame, const QRectF& updateRect) = 0;

    virtual ~QnAbstractImageFilter() {}
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __ABSTRACT_IMAGE_FILTER_H__
