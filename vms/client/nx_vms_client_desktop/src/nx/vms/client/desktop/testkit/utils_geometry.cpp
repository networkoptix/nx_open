// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QWidget>
#include <QtWidgets/QGraphicsObject>
#include <QtWidgets/QGraphicsView>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QAbstractItemView>

#include "model_index_wrapper.h"
#include "tab_item_wrapper.h"

namespace nx::vms::client::desktop::testkit::utils {

std::pair<QMenu*, QRect> findActionGeometry(QAction* action)
{
    // Try to use direct parent, maybe it's a QMenu and we can get the action geometry.
    auto menu = qobject_cast<QMenu*>(action->parentWidget());
    QRect rect;
    if (menu && menu->isVisible())
        rect = menu->actionGeometry(action);

    if (rect.isValid())
        return {menu, rect};

    // Try to find top level QMenu which provides valid geometry for the action.
    const auto topLevelWidgets = qApp->topLevelWidgets();
    for (auto widget: topLevelWidgets)
    {
        menu = qobject_cast<QMenu*>(widget);
        if (!menu || !menu->isVisible())
            continue;
        rect = menu->actionGeometry(action);
        if (rect.isValid())
            return {menu, rect};
    }

    return {nullptr, {}};
}

QRect globalRect(QVariant object, QWindow** window)
{
    // Avoid multiple `if (window)` conditions below.
    QWindow* dummyStorage;
    if (!window)
        window = &dummyStorage;
    *window = nullptr;

    // If we already got the rectangle, just pass it through.
    const QMetaType::Type objectType = (QMetaType::Type)object.type();
    if (objectType == QMetaType::QRect || objectType == QMetaType::QRectF)
        return object.toRect();

    auto qobj = qvariant_cast<QObject*>(object);

    // Map object rectangle to global, each type has it's own way of mapping the coordinates.
    if (const auto w = qobject_cast<const QWidget*>(qobj))
    {
        const QRect r = w->rect();
        *window = w->window()->windowHandle();
        return QRect(w->mapToGlobal(r.topLeft()), w->mapToGlobal(r.bottomRight()));
    }
    else if (const auto action = qobject_cast<QAction*>(qobj))
    {
        auto [menu, rect] = findActionGeometry(action);
        if (!menu)
            return {};

        *window = menu->window()->windowHandle();
        return QRect(
            menu->mapToGlobal(rect.topLeft()),
            menu->mapToGlobal(rect.bottomRight()));
    }
    else if (const auto go = qobject_cast<const QGraphicsObject*>(qobj))
    {
        const QGraphicsView* v = go->scene()->views().first();
        const QRectF sceneRect = go->mapRectToScene(go->boundingRect());
        const QRect viewRect = v->mapFromScene(sceneRect).boundingRect();
        const QPoint topLeft = v->viewport()->mapToGlobal(viewRect.topLeft());
        const QPoint bottomRight = v->viewport()->mapToGlobal(viewRect.bottomRight());
        *window = v->window()->windowHandle();
        return QRect(topLeft, bottomRight);
    }
    else if (auto qi = qobject_cast<QQuickItem*>(qobj))
    {
        *window = qi->window();
        const QPointF topLeft = qi->mapToGlobal(qi->boundingRect().topLeft());
        const QPointF bottomRight = qi->mapToGlobal(qi->boundingRect().bottomRight());
        return QRect(topLeft.toPoint(), bottomRight.toPoint());
    }
    else if (auto w = qobject_cast<QWindow*>(qobj))
    {
        const QPointF topLeft = w->mapToGlobal(QPoint(0, 0));
        const QPointF bottomRight = w->mapToGlobal(QPoint(w->width(), w->height()));
        *window = w;
        return QRect(topLeft.toPoint(), bottomRight.toPoint());
    }
    else if (object.isValid())
    {
        if (auto wrap = object.value<ModelIndexWrapper>(); wrap.isValid())
        {
            auto view = qobject_cast<QAbstractItemView*>(wrap.container());
            if (!view)
                return QRect();

            const QRect rect = view->visualRect(wrap.index());
            *window = view->window()->windowHandle();
            return QRect(
                view->viewport()->mapToGlobal(rect.topLeft()),
                view->viewport()->mapToGlobal(rect.bottomRight()));
        }
        else if (auto wrap = object.value<TabItemWrapper>(); wrap.isValid())
        {
            const auto container = wrap.container();
            const auto r = container->tabRect(wrap.index());
            const auto topLeft = container->mapToGlobal(r.topLeft());
            const auto bottomRight = container->mapToGlobal(r.bottomRight());
            return QRect(topLeft, bottomRight);
        }
    }
    return QRect();
}

qreal sideIntersect(QRect widgetRect, QRect sideRect, Qt::Alignment align)
{
    const int area = sideRect.width() * sideRect.height();
    if (area == 0)
        return 0.0;

    // Compute aligned rect
    QRect testRect;
    switch (align)
    {
        case Qt::AlignLeft:
        {
            int left = std::min(sideRect.left(), widgetRect.left());
            testRect = QRect(
                left,
                widgetRect.top(),
                widgetRect.left() - left,
                widgetRect.height());
            break;
        }
        case Qt::AlignRight:
        {
            int right = std::max(sideRect.right(), widgetRect.right());
            testRect = QRect(
                widgetRect.right(),
                widgetRect.top(),
                right - widgetRect.right(),
                widgetRect.height());
            break;
        }
        case Qt::AlignTop:
        {
            int top = std::min(sideRect.top(), widgetRect.top());
            testRect = QRect(
                widgetRect.left(),
                top,
                widgetRect.width(),
                widgetRect.top() - top);
            break;
        }
        case Qt::AlignBottom:
        {
            int bottom = std::max(sideRect.bottom(), widgetRect.bottom());
            testRect = QRect(
                widgetRect.left(),
                widgetRect.bottom(),
                widgetRect.width(),
                bottom - widgetRect.bottom());
        }
    }

    if (testRect.width() == 0 || testRect.height() == 0)
        return 0.0;

    auto r = sideRect.intersected(testRect);
    return (r.width() * r.height()) / (qreal) area;
}

} // namespace nx::vms::client::desktop::testkit::utils
