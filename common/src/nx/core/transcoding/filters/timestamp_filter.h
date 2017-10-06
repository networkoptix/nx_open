#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <QtCore/qnamespace.h>
#include <transcoding/filters/abstract_image_filter.h>

namespace nx {
namespace core {
namespace transcoding {

struct TimestampOverlaySettings;

class TimestampFilter: public QnAbstractImageFilter
{
public:
    TimestampFilter(const core::transcoding::TimestampOverlaySettings& params);
    virtual ~TimestampFilter();

    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;
    virtual QSize updatedResolution(const QSize& sourceSize) override;

    static QString timestampTextUtc(
        qint64 sinceEpochMs,
        int displayOffsetMs,
        Qt::DateFormat format);

    static QString timestampTextSimple(qint64 timeOffsetMs);

private:
    class Internal;
    const QScopedPointer<Internal> d;
};

} // namespace transcoding
} // namespace core
} // namespace nx

#endif // ENABLE_DATA_PROVIDERS
