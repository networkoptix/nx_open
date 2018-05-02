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

bool NotificationListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    const auto result = base_type::setData(index, value, role);
    if (role != Qn::DefaultNotificationRole || !result)
        return result;

    const auto& event = getEvent(index.row());
    if (event.actionId != ui::action::NoAction)
    {
        context()->statisticsModule()->registerClick(
            getStatisticsAlias(QnLexical::serialized(event.actionId)));
    }

    return result;
}

} // namespace desktop
} // namespace client
} // namespace nx
