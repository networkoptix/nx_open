// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

namespace nx::vms::client::desktop::saas {

class TierUsageSortModel: public QSortFilterProxyModel
{
    Q_OBJECT
    using base_type = QSortFilterProxyModel;

public:
    TierUsageSortModel(QObject* parent = nullptr);

protected:
    virtual bool lessThan(const QModelIndex& lhs, const QModelIndex& rhs) const;
};

} // namespace nx::vms::client::desktop::saas
