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

    if (event.extraData.toInt() == QnSystemHealth::CloudPromo)
        menu()->trigger(ui::action::HideCloudPromoAction);
}

void SystemHealthListModel::triggerCloseAction(const EventData& event)
{
    base_type::triggerCloseAction(event);

    if (event.extraData.toInt() == QnSystemHealth::CloudPromo)
        menu()->trigger(ui::action::HideCloudPromoAction);
}

} // namespace
} // namespace client
} // namespace nx
