#include "system_health_list_model.h"
#include "private/system_health_list_model_p.h"

#include <nx/client/desktop/ui/actions/action_manager.h>

namespace nx {
namespace client {
namespace desktop {

SystemHealthListModel::SystemHealthListModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

SystemHealthListModel::~SystemHealthListModel()
{
}

void SystemHealthListModel::triggerDefaultAction(const EventData& event)
{
    base_type::triggerDefaultAction(event);

    if (Private::extraData(event).first == QnSystemHealth::CloudPromo)
        menu()->trigger(ui::action::HideCloudPromoAction);
}

void SystemHealthListModel::triggerCloseAction(const EventData& event)
{
    if (Private::extraData(event).first == QnSystemHealth::CloudPromo)
        menu()->trigger(ui::action::HideCloudPromoAction);

    base_type::triggerCloseAction(event);
}

void SystemHealthListModel::beforeRemove(const EventData& event)
{
    base_type::beforeRemove(event);
    d->beforeRemove(event);
}

} // namespace
} // namespace client
} // namespace nx
