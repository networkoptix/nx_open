#include "widget_utils.h"

#include <limits>

#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QLayout>
#include <QtWidgets/QMenu>
#include <QtGui/QTextDocument>
#include <QtGui/QTextBlock>

namespace nx::vms::client::desktop {

void WidgetUtils::removeLayout(QLayout* layout)
{
    if (!layout)
        return;

    while (const auto item = layout->takeAt(0))
    {
        if (const auto widget = item->widget())
            delete widget;
        else if (const auto childLayout = item->layout())
            removeLayout(childLayout);
        delete item;
    }
    delete layout;
}

void WidgetUtils::setFlag(QWidget* widget, Qt::WindowFlags flags, bool value)
{
    auto initialFlags = widget->windowFlags();
    if (value)
        initialFlags |= flags;
    else
        initialFlags &= ~flags;

    widget->setWindowFlags(initialFlags);
}

void WidgetUtils::clearMenu(QMenu* menu)
{
    if (!menu)
        return;

    menu->clear();

    for (auto subMenu: menu->findChildren<QMenu*>(QString(), Qt::FindChildrenRecursively))
        subMenu->deleteLater();
}

const QWidget* WidgetUtils::graphicsProxiedWidget(const QWidget* widget)
{
    while (widget && !widget->graphicsProxyWidget())
        widget = widget->parentWidget();

    return widget;
}

QGraphicsProxyWidget* WidgetUtils::graphicsProxyWidget(const QWidget* widget)
{
    if (auto proxied = graphicsProxiedWidget(widget))
        return proxied->graphicsProxyWidget();

    return nullptr;
}

QPoint WidgetUtils::mapFromGlobal(const QWidget* to, const QPoint& globalPos)
{
    if (auto proxied = graphicsProxiedWidget(to))
    {
        return to->mapFrom(proxied, mapFromGlobal(
            proxied->graphicsProxyWidget(), globalPos));
    }

    return to->mapFromGlobal(globalPos);
}

QPoint WidgetUtils::mapFromGlobal(const QGraphicsWidget* to, const QPoint& globalPos)
{
    static const QPoint kInvalidPos(
        std::numeric_limits<int>::max(),
        std::numeric_limits<int>::max());

    auto scene = to->scene();
    if (!scene)
        return kInvalidPos;

    auto views = scene->views();
    if (views.empty())
        return kInvalidPos;

    auto viewPos = mapFromGlobal(views[0], globalPos);
    return to->mapFromScene(views[0]->mapToScene(viewPos)).toPoint();
}

void WidgetUtils::elideDocumentHeight(QTextDocument* document, int maxHeight, const QString& tail)
{
    std::optional<int> lastFullyVisiblePosition;

    // Iterate over document blocks and their lines to find first partially visible line.
    for (QTextBlock block = document->firstBlock(); block.isValid(); block = block.next())
    {
        const QTextLayout* layout = block.layout();
        for (int lineIndex = 0; lineIndex < layout->lineCount(); ++lineIndex)
        {
            const QTextLine line = layout->lineAt(lineIndex);

            const int documentPosition = block.position() + line.textStart();

            // Is line fully visible?
            if (layout->position().y() + line.naturalTextRect().bottom() <= maxHeight)
            {
                // Remember it and go to the next line.
                lastFullyVisiblePosition = documentPosition;
                continue;
            }

            // Found first partially visible line.

            if (!lastFullyVisiblePosition) //< First line is already partially visible.
                lastFullyVisiblePosition = documentPosition;

            // Replace last fully visible line with tail.
            QTextCursor cursor(document);
            cursor.setPosition(*lastFullyVisiblePosition);
            cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            cursor.insertText(tail);
            return;
        }
    }
}

} // namespace nx::vms::client::desktop
