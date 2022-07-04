// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logs_management_model.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace nx::vms::client::desktop {

namespace {

const QString kNormalLogLevelColor = "light16";
const QString kWarningLogLevelColor = "yellow_d2";

QIcon icon(LogsManagementUnitPtr unit)
{
    return unit->server()
        ? qnResIconCache->icon(unit->server())
        : qnResIconCache->icon(QnResourceIconCache::Client);
}

QIcon statusIcon(LogsManagementUnitPtr unit)
{
    using State = LogsManagementWatcher::Unit::DownloadState;
    switch (unit->state())
    {
        case State::pending:
        case State::loading:
            return qnSkin->icon("text_buttons/rapid_review.png"); //< FIXME: #spanasenko Use a separate icon.

        case State::complete:
            return qnSkin->icon("text_buttons/ok.png");

        case State::error:
            return qnSkin->icon("text_buttons/clear_error.png");
    }

    return {};
}

nx::utils::log::Level logLevel(LogsManagementUnitPtr unit)
{
    if (!unit->settings())
        return nx::utils::log::Level::undefined;

    // Currently we show MainLog level only, even if settings of other Loggers differ.
    return unit->settings()->mainLog.primaryLevel;
}

QColor logLevelColor(LogsManagementUnitPtr unit)
{
    return logLevel(unit) == LogsManagementWatcher::defaultLogLevel()
        ? colorTheme()->color(kNormalLogLevelColor)
        : colorTheme()->color(kWarningLogLevelColor);
}

} // namespace

LogsManagementModel::LogsManagementModel(QObject* parent, LogsManagementWatcher* watcher):
    base_type(parent),
    m_watcher(watcher)
{
    // Model uses its own list of shared pointers to LogsManagementUnit instances, which is updated
    // when receiving a signal from Watcher. This list allows to safely show quite consistent data
    // w/o any additional synchronization between Model and Watcher during ResourcePool updates.
    connect(
        m_watcher, &LogsManagementWatcher::itemListChanged,
        this, &LogsManagementModel::onItemsListChanged);

    m_items = watcher->items();
}

int LogsManagementModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return ColumnCount;
}

int LogsManagementModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return m_items.size();
}

QVariant LogsManagementModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
            case NameColumn:
                return tr("Unit");
            case LogLevelColumn:
                return tr("Current Level");
            default:
                break;
        }
    }

    return {};
}

QVariant LogsManagementModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    auto unit = m_items.value(index.row());
    if (!NX_ASSERT(unit))
        return {};

    if (role == IpAddressRole)
    {
        if (auto server = unit->server())
            return QnResourceDisplayInfo(server).host();

        return {};
    }

    switch (index.column())
    {
        case NameColumn:
            switch (role)
            {
                case Qt::DecorationRole:
                    return icon(unit);

                case Qt::DisplayRole:
                    return unit->server()
                        ? QnResourceDisplayInfo(unit->server()).name()
                        : tr("Client");

            }
            break;

        case LogLevelColumn:
            switch (role)
            {
                case Qt::DisplayRole:
                    return logLevelName(logLevel(unit));

                case Qt::ForegroundRole:
                    return logLevelColor(unit);
            }
            break;

        case CheckBoxColumn:
            switch (role)
            {
                case Qt::CheckStateRole:
                    return unit->isChecked() ? Qt::Checked : Qt::Unchecked;
            }
            break;

        case StatusColumn:
            switch (role)
            {
                case Qt::DecorationRole:
                    return statusIcon(unit);
            }
            break;

        default:
            NX_ASSERT(false, "Unknown column id");
            break;
    }

    return {};
}

bool LogsManagementModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;

    if (index.column() != CheckBoxColumn || role != Qt::CheckStateRole)
        return false;

    if (!m_watcher)
        return false;

    const auto state = static_cast<Qt::CheckState>(value.toInt());
    m_watcher->setItemIsChecked(m_items.value(index.row()), state == Qt::Checked);

    return true;
}

Qt::ItemFlags LogsManagementModel::flags(const QModelIndex& index) const
{
    if (index.column() == CheckBoxColumn)
        return {Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled};

    return base_type::flags(index);
}

QList<nx::utils::log::Level> LogsManagementModel::logLevels()
{
    using Level = nx::utils::log::Level;
    return {Level::none, Level::error, Level::warning, Level::info, Level::debug, Level::verbose};
}

QString LogsManagementModel::logLevelName(nx::utils::log::Level level)
{
    switch (level)
    {
        case nx::utils::log::Level::none: return tr("none");
        case nx::utils::log::Level::error: return tr("error");
        case nx::utils::log::Level::warning: return tr("warning");
        case nx::utils::log::Level::info: return tr("info");
        case nx::utils::log::Level::debug: return tr("debug");
        case nx::utils::log::Level::verbose: return tr("verbose");
        default: return {};
    }
}

void LogsManagementModel::onItemsListChanged()
{
    beginResetModel();
    m_items = m_watcher->items();
    endResetModel();
}

} // namespace nx::vms::client::desktop
