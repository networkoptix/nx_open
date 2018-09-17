#include "widget_utils.h"

#include <limits>

#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QLayout>

namespace nx {
namespace client {
namespace desktop {

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

} // namespace desktop
} // namespace client
} // namespace nx
