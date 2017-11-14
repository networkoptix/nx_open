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

bool SystemHealthListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    const auto result = base_type::setData(index, value, role);
    if (role != Qn::DefaultTriggerRole || !result)
        return result;

    const auto& event = getEvent(index.row());
    if (Private::extraData(event).first == QnSystemHealth::CloudPromo)
        menu()->trigger(ui::action::HideCloudPromoAction);

    return result;
}

bool SystemHealthListModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (parent.isValid() || row < 0 || count < 0 || row + count > rowCount())
        return false;

    for (int i = 0; i <= count; ++i)
    {
        const auto& event = getEvent(row + i);
        if (Private::extraData(event).first == QnSystemHealth::CloudPromo)
        {
            menu()->trigger(ui::action::HideCloudPromoAction);
            break;
        }
    }

    return base_type::removeRows(row, count);
}

int SystemHealthListModel::eventPriority(const EventData& event) const
{
    return d->eventPriority(event);
}

} // namespace
} // namespace client
} // namespace nx
