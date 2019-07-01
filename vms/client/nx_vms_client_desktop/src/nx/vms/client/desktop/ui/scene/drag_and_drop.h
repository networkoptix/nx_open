#pragma once

#include <QtCore/QtCore>
#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/QUrl>

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
    Q_INVOKABLE Qt::DropAction execute(
        QObject* source,
        QMimeData* mimeData, //< Ownership is taken, instance is deleted after the drag.
        Qt::DropActions supportedActions = Qt::MoveAction,
        Qt::DropAction proposedAction = Qt::IgnoreAction,
        const QUrl& imageSource = QUrl(),
        const QPointF& hotSpot = QPointF(0, 0)) const;

    static void registerQmlType();
};

} // namespace nx::vms::client::desktop
