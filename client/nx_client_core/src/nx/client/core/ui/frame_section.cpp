#include "frame_section.h"

#include <QtQml/QtQml>

namespace nx {
namespace client {
namespace core {
namespace ui {

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

    if (point.x() >= rect.left() && point.x() <= rect.left() + frameWidth)
        result |= Left;
    if (point.x() >= rect.right() - frameWidth && point.x() <= rect.right())
        result |= Right;
    if (point.y() >= rect.top() && point.y() <= rect.top() + frameWidth)
        result |= Top;
    if (point.y() >= rect.bottom() - frameWidth && point.y() <= rect.bottom())
        result |= Bottom;

    return static_cast<Section>(static_cast<int>(result));
}

void FrameSection::registedQmlType()
{
    qmlRegisterSingletonType<nx::client::core::ui::FrameSection>(
        "Nx.Client.Core.Ui", 1, 0, "FrameSection", &createFrameSection);
}

} // namespace ui
} // namespace core
} // namespace client
} // namespace nx
