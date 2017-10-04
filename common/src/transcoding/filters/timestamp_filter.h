#pragma once

#include <QtCore/qnamespace.h>
#include <transcoding/filters/abstract_image_filter.h>


namespace nx {

namespace core {
namespace transcoding {

struct TimestampOverlaySettings;

} // namespace transcoding
} // namespace core

namespace transcoding {
namespace filters {

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

} // namespace filters
} // namespace transcoding
} // namespace nx
