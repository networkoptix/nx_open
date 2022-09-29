// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPoint>
#include <QtCore/QString>

class QMenu;
class QWidget;
class QLayout;
class QGraphicsWidget;
class QGraphicsProxyWidget;
class QTextDocument;

namespace nx::vms::client::desktop {

class WidgetUtils
{
public:
    static void removeLayout(QLayout* layout);
    static void setFlag(QWidget* widget, Qt::WindowFlags flags, bool value);

    static void setRetainSizeWhenHidden(QWidget* widget, bool value);

    /** Clears specified menu and, unline QMenu::clear, deletes all owned submenus. */
    static void clearMenu(QMenu* menu);

    /** Unlike QWidget::graphicsProxyWidget finds proxy recursively. */
    static QGraphicsProxyWidget* graphicsProxyWidget(const QWidget* widget);
    static const QWidget* graphicsProxiedWidget(const QWidget* widget);

    /** Workaround while Qt's QWidget::mapFromGlobal is broken. */
    static QPoint mapFromGlobal(const QGraphicsWidget* to, const QPoint& globalPos);
    static QPoint mapFromGlobal(const QWidget* to, const QPoint& globalPos);

    /** Limit document height, adding specified text (default to "...") as the last visible line. */
    static void elideDocumentHeight(
        QTextDocument* document,
        int maxHeight,
        const QString& tail = "...");

    /** Limit document lines, adding specified text (default to "...") as the last visible line. */
    static void elideDocumentLines(
        QTextDocument* document,
        int maxLines,
        bool trimLastLine = false,
        const QString& tail = "...");
};

} // namespace nx::vms::client::desktop
