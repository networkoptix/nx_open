#include "timestamp_filter.h"

#include <QtCore/QDateTime>
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>

#include <utils/common/util.h>
#include <utils/media/frame_info.h>
#include <transcoding/filters/image_to_frame_painter.h>
#include <nx/core/transcoding/filters/transcoding_settings.h>

namespace {

QFont fontFromParams(const nx::core::transcoding::TimestampOverlaySettings& params)
{
    QFont result;
    result.setPixelSize(params.fontSize);
    result.setBold(true);
    return result;
}

} // namespace

namespace nx {
namespace transcoding {
namespace filters {

class TimestampFilter::Internal
{
public:
    Internal(const core::transcoding::TimestampOverlaySettings& params);

    void updateTimestamp(const CLVideoDecoderOutputPtr& frame);
    detail::ImageToFramePainter& painter();

private:

private:
    const QFont m_font;
    const QFontMetrics m_fontMetrics;

    core::transcoding::TimestampOverlaySettings m_params;
    detail::ImageToFramePainter m_painter;
    qint64 m_currentTimeMs = -1;
};

TimestampFilter::Internal::Internal(const core::transcoding::TimestampOverlaySettings& params):
    m_font(fontFromParams(params)),
    m_fontMetrics(m_font),
    m_params(params)
{
}

void TimestampFilter::Internal::updateTimestamp(const CLVideoDecoderOutputPtr& frame)
{
    const qint64 displayTime = frame->pts / 1000 + m_params.serverTimeDisplayOffsetMs;
    if (m_currentTimeMs == displayTime)
        return;

    m_currentTimeMs = displayTime;

    const auto timeString =
        QDateTime::fromMSecsSinceEpoch(m_currentTimeMs).toString(m_params.format);

    const auto textSize = QSize(m_fontMetrics.width(timeString) + 30, m_fontMetrics.height() + 30);
    QImage image(textSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing
        | QPainter::TextAntialiasing | QPainter::HighQualityAntialiasing);

    QPainterPath path;
    path.addText(0, m_fontMetrics.ascent(), m_font, timeString);
    painter.setBrush(m_params.foreground);
    painter.drawPath(path);
    painter.strokePath(path, m_params.outline);

    m_painter.setImage(image, m_params.offset, m_params.alignment);
}

detail::ImageToFramePainter& TimestampFilter::Internal::painter()
{
    return m_painter;
}

///

TimestampFilter::TimestampFilter(const core::transcoding::TimestampOverlaySettings& params):
    d(new Internal(params))
{
}

TimestampFilter::~TimestampFilter()
{
}

CLVideoDecoderOutputPtr TimestampFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    d->updateTimestamp(frame);
    d->painter().drawTo(frame);
    return frame;
}

QSize TimestampFilter::updatedResolution(const QSize& sourceSize)
{
    d->painter().updateSourceSize(sourceSize);
    return sourceSize;
}

} // namespace filters
} // namespace transcoding
} // namespace nx

