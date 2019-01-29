#include "time_synchronization_servers_model.h"

#include <algorithm>

#include <text/time_strings.h>
#include <translation/datetime_formatter.h>
#include <ui/style/resource_icon_cache.h>

#include <nx/client/core/utils/human_readable.h>
#include <nx/utils/guarded_callback.h>

#include <translation/datetime_formatter.h>

namespace {

static const QVector<int> kTextRoles = {
    Qt::DisplayRole,
    Qt::StatusTipRole,
    Qt::WhatsThisRole,
    Qt::AccessibleTextRole,
    Qt::AccessibleDescriptionRole,
    Qt::ToolTipRole
};

static const QVector<int> kCheckboxRoles =
{
    Qt::DisplayRole,
    Qt::CheckStateRole
};

static const QVector<int> kForegroundRole =
{
    Qt::ForegroundRole
};

} // namespace

namespace nx::vms::client::desktop {

TimeSynchronizationServersModel::TimeSynchronizationServersModel(QObject* parent):
    base_type(parent)
{
}

int TimeSynchronizationServersModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return ColumnCount;
}

int TimeSynchronizationServersModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return m_servers.size();
}

QVariant TimeSynchronizationServersModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
            case CheckboxColumn:
                return QString();
            case NameColumn:
                return tr("Server");
            case TimezoneColumn:
                return tr("Time Zone");
            case DateColumn:
                return tr("Date");
            case OsTimeColumn:
                return tr("Server OS Time");
            case VmsTimeColumn:
                return tr("VMS Time");
            default:
                break;
        }
    }
    return QVariant();
}

QVariant TimeSynchronizationServersModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return QVariant();

    const auto column = index.column();
    const auto server = m_servers[index.row()];

    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::StatusTipRole:
        case Qt::AccessibleTextRole:
        case Qt::ToolTipRole:
        {
            switch (column)
            {
                case NameColumn:
                    return server.name;
                case DateColumn:
                case TimezoneColumn:
                case OsTimeColumn:
                case VmsTimeColumn:
                {
                    static const QString kPlaceholder = QString::fromWCharArray(L"\x2014\x2014\x2014\x2014");
                    if (!server.online)
                        return kPlaceholder;

                    const auto offsetFromUtc = duration_cast<seconds>(server.timeZoneOffset).count();
                    const auto osTimestamp = duration_cast<milliseconds>(m_vmsTime + server.osTimeOffset).count();
                    const auto vmsTimestamp = duration_cast<milliseconds>(m_vmsTime + server.vmsTimeOffset).count();

                    auto osDateTime = QDateTime::fromMSecsSinceEpoch(osTimestamp, Qt::OffsetFromUTC, offsetFromUtc);
                    auto vmsDateTime = QDateTime::fromMSecsSinceEpoch(vmsTimestamp, Qt::OffsetFromUTC, offsetFromUtc);

                    switch (column)
                    {
                        case DateColumn:
                            return datetime::toString(osDateTime.date());
                        case TimezoneColumn:
                            return osDateTime.timeZoneAbbreviation();
                        case OsTimeColumn:
                            return datetime::toString(osDateTime.time());
                        case VmsTimeColumn:
                            return datetime::toString(vmsDateTime.time());
                    }
                }

                default:
                    break;
            }

            break;
        }

        case Qt::CheckStateRole:
            if (column == CheckboxColumn)
            {
                return server.id == m_selectedServer
                    ? Qt::Checked
                    : Qt::Unchecked;
            }
            break;

        case Qt::DecorationRole:
            if (column == NameColumn)
            {
                QnResourceIconCache::Key key = QnResourceIconCache::Server;
                if (!server.online)
                    key |= QnResourceIconCache::Offline;
                return qnResIconCache->icon(key);
            }
            break;

        case IpAddressRole:
            return server.ipAddress;

        case ServerIdRole:
            return qVariantFromValue(server.id);

        case ServerOnlineRole:
            return server.online;

        case TimeOffsetRole:
        {
            switch (column)
            {
                case OsTimeColumn:
                    return (qint64) server.osTimeOffset.count();
                case VmsTimeColumn:
                    return (qint64) server.vmsTimeOffset.count();

                default:
                    break;
            }
            break;
        }

        default:
            break;
    }

    return QVariant();
}

bool TimeSynchronizationServersModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!isValid(index))
        return false;

    if (index.column() != CheckboxColumn || role != Qt::CheckStateRole)
        return false;

    const auto state = static_cast<Qt::CheckState>(value.toInt());

    // Do not allow to uncheck element
    if (state == Qt::Unchecked)
        return false;

    const auto server = m_servers[index.row()];
    emit serverSelected(server.id);
    return true;
}

Qt::ItemFlags TimeSynchronizationServersModel::flags(const QModelIndex& index) const
{
    if (!isValid(index))
        return Qt::NoItemFlags;

    const auto column = index.column();
    const auto server = m_servers[index.row()];

    static const Qt::ItemFlags kBaseFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    static const Qt::ItemFlags kCheckboxFlags = Qt::ItemIsUserCheckable | Qt::ItemIsEditable;

    if (column == CheckboxColumn)
        return kBaseFlags | kCheckboxFlags;

    return kBaseFlags;
}

void TimeSynchronizationServersModel::loadState(const State& state)
{
    beginResetModel();
    m_vmsTime = state.vmsTime;
    m_servers = state.servers;
    m_selectedServer = state.primaryServer;
    endResetModel();
}

bool TimeSynchronizationServersModel::isValid(const QModelIndex& index) const
{
    return index.isValid()
        && index.model() == this
        && hasIndex(
            index.row(),
            index.column(),
            index.parent())
        && index.row() >= 0
        && index.row() < m_servers.size();
}

} // namespace nx::vms::client::desktop
