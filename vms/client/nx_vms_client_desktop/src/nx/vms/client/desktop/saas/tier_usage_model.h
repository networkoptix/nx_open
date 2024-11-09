// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtGui/QStandardItemModel>

#include <client/client_globals.h>
#include <nx/utils/impl_ptr.h>

class QnResourcePool;

namespace nx::vms::common::saas { class ServiceManager; };
namespace nx::vms::client::desktop { class SystemContext; };

namespace nx::vms::client::desktop::saas {

class TierUsageModel: public QStandardItemModel
{
    Q_OBJECT
    using base_type = QStandardItemModel;

public:
    enum Column
    {
        LimitTypeColumn,
        AllowedLimitColumn,
        UsedLimitColumn,

        ColumnCount,
    };

    enum Role
    {
        LimitTypeRole = Qn::ItemDataRoleCount + 1,
    };

    TierUsageModel(
        nx::vms::common::saas::ServiceManager* serviceManager,
        QObject* parent = nullptr);

    virtual ~TierUsageModel() override;

    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    void setResourcesChangesTracking(SystemContext* systemContext, bool enabled);
    bool isTrackingResourcesChanges() const;

signals:
    void modelDataUpdated();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::saas
