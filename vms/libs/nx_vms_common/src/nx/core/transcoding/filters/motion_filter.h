// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/meta_data_packet.h>

#include "abstract_metadata_filter.h"
#include "transcoding_settings.h"

namespace nx::core::transcoding {

class NX_VMS_COMMON_API MotionFilter: public AbstractMetadataFilter
{
public:
    MotionFilter(const MotionExportSettings& settings);
    virtual ~MotionFilter() override;

    CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;
    void updateImage(QImage& image) override;

    QSize updatedResolution(const QSize& sourceSize) override;

    void setMetadata(const std::list<QnConstAbstractCompressedMetadataPtr>& metadata) override;

private:
    void paintMotionGrid(
        QPainter* painter,
        const QRectF& rect,
        const QnConstMetaDataV1Ptr& motion);

    MotionExportSettings m_settings;
    std::list<QnConstAbstractCompressedMetadataPtr> m_metadata;
};

} // namespace nx::core::transcoding
