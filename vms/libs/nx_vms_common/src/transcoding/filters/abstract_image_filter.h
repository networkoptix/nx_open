// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QSharedPointer>

#include <QtCore/QRectF>

class CLVideoDecoderOutput;
using CLVideoDecoderOutputPtr = QSharedPointer<CLVideoDecoderOutput>;

struct QnAbstractCompressedMetadata;
using QnAbstractCompressedMetadataPtr = std::shared_ptr<QnAbstractCompressedMetadata>;

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

    // TODO: #sivanov Apply to filters.
    virtual ColorSpace colorSpace() const { return ColorSpace::yuv; }

    /**
     * Update video image.
     *
     * @param frame Frame image to update.
     * @param metadata Frame metadata.
     */
    virtual CLVideoDecoderOutputPtr updateImage(
        const CLVideoDecoderOutputPtr& frame,
        const QnAbstractCompressedMetadataPtr& metadata) = 0;

    /**
     * Update video size.
     *
     * @param srcSize input image size. Function should return output image size
     */
    virtual QSize updatedResolution(const QSize& srcSize) = 0;
};

using QnAbstractImageFilterPtr = QSharedPointer<QnAbstractImageFilter>;
using QnAbstractImageFilterList = QList<QnAbstractImageFilterPtr>;
