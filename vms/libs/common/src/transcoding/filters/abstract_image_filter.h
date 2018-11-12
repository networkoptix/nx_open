#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QRectF>
#include <QSharedPointer>

class CLVideoDecoderOutput;
using CLVideoDecoderOutputPtr = QSharedPointer<CLVideoDecoderOutput>;

// todo: simplify ffmpegVideoTranscoder and perform crop scale operations as filters

/**
 * Base class for addition effects during video transcoding
 */
class QnAbstractImageFilter
{
public:
    enum class ColorSpace
    {
        noImage,
        rgb,
        yuv
    };

    virtual ~QnAbstractImageFilter() {}

    // TODO: #GDM apply to filters.
    virtual ColorSpace colorSpace() const { return ColorSpace::yuv; }

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

using QnAbstractImageFilterPtr = QSharedPointer<QnAbstractImageFilter>;
using QnAbstractImageFilterList = QList<QnAbstractImageFilterPtr>;

#endif // ENABLE_DATA_PROVIDERS
