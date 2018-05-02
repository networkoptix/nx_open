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

bool FrameSection::isEdge(FrameSection::Section section)
{
    switch (section)
    {
        case Left:
        case Right:
        case Top:
        case Bottom:
            return true;
        default:
            return false;
    }
}

bool FrameSection::isCorner(FrameSection::Section section)
{
    return section != NoSection && !isEdge(section);
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

QPointF FrameSection::sectionCenterPoint(const QRectF& rect, FrameSection::Section section)
{
    switch (section)
    {
        case Left:
            return QPointF(rect.left(), rect.top() + rect.height() / 2);
        case TopLeft:
            return rect.topLeft();
        case Top:
            return QPointF(rect.left() + rect.width() / 2, rect.top());
        case TopRight:
            return rect.topRight();
        case Right:
            return QPointF(rect.right(), rect.top() + rect.height() / 2);
        case BottomRight:
            return rect.bottomRight();
        case Bottom:
            return QPointF(rect.left() + rect.width() / 2, rect.bottom());
        case BottomLeft:
            return rect.bottomLeft();
        default:
            return QPointF();
    }
}

FrameSection::Section FrameSection::oppositeSection(FrameSection::Section section)
{
    switch (section)
    {
        case Left:
            return Right;
        case TopLeft:
            return BottomRight;
        case Top:
            return Bottom;
        case TopRight:
            return BottomLeft;
        case Right:
            return Left;
        case BottomRight:
            return TopLeft;
        case Bottom:
            return Top;
        case BottomLeft:
            return TopRight;
        default:
            return NoSection;
    }
}

QSizeF FrameSection::sizeDelta(const QPointF& dragDelta, FrameSection::Section section)
{
    qreal dx = 0;
    qreal dy = 0;

    const auto& flags = SectionFlags(section);

    if (flags.testFlag(Left))
        dx = -dragDelta.x();
    else if (flags.testFlag(Right))
        dx = dragDelta.x();

    if (flags.testFlag(Top))
        dy = -dragDelta.y();
    else if (flags.testFlag(Bottom))
        dy = dragDelta.y();

    return QSizeF(dx, dy);
}

void FrameSection::registedQmlType()
{
    qmlRegisterSingletonType<FrameSection>(
        "nx.client.core", 1, 0, "FrameSection", &createFrameSection);
}

} // namespace core
} // namespace client
} // namespace nx
