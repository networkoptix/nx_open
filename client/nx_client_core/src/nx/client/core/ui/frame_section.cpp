#include "frame_section.h"

#include <QtQml/QtQml>

namespace nx {
namespace client {
namespace core {

static QObject* createFrameSection(QQmlEngine*, QJSEngine*)
{
    return new FrameSection();
}

FrameSection::Section FrameSection::frameSection(
    const QPointF& point,
    const QRectF& rect,
    qreal frameWidth)
{
    SectionFlags result = NoSection;

    if (point.y() >= rect.top() - frameWidth && point.y() <= rect.bottom() + frameWidth)
    {
        if (point.x() >= rect.left() - frameWidth && point.x() <= rect.left() + frameWidth)
            result |= Left;
        if (point.x() >= rect.right() - frameWidth && point.x() <= rect.right() + frameWidth)
            result |= Right;
    }
    if (point.x() >= rect.left() - frameWidth && point.x() <= rect.right() + frameWidth)
    {
        if (point.y() >= rect.top() - frameWidth && point.y() <= rect.top() + frameWidth)
            result |= Top;
        if (point.y() >= rect.bottom() - frameWidth && point.y() <= rect.bottom() + frameWidth)
            result |= Bottom;
    }

    return static_cast<Section>(static_cast<int>(result));
}

Qt::WindowFrameSection FrameSection::toQtWindowFrameSection(FrameSection::Section section)
{
    switch (section)
    {
        case Left:
            return Qt::LeftSection;
        case Right:
            return Qt::RightSection;
        case Top:
            return Qt::TopSection;
        case Bottom:
            return Qt::BottomSection;
        case TopLeft:
            return Qt::TopLeftSection;
        case TopRight:
            return Qt::TopRightSection;
        case BottomLeft:
            return Qt::BottomLeftSection;
        case BottomRight:
            return Qt::BottomRightSection;
        default:
            return Qt::NoSection;
    }
}

void FrameSection::registedQmlType()
{
    qmlRegisterSingletonType<FrameSection>(
        "nx.client.core", 1, 0, "FrameSection", &createFrameSection);
}

} // namespace core
} // namespace client
} // namespace nx
