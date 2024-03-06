// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>

#include <client/client_globals.h>
#include <nx/utils/impl_ptr.h>

class QnResourcePool;

namespace nx::vms::common::saas { class ServiceManager; };
namespace nx::vms::client::desktop { class SystemContext; };

namespace nx::vms::client::desktop::saas {

class ServicesUsageModel: public QAbstractItemModel
{
    Q_OBJECT
    using base_type = QAbstractItemModel;

public:
    enum Column
    {
        ServiceNameColumn,
        ServiceTypeColumn,
        ServiceOveruseWarningIconColumn,
        TotalQantityColumn,
        UsedQantityColumn,

        ColumnCount,
    };

    enum Role
    {
        ServiceTypeRole = Qn::ItemDataRoleCount + 1,
        ServiceOverusedRole,
    };

    ServicesUsageModel(
        nx::vms::common::saas::ServiceManager* serviceManager,
        QObject* parent = nullptr);

    virtual ~ServicesUsageModel() override;

    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    virtual QModelIndex index(int row, int column,
        const QModelIndex& parent = QModelIndex()) const override;

    virtual QModelIndex parent(const QModelIndex& index) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    /**
     * Sets whether the model should track changes of relevant properties of camera resources to
     * notify about services usage count changes. It's worth stopping track camera changes if
     * there is no visible item view using this model as a source.
     */
    void setCamerasChangesTracking(SystemContext* systemContext, bool enabled);
    bool isTrackingCamerasChanges() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::saas
