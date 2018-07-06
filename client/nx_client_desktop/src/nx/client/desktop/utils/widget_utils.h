#pragma once

class QWidget;
class QGraphicsWidget;

namespace nx {
namespace client {
namespace desktop {

class WidgetUtils
{
public:
    static void removeLayout(QLayout* layout);

    /** Unlike QWidget::graphicsProxyWidget finds proxy recursively. */
    static QGraphicsProxyWidget* graphicsProxyWidget(const QWidget* widget);
    static const QWidget* graphicsProxiedWidget(const QWidget* widget);

    /** Workaround while Qt's QWidget::mapFromGlobal is broken. */
    static QPoint mapFromGlobal(const QGraphicsWidget* to, const QPoint& globalPos);
    static QPoint mapFromGlobal(const QWidget* to, const QPoint& globalPos);
};

} // namespace desktop
} // namespace client
} // namespace nx
