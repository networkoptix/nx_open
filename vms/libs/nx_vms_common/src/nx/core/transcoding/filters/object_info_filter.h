// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <transcoding/filters/abstract_image_filter.h>

#include "transcoding_settings.h"

class QTextDocument;
namespace nx::core::transcoding {
namespace detail { class ImageToFramePainter; } // namespace detail

class NX_VMS_COMMON_API ObjectInfoFilter: public QnAbstractImageFilter
{
public:
    ObjectInfoFilter(const ObjectExportSettings& settings);
    virtual ~ObjectInfoFilter() override;

    CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;

    QSize updatedResolution(const QSize& sourceSize) override;

    void setMetadata(const std::list<QnConstAbstractCompressedMetadataPtr>& metadata);

private:
    QColor getFrameColor(const common::metadata::ObjectMetadata& objectMetadata) const;
    std::shared_ptr<QTextDocument> buildDescription(
        const common::metadata::ObjectMetadata& objectMetadata,
        int height);
    void drawFrame(const QRect& box, const QColor& frameColor, int lineWidth, QPainter& painter);
    void drawDescription(
        const QRect& frameRect,
        const QColor& frameColor,
        QPainter& painter,
        const common::metadata::ObjectMetadata& objectMetadata,
        int width,
        int height);
    ObjectExportSettings m_settings;
    std::list<QnConstAbstractCompressedMetadataPtr> m_metadata;
    std::map<Uuid, std::shared_ptr<QTextDocument>> m_descriptions;
};

} // namespace nx::core::transcoding
