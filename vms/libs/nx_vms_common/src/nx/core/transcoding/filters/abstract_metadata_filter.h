// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QSharedPointer>

#include <QtGui/QImage>

#include <transcoding/filters/abstract_image_filter.h>

class CLVideoDecoderOutput;
using CLVideoDecoderOutputPtr = QSharedPointer<CLVideoDecoderOutput>;

struct QnAbstractCompressedMetadata;
using QnAbstractCompressedMetadataPtr = std::shared_ptr<QnAbstractCompressedMetadata>;

/**
 * Base class for additional effects during video transcoding
 */
class AbstractMetadataFilter: public QnAbstractImageFilter
{
public:
    ~AbstractMetadataFilter() override = default;

    using QnAbstractImageFilter::updateImage;

    /**
     * Frame converted to image to be updated provided here and then QPainter used to draw.
     *
     * @param image input image to be used as span for metadata adding.
     */
    virtual void updateImage(QImage& image) = 0;

    /**
     * Store relevant metadata inside filter and use it to draw found objects, motions, etc.
     *
     * @param metadata list of metadata that is applicable for the current frame.
     */
    virtual void setMetadata(const std::list<QnConstAbstractCompressedMetadataPtr>& metadata) = 0;
};

using AbstractMetadataFilterPtr = QSharedPointer<AbstractMetadataFilter>;
