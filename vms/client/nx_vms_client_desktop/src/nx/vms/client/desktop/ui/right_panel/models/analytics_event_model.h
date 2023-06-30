// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/core/analytics/analytics_entities_tree.h>

namespace nx::vms::client::core { class AnalyticsEventsSearchTreeBuilder; }

namespace nx::vms::client::desktop {

class AnalyticsEventModel: public ScopedModelOperations<QAbstractItemModel>
{
    using base_type = ScopedModelOperations<QAbstractItemModel>;

public:
    enum Roles
    {
        NameRole = Qt::DisplayRole,
        IdRole = Qt::UserRole,
        EnabledRole
    };

public:
    AnalyticsEventModel(core::AnalyticsEventsSearchTreeBuilder* builder, QObject* parent = nullptr);
    virtual ~AnalyticsEventModel() override;

    virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    virtual QModelIndex parent(const QModelIndex& index) const override;
    virtual int rowCount(const QModelIndex& parent) const override;
    virtual int columnCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
