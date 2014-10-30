#ifndef __ABSTRACT_IMAGE_FILTER_H__
#define __ABSTRACT_IMAGE_FILTER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QRectF>

class CLVideoDecoderOutput;
typedef QSharedPointer<CLVideoDecoderOutput> CLVideoDecoderOutputPtr;

// todo: simplify ffmpegVideoTranscoder and perform crop scale operations as filters

/**
 * Base class for addition effects during video transcoding
 */
class QnAbstractImageFilter
{
public:
    virtual ~QnAbstractImageFilter() {}

    /**
     * Update video image.
     * 
     * \param frame image to update
     * \param updateRect    image rect to update. Filter MUST not update image outside the rect. Rect in range [0..1]
     * \param ar    image aspect ratio.
     */
    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame, const QRectF& updateRect, qreal ar) = 0;
    virtual QSize updatedResolution(const QSize& srcSize) { return srcSize; }
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __ABSTRACT_IMAGE_FILTER_H__
