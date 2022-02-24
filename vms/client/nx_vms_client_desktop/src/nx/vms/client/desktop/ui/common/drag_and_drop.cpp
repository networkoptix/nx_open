// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "drag_and_drop.h"
#include "private/drag_and_drop_p.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QMimeData>
#include <QtCore/QPointer>
#include <QtGui/QDrag>
#include <QtQml/QtQml>
#include <QtQuick/private/qquickpixmapcache_p.h>

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

namespace {

QPixmap emptyPixmap()
{
    QPixmap result(1, 1);
    result.fill(Qt::transparent);
    return result;
}

QPixmap loadPixmap(const DragAndDrop* context, const QUrl& source)
{
    if (source.isEmpty())
        return emptyPixmap();

    const QQuickPixmap pixmapLoader(qmlEngine(context), source);
    if (!pixmapLoader.image().isNull())
        return QPixmap::fromImage(pixmapLoader.image());

    NX_WARNING(context, "Image \"%1\" cannot be loaded", source);
    return emptyPixmap();
}

} // namespace

Qt::DropAction DragAndDrop::execute(
    QObject* source,
    QMimeData* mimeData,
    Qt::DropActions supportedActions,
    Qt::DropAction proposedAction,
    const QUrl& imageSource,
    const QPointF& hotSpot) const
{
    if (!NX_ASSERT(mimeData))
        return Qt::IgnoreAction;

    if (!NX_ASSERT(source))
    {
        mimeData->deleteLater();
        return Qt::IgnoreAction;
    }

    auto drag(new QDrag(source));
    drag->setPixmap(loadPixmap(this, imageSource));
    drag->setHotSpot(hotSpot.toPoint());

    drag->setMimeData(mimeData);
    QQmlEngine::setObjectOwnership(mimeData, QQmlEngine::CppOwnership);

    // The drag object will be automatically deleted afterwards.
    return drag->exec(supportedActions, proposedAction);
}

QMimeData* DragAndDrop::createMimeData(const QModelIndexList& indices)
{
    const QAbstractItemModel* model = nullptr;
    QModelIndexList filtered;

    for (const auto& index : indices)
    {
        if (!index.isValid())
            continue;

        if (!model)
            model = index.model();
        else if (!NX_ASSERT(model == index.model()))
            continue;

        if (index.flags().testFlag(Qt::ItemIsDragEnabled))
            filtered.push_back(index);
    }

    if (filtered.empty() || !NX_ASSERT(model))
        return {};

    return model->mimeData(filtered);
}

Qt::DropActions DragAndDrop::supportedDragActions(QAbstractItemModel* model)
{
    return NX_ASSERT(model)
        ? model->supportedDragActions()
        : Qt::DropActions();
}

Qt::DropActions DragAndDrop::supportedDropActions(QAbstractItemModel* model)
{
    return NX_ASSERT(model)
        ? model->supportedDropActions()
        : Qt::DropActions();
}

bool DragAndDrop::canDrop(QMimeData* mimeData, Qt::DropAction action, const QModelIndex& target)
{
    if (!target.isValid() || !NX_ASSERT(target.model() && target.model()->checkIndex(target)))
        return false;

    return target.model()->canDropMimeData(mimeData, action, -1, -1, target);
}

bool DragAndDrop::drop(QMimeData* mimeData, Qt::DropAction action, const QModelIndex& target)
{
    if (!target.isValid() || !NX_ASSERT(target.model() && target.model()->checkIndex(target)))
        return false;

    return const_cast<QAbstractItemModel*>(target.model())->dropMimeData(
        mimeData, action, -1, -1, target);
}

void DragAndDrop::registerQmlType()
{
    qmlRegisterSingletonType<DragAndDrop>("nx.vms.client.desktop", 1, 0, "DragAndDrop",
        [](QQmlEngine*, QJSEngine*) -> QObject* { return new DragAndDrop(); });

    qmlRegisterType<DragMimeDataWatcher>("nx.vms.client.desktop", 1, 0, "DragMimeDataWatcher");
    qmlRegisterType<DragHoverWatcher>("nx.vms.client.desktop", 1, 0, "DragHoverWatcher");

    qRegisterMetaType<QMimeData*>();
}

} // namespace nx::vms::client::desktop
