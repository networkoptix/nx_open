#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <QtCore/QScopedPointer>

#include "paint_image_filter.h"

struct QnWatermarkSettings;

namespace nx {
namespace core {
namespace transcoding {

struct WatermarkOverlaySettings;

class WatermarkImageFilter: public PaintImageFilter
{
public:
    WatermarkImageFilter(const WatermarkOverlaySettings& settings);

    virtual QSize updatedResolution(const QSize& sourceSize) override;

private:
    QScopedPointer<QnWatermarkSettings> m_settings;
    QString m_text;
};

} // namespace transcoding
} // namespace core
} // namespace nx

#endif // ENABLE_DATA_PROVIDERS
