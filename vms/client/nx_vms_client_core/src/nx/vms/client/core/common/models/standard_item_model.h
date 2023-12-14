// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QStandardItemModel>

namespace nx::vms::client::core {

/**
 * QStandardItemModel with `moveRows` implementation.
 */
class NX_VMS_CLIENT_CORE_API StandardItemModel: public QStandardItemModel
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QAbstractItemModel)

public:
    virtual bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
        const QModelIndex& destinationParent, int destinationChild) override;
};

} // namespace nx::vms::client::core
