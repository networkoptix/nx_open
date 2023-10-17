// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_tab_model.h"

#include <nx/vms/client/desktop/common/models/subset_list_model.h>
#include <ui/models/sort_filter_list_model.h>

#include "call_notifications_list_model.h"
#include "local_notifications_list_model.h"
#include "notification_list_model.h"
#include "system_health_list_model.h"

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
        return !parent.isValid() && m_visible ? 1 : 0;
    }

    virtual QVariant data(const QModelIndex& /*index*/, int /*role*/) const override
    {
        return QVariant();
    }

    void setVisible(bool visible)
    {
        if (m_visible == visible)
            return;

        beginResetModel();
        m_visible = visible;
        endResetModel();
    }

private:
    bool m_visible = false;
};

} // namespace

struct NotificationTabModel::Private
{
    NotificationListModel* const notificationsModel;
    SystemHealthListModel* const systemHealthModel;
    LocalNotificationsListModel* const localNotificationsModel;
    CallNotificationsListModel* const callNotificationsModel;

    SeparatorListModel* createSeparatorModel(QAbstractItemModel* baseModel)
    {
        auto separatorModel = new SeparatorListModel(baseModel);

        separatorModel->setVisible(baseModel->rowCount() > 0);

        connect(baseModel, &QAbstractItemModel::rowsAboutToBeInserted, separatorModel,
            [baseModel, separatorModel]
            {
                separatorModel->setVisible(true);
            });

        connect(baseModel, &QAbstractItemModel::rowsAboutToBeRemoved, separatorModel,
            [baseModel, separatorModel](const QModelIndex& /*parent*/, int first, int last)
            {
                const bool stillHasRows = (baseModel->rowCount() - (first - last + 1)) > 0;
                separatorModel->setVisible(stillHasRows);
            });

        // We cannot connect fully valid handler to modelReset event.
        // This handler is for actual baseModel behavior.
        // Model is resetted in case of system disconnecting. It is just clearing.
        connect(baseModel, &QAbstractItemModel::modelAboutToBeReset, separatorModel,
            [baseModel, separatorModel]
            {
                separatorModel->setVisible(false);
            });

        return separatorModel;
    }
};

NotificationTabModel::NotificationTabModel(WindowContext* context, QObject* parent):
    base_type(parent),
    d(new Private{
        new NotificationListModel(context, this),
        new SystemHealthListModel(context, this),
        new LocalNotificationsListModel(context, this),
        new CallNotificationsListModel(context, this)})
{
    auto sortModel = new SystemHealthSortFilterModel(this);
    auto systemHealthModelProxy = new SubsetListModel(sortModel, 0, QModelIndex(), this);
    sortModel->setSourceModel(d->systemHealthModel);

    setModels({systemHealthModelProxy,
        d->localNotificationsModel,
        d->createSeparatorModel(d->callNotificationsModel),
        d->callNotificationsModel,
        d->createSeparatorModel(d->notificationsModel),
        d->notificationsModel});
}

NotificationTabModel::~NotificationTabModel()
{
}

} // namespace nx::vms::client::desktop
