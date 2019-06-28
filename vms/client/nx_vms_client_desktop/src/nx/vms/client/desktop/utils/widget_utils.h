#pragma once

#include <QtCore/QPoint>

class QMenu;
class QWidget;
class QLayout;
class QGraphicsWidget;
class QGraphicsProxyWidget;

namespace nx::vms::client::desktop {

class WidgetUtils
{
public:
    static void removeLayout(QLayout* layout);
    static void setFlag(QWidget* widget, Qt::WindowFlags flags, bool value);

    /** Clears specified menu and, unline QMenu::clear, deletes all owned submenus. */
    static void clearMenu(QMenu* menu);

    /** Unlike QWidget::graphicsProxyWidget finds proxy recursively. */
    static QGraphicsProxyWidget* graphicsProxyWidget(const QWidget* widget);
    static const QWidget* graphicsProxiedWidget(const QWidget* widget);

    /** Workaround while Qt's QWidget::mapFromGlobal is broken. */
    static QPoint mapFromGlobal(const QGraphicsWidget* to, const QPoint& globalPos);
    static QPoint mapFromGlobal(const QWidget* to, const QPoint& globalPos);
};

} // namespace nx::vms::client::desktop
