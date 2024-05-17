// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <transcoding/filters/abstract_image_filter.h>

class QnResourceVideoLayout;

namespace nx::core::transcoding {

class TextImageFilter: public QnAbstractImageFilter
{
public:
    using VideoLayoutPtr = std::shared_ptr<const QnResourceVideoLayout>;
    using TextGetter =
        std::function<QString (const CLVideoDecoderOutputPtr& frame, int cutSymbolsCount)>;

    using Factor = QPointF;

    static QnAbstractImageFilterPtr create(
        const VideoLayoutPtr& videoLayout,
        const Qt::Corner corner,
        const TextGetter& textGetter,
        const Factor& factor = Factor(1, 1));

    TextImageFilter(
        const VideoLayoutPtr& videoLayout,
        const Qt::Corner corner,
        const TextGetter& textGetter,
        const Factor& factor = Factor(1, 1));

    virtual ~TextImageFilter();

    virtual CLVideoDecoderOutputPtr updateImage(
        const CLVideoDecoderOutputPtr& frame,
        const QnAbstractCompressedMetadataPtr& metadata) override;

    virtual QSize updatedResolution(const QSize& srcSize) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::core::transcoding
