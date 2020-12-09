#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <functional>

#include <nx/utils/impl_ptr.h>
#include <transcoding/filters/abstract_image_filter.h>

class QnResourceVideoLayout;

namespace nx::vms::common::transcoding {

class TextImageFilter: public QnAbstractImageFilter
{
public:
    using VideoLayoutPtr = QSharedPointer<const QnResourceVideoLayout>;
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

    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;
    virtual QSize updatedResolution(const QSize& srcSize) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::common::transcoding

#endif // ENABLE_DATA_PROVIDERS
