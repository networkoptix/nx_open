// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

namespace nx::vms::client::mobile {

class ResourceTreeSearchModel: public QSortFilterProxyModel
{
    Q_OBJECT
    using base_type = QSortFilterProxyModel;

public:
    ResourceTreeSearchModel(QObject* parent = nullptr);
    virtual ~ResourceTreeSearchModel() override;

protected:
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

} // namespace nx::vms::client::mobile
