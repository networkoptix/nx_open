// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API LookupListEntriesSortFilterProxyModel: public QSortFilterProxyModel
{
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    explicit LookupListEntriesSortFilterProxyModel(QObject* parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

} // namespace nx::vms::client::desktop
