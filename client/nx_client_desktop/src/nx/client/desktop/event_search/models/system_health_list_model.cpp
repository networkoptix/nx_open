#include "system_health_list_model.h"
#include "private/system_health_list_model_p.h"

#include <core/resource/resource.h>

#include <nx/client/desktop/ui/actions/action_manager.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

const auto kDisplayTimeout = std::chrono::milliseconds(12500);

} // namespace

SystemHealthListModel::SystemHealthListModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

SystemHealthListModel::~SystemHealthListModel()
{
}

int SystemHealthListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d->count();
}

QVariant SystemHealthListModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return QVariant();

    switch (role)
    {
        case Qt::DisplayRole:
            return d->text(index.row());

        case Qt::DecorationRole:
            return d->pixmap(index.row());

        case Qt::ToolTipRole:
            return d->toolTip(index.row());

        case Qt::ForegroundRole:
        {
            const auto color = d->color(index.row());
            return color.isValid() ? QVariant::fromValue(color) : QVariant();
        }

        case Qn::ResourceRole:
            return QVariant::fromValue(d->resource(index.row()));

        case Qn::RemovableRole:
            return true;

        case Qn::PriorityRole:
            return d->priority(index.row());

        case Qn::TimeoutRole:
            return d->locked(index.row()) ? -1 : std::chrono::milliseconds(kDisplayTimeout).count();

        default:
            return base_type::data(index, role);
    }
}

bool SystemHealthListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!isValid(index))
        return false;

    switch (role)
    {
        case Qn::DefaultNotificationRole:
        case Qn::ActivateLinkRole:
        {
            const auto action = d->action(index.row());
            if (action == ui::action::NoAction)
                return false;

            menu()->triggerIfPossible(action, d->parameters(index.row()));

            if (d->message(index.row()) == QnSystemHealth::CloudPromo)
                menu()->trigger(ui::action::HideCloudPromoAction);

            return true;
        }

        default:
            return false;
    }
}

bool SystemHealthListModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (parent.isValid() || row < 0 || count < 0 || row + count > rowCount())
        return false;

    for (int i = 0; i < count; ++i)
    {
        if (d->message(row + i) == QnSystemHealth::CloudPromo)
        {
            menu()->trigger(ui::action::HideCloudPromoAction);
            break;
        }
    }

    d->remove(row, count);
    return true;
}

} // namespace
} // namespace client
} // namespace nx
