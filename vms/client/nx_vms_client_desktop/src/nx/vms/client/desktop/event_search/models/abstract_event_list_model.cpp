// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_event_list_model.h"

#include <chrono>

#include <client/client_globals.h>
#include <nx/utils/datetime.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/time/formatter.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/synctime.h>

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

        case Qn::DisplayedResourceListRole:
            return index.data(Qn::ResourceListRole);

        case Qt::AccessibleTextRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
            return index.data(Qt::DisplayRole);

        case Qt::AccessibleDescriptionRole:
            return index.data(Qn::DescriptionTextRole);

        case Qt::DecorationRole:
        {
            const auto path = index.data(Qn::DecorationPathRole).toString();
            return path.isEmpty() ? QPixmap() : qnSkin->pixmap(path);
        }

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

    // TODO: #sivanov Actualize used system context.
    const auto timeWatcher = appContext()->currentSystemContext()->serverTimeWatcher();
    const QDateTime dateTime = timeWatcher->displayTime(timestampMs);

    // For current day just display the time in system format.
    if (qnSyncTime->currentDateTime().date() == dateTime.date())
        return nx::vms::time::toString(dateTime.time());

    // Display both date and time for all other days.
    return nx::vms::time::toString(dateTime, nx::vms::time::Format::dd_MM_yy_hh_mm_ss);
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
