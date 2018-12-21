#include "abstract_event_list_model.h"

#include <chrono>

#include <translation/datetime_formatter.h>
#include <utils/common/synctime.h>

#include <nx/utils/datetime.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

AbstractEventListModel::AbstractEventListModel(QnWorkbenchContext* context, QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(context)
{
    NX_CRITICAL(context, "Workbench context must be specified.");
}

QVariant AbstractEventListModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return QVariant();

    switch (role)
    {
        case Qn::TimestampTextRole:
            return timestampText(index.data(Qn::TimestampRole).value<microseconds>());

        case Qt::AccessibleTextRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
            return index.data(Qt::DisplayRole);

        case Qt::AccessibleDescriptionRole:
            return index.data(Qn::DescriptionTextRole);

        default:
            return QVariant();
    }
}

bool AbstractEventListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!isValid(index) || index.model() != this)
        return false;

    switch (role)
    {
        case Qn::DefaultNotificationRole:
            return defaultAction(index);

        case Qn::ActivateLinkRole:
            return activateLink(index, value.toString());

        default:
            return base_type::setData(index, value, role);
    }
}

bool AbstractEventListModel::isValid(const QModelIndex& index) const
{
    return index.model() == this && !index.parent().isValid() && index.column() == 0
        && index.row() >= 0 && index.row() < rowCount();
}

QString AbstractEventListModel::timestampText(microseconds timestamp) const
{
    if (timestamp <= 0ms)
        return QString();

    const auto timestampMs = duration_cast<milliseconds>(timestamp).count();
    const auto dateTime = QDateTime::fromMSecsSinceEpoch(timestampMs);
    if (qnSyncTime->currentDateTime().date() != dateTime.date())
        return datetime::toString(dateTime.date());
    else
        return datetime::toString(dateTime.time());
}

bool AbstractEventListModel::defaultAction(const QModelIndex& /*index*/)
{
    return false;
}

bool AbstractEventListModel::activateLink(const QModelIndex& index, const QString& /*link*/)
{
    // Default fallback implementation.
    return defaultAction(index);
}

} // namespace nx::vms::client::desktop
