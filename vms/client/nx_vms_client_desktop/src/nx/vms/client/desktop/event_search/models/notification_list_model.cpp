// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_list_model.h"

#include <nx/fusion/model_functions.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench_context.h>

#include "private/notification_list_model_p.h"

namespace nx::vms::client::desktop {

namespace {

QString getStatisticsAlias(const QString& postfix)
{
    static const auto kNotificationWidgetAlias = lit("notification_widget");
    return lit("%1_%2").arg(kNotificationWidgetAlias, postfix);
}

} // namespace

NotificationListModel::NotificationListModel(WindowContext* context, QObject* parent):
    base_type(context, parent),
    d(new Private(this))
{
}

NotificationListModel::~NotificationListModel()
{
}

bool NotificationListModel::defaultAction(const QModelIndex& index)
{
    const auto result = base_type::defaultAction(index);
    if (!result)
        return false;

    const auto& event = getEvent(index.row());
    if (event.actionId != menu::NoAction)
    {
        statisticsModule()->controls()->registerClick(
            getStatisticsAlias(QnLexical::serialized(event.actionId)));
    }

    return true;
}

int NotificationListModel::maximumCount() const
{
    return d->maximumCount();
}

void NotificationListModel::setMaximumCount(int value)
{
    d->setMaximumCount(value);
}

} // namespace nx::vms::client::desktop
