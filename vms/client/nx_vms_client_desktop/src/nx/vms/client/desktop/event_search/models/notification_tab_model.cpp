// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_tab_model.h"
#include "notification_list_model.h"
#include "local_notifications_list_model.h"
#include "system_health_list_model.h"

#include <ui/models/sort_filter_list_model.h>

#include <nx/vms/client/desktop/common/models/subset_list_model.h>

namespace nx::vms::client::desktop {

namespace {

class SystemHealthSortFilterModel: public QnSortFilterListModel
{
public:
    using QnSortFilterListModel::QnSortFilterListModel;

    bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const
    {
        return sourceLeft.data(Qn::PriorityRole).toInt()
            > sourceRight.data(Qn::PriorityRole).toInt();
    }
};

class SeparatorListModel: public QAbstractListModel
{
public:
    using QAbstractListModel::QAbstractListModel; //< Forward constructors.

    virtual int rowCount(const QModelIndex& parent) const override
    {
        return parent.isValid() ? 0 : 1;
    }

    virtual QVariant data(const QModelIndex& /*index*/, int /*role*/) const override
    {
        return QVariant();
    }
};

} // namespace

struct NotificationTabModel::Private
{
    NotificationListModel* const notificationsModel;
    SystemHealthListModel* const systemHealthModel;
    LocalNotificationsListModel* const localNotificationsModel;
};

NotificationTabModel::NotificationTabModel(QnWorkbenchContext* context, QObject* parent):
    base_type(parent),
    d(new Private{
        new NotificationListModel(context, this),
        new SystemHealthListModel(context, this),
        new LocalNotificationsListModel(context, this)})
{
    auto sortModel = new SystemHealthSortFilterModel(this);
    auto systemHealthModelProxy = new SubsetListModel(sortModel, 0, QModelIndex(), this);
    sortModel->setSourceModel(d->systemHealthModel);

    auto separatorModel = new SeparatorListModel(this);

    setModels({systemHealthModelProxy,
        d->localNotificationsModel,
        separatorModel,
        d->notificationsModel});
}

NotificationTabModel::~NotificationTabModel()
{
}

NotificationListModel* NotificationTabModel::notificationsModel() const
{
    return d->notificationsModel;
}

SystemHealthListModel* NotificationTabModel::systemHealthModel() const
{
    return d->systemHealthModel;
}

LocalNotificationsListModel* NotificationTabModel::localNotificationsModel() const
{
    return d->localNotificationsModel;
}

} // namespace nx::vms::client::desktop
