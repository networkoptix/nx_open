#pragma once

#include <QtCore/QObject>
#include <QtCore/QItemSelection>
#include <QtCore/QModelIndexList>

class QMimeData;

namespace nx::vms::client::desktop {

class ItemModelUtils: public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE static QItemSelection createSelection(
        const QModelIndex& topLeft, const QModelIndex& bottomRight);

    Q_INVOKABLE static QMimeData* mimeData(const QModelIndexList& indices);

    static void registerQmlType();
};

} // namespace nx::vms::client::desktop
