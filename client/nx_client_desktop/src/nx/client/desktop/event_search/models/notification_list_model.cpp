#include "notification_list_model.h"
#include "private/notification_list_model_p.h"

#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench_context.h>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

QString getStatisticsAlias(const QString& postfix)
{
    static const auto kNotificationWidgetAlias = lit("notification_widget");
    return lit("%1_%2").arg(kNotificationWidgetAlias, postfix);
};

} // namespace

NotificationListModel::NotificationListModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

NotificationListModel::~NotificationListModel()
{
}

void NotificationListModel::triggerDefaultAction(const EventData& event)
{
    base_type::triggerDefaultAction(event);

    if (event.actionId != ui::action::NoAction)
    {
        context()->statisticsModule()->registerClick(
            getStatisticsAlias(QnLexical::serialized(event.actionId)));
    }
}

void NotificationListModel::beforeRemove(const EventData& event)
{
    base_type::beforeRemove(event);
    d->beforeRemove(event);
}

} // namespace
} // namespace client
} // namespace nx
