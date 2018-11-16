#include "notification_list_model.h"
#include "private/notification_list_model_p.h"

#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench_context.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::desktop {

namespace {

QString getStatisticsAlias(const QString& postfix)
{
    static const auto kNotificationWidgetAlias = lit("notification_widget");
    return lit("%1_%2").arg(kNotificationWidgetAlias, postfix);
};

} // namespace

NotificationListModel::NotificationListModel(QnWorkbenchContext* context, QObject* parent):
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
    if (event.actionId != ui::action::NoAction)
    {
        context()->statisticsModule()->registerClick(
            getStatisticsAlias(QnLexical::serialized(event.actionId)));
    }

    return true;
}

} // namespace nx::vms::client::desktop
