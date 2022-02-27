// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/QModelIndex>
#include <QtCore/QUrl>

class QAbstractItemModel;
class QMimeData;

namespace nx::vms::client::desktop {

/**
 * Drag & drop mechanism that allows using C++ QMimeData in QML.
 * It is useful for QML item views with C++ item models (QAbstractItemModel descendants).
 */
class DragAndDrop: public QObject
{
    Q_OBJECT

public:
    /**
     * Performs drag-n-drop operation synchronously.
     * Doesn't block the UI, but uses different mechanics on different platforms (see QDrag::exec).
     *
     * @param source Dragged object or an object designated as a drag source.
     * @param mimeData Drag MIME data; automatically deleted after the drag.
     * @param supportedActions Drop actions supported by the source.
     * @param proposedAction Default drop action.
     * @param imageSource URL of an image displayed near the mouse cursor during the drag.
     * @param hotSpot Point at the image that sticks to the mouse cursor.
     */
    Q_INVOKABLE Qt::DropAction execute(
        QObject* source,
        QMimeData* mimeData, //< Automatically deleted after the drag.
        Qt::DropActions supportedActions = Qt::MoveAction,
        Qt::DropAction proposedAction = Qt::IgnoreAction,
        const QUrl& imageSource = QUrl(),
        const QPointF& hotSpot = QPointF(0, 0)) const;

    Q_INVOKABLE static QMimeData* createMimeData(const QModelIndexList& indices);

    Q_INVOKABLE static Qt::DropActions supportedDragActions(QAbstractItemModel* model);
    Q_INVOKABLE static Qt::DropActions supportedDropActions(QAbstractItemModel* model);

    Q_INVOKABLE static bool canDrop(
        QMimeData* mimeData, Qt::DropAction action, const QModelIndex& target);

    Q_INVOKABLE static bool drop(
        QMimeData* mimeData, Qt::DropAction action, const QModelIndex& target);

    static void registerQmlType();
};

} // namespace nx::vms::client::desktop
