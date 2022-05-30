// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logs_management_model.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/workbench/workbench_context.h>

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

nx::utils::log::Level logLevel(LogsManagementUnitPtr unit)
{
    if (!unit->settings())
        return nx::utils::log::Level::undefined;

    // TODO: should we check that http log has the same settings as main log?
    return unit->settings()->mainLog.primaryLevel;
}

QColor logLevelColor(LogsManagementUnitPtr unit)
{
    return logLevel(unit) == nx::utils::log::Level::info
        ? colorTheme()->color(kNormalLogLevelColor)
        : colorTheme()->color(kWarningLogLevelColor);
}

} // namespace

LogsManagementUnitPtr LogsManagementUnit::createClientUnit()
{
    return std::make_shared<LogsManagementUnit>();
}

LogsManagementUnitPtr LogsManagementUnit::createServerUnit(QnMediaServerResourcePtr server)
{
    auto unit = std::make_shared<LogsManagementUnit>();
    unit->m_server = server;
    return unit;
}

QnMediaServerResourcePtr LogsManagementUnit::server() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_server;
}

bool LogsManagementUnit::isChecked() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_checked;
}

void LogsManagementUnit::setChecked(bool isChecked)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_checked = isChecked;
}

LogsManagementUnit::DownloadState LogsManagementUnit::state() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_state;
}

void LogsManagementUnit::setState(LogsManagementUnit::DownloadState state)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_state = state;
}

std::optional<nx::vms::api::ServerLogSettings> LogsManagementUnit::settings() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_settings;
}

void LogsManagementUnit::setSettings(
    const std::optional<nx::vms::api::ServerLogSettings>& settings)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_settings = settings;
}

LogsManagementModel::LogsManagementModel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    // Init dummy data.
    m_client = LogsManagementUnit::createClientUnit();

    nx::vms::api::ServerLogSettings settings;
    settings.mainLog.primaryLevel = nx::utils::log::mainLogger()->defaultLevel();
    m_client->setSettings(settings);

    for (const auto& server: resourcePool()->servers())
    {
        auto unit = LogsManagementUnit::createServerUnit(server);

        nx::vms::api::ServerLogSettings settings;
        settings.mainLog.primaryLevel = nx::utils::log::Level::debug;
        unit->setSettings(settings);

        m_servers.push_back(unit);
    }
}

int LogsManagementModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return ColumnCount;
}

int LogsManagementModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid())
        return 0;

    return m_servers.size() + 1;
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

    auto unit = itemForRow(index.row());
    if (!NX_ASSERT(unit))
        return {};

    if (role == Qn::ResourceRole)
    {
        return {}; // TODO: media server resource.
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
                        ? unit->server()->getName()
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
                    return QIcon(); // TODO: add icons.
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

    const auto state = static_cast<Qt::CheckState>(value.toInt());

    auto unit = itemForRow(index.row());
    if (!unit)
        return false;
    unit->setChecked(!unit->isChecked());

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

LogsManagementUnitPtr LogsManagementModel::itemForRow(int idx) const
{
    if (idx < 0 || idx > m_servers.size())
        return nullptr;

    return idx
        ? m_servers[idx - 1]
        : m_client;
}

} // namespace nx::vms::client::desktop
