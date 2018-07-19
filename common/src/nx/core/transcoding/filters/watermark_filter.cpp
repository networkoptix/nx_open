#include "watermark_filter.h"

#if defined(ENABLE_DATA_PROVIDERS)

#include "watermark_images.h"
#include "transcoding_settings.h"

namespace nx {
namespace core {
namespace transcoding {

WatermarkImageFilter::WatermarkImageFilter(const WatermarkOverlaySettings& settings):
    m_settings(new QnWatermarkSettings(settings.settings)),
    m_text(settings.username)
{
}

QSize WatermarkImageFilter::updatedResolution(const QSize& sourceSize)
{
    PaintImageFilter::updatedResolution(sourceSize);
    setImage(getWatermarkImage(*m_settings, m_text, sourceSize).toImage());
    return sourceSize;
}

} // namespace transcoding
} // namespace core
} // namespace nx

#endif // ENABLE_DATA_PROVIDERS
