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
     */
    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) = 0;

    /**
     * Update video size.
     * 
     * \param srcSize input image size. Function should return output image size
     */
    virtual QSize updatedResolution(const QSize& srcSize) = 0;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __ABSTRACT_IMAGE_FILTER_H__
