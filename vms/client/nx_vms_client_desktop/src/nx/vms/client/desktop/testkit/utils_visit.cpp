// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <QtQml/QJSValue>
#include <QtQuickWidgets/QQuickWidget>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QGraphicsObject>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QMenu>

namespace nx::vms::client::desktop::testkit::utils {

void visitModel(
    const QAbstractItemModel* model,
    QModelIndex parent,
    std::function<bool(QModelIndex)> onVisited)
{
    if (!model || !model->hasChildren(parent))
        return;

    for (int r = 0; r < model->rowCount(parent); ++r)
    {
        for (int c = 0; c < model->columnCount(parent); ++c)
        {
            QModelIndex index = model->index(r, c, parent);
            if (onVisited(index))
                visitModel(model, index, onVisited);
        }
    }
}

void visitTree(QObject* object, std::function<bool(QObject*)> onVisited, VisitOptions flags)
{
    if (!object)
        return;

    if (flags.testFlag(OnlyQuickItems) && !qobject_cast<const QQuickItem*>(object))
        return;

    if (!onVisited(object))
        return;

    if (const auto w = qobject_cast<const QMenu*>(object))
    {
        foreach(QAction* a, w->actions())
            visitTree(a, onVisited, flags);
    }

    // Visual children of QQuickItem
    if (const auto root = qobject_cast<const QQuickItem*>(object))
    {
        QList<QQuickItem*> items = root->childItems();
        for (int i = 0; i < items.size(); ++i)
            visitTree(items.at(i), onVisited, flags | OnlyQuickItems);
    }

    // Visual children of QGraphicsObject
    if (const auto root = qobject_cast<const QGraphicsObject*>(object))
    {
        QList<QGraphicsItem*> items = root->childItems();
        for (int i = 0; i < items.size(); ++i)
        {
            if (const auto o = dynamic_cast<QGraphicsObject*>(items.at(i)))
                visitTree(o, onVisited, flags);
        }
    }

    // Step inside Qt Quick root object.
    if (const auto w = qobject_cast<const QQuickWidget*>(object))
        visitTree(w->rootObject(), onVisited, flags | OnlyQuickItems);

    if (const auto w = qobject_cast<const QQuickWindow*>(object))
        visitTree(w->contentItem(), onVisited, flags | OnlyQuickItems);

    // Step inside QGraphicsView.
    if (const auto w = qobject_cast<const QGraphicsView*>(object))
    {
        QList<QGraphicsItem*> items = w->items();
        for (QGraphicsItem* item: items)
        {
            if (const auto o = dynamic_cast<QGraphicsObject*>(item))
                visitTree(o, onVisited, flags);
        }
    }

    const QObjectList children = object->children();
    for (int i = 0; i < children.size(); ++i)
        visitTree(children.at(i), onVisited, flags);
}

} // namespace nx::vms::client::desktop::testkit::utils
