// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "import_from_device_dialog_model.h"

#include <client/client_globals.h>
#include <client_core/client_core_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/resource_dialogs/filtering/filtered_resource_proxy_model.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/time/formatter.h>

using namespace nx::vms::client::core;
using namespace std::chrono;

namespace nx::vms::client::desktop::integrations {

namespace {

enum Columns
{
    NameColumn,
    ImportedUpToColumn,
    StatusColumn,

    ColumnCount
};

enum class State
{
    allImported,
    inProgress,
    disabled,
    error
};

struct DeviceData
{
    State state = State::allImported;
    QDate importedUpTo;
    std::chrono::milliseconds importTimeLeft{0};
};

State convertState(nx::vms::api::RemoteArchiveSynchronizationStateCode code)
{
    switch (code)
    {
        case nx::vms::api::RemoteArchiveSynchronizationStateCode::idle:
            return State::allImported;
        case nx::vms::api::RemoteArchiveSynchronizationStateCode::inProgress:
            return State::inProgress;
        case nx::vms::api::RemoteArchiveSynchronizationStateCode::disabled:
            return State::disabled;
        case nx::vms::api::RemoteArchiveSynchronizationStateCode::error:
            return State::error;
    }

    NX_ASSERT(false, "Unexpected archive sync state code");
    return State::error;
}

} // namespace

struct ImportFromDeviceDialogModel::Private
{
    ImportFromDeviceDialogModel* const q;

    std::map<QnUuid, DeviceData> deviceData;

    FilteredResourceProxyModel* const resourceModel = nullptr;
    QnResourceIconCache* const resourceIconCache = nullptr;

public:
    Private(ImportFromDeviceDialogModel* parent);

    std::map<QnUuid, DeviceData>::const_iterator getDeviceDataIter(const QModelIndex& index) const;

    QString getNameText(const QModelIndex& index) const;
    QString getImportedUpToText(const QModelIndex& index) const;
    QString getStatusText(const QModelIndex& index) const;
    QString getTooltipText(const QModelIndex& index) const;
    QColor getCellColor(const QModelIndex& index) const;
};

ImportFromDeviceDialogModel::Private::Private(ImportFromDeviceDialogModel* parent):
    q(parent),
    resourceModel(resourceModel),
    resourceIconCache(qnResIconCache)
{
}

std::map<QnUuid, DeviceData>::const_iterator
    ImportFromDeviceDialogModel::Private::getDeviceDataIter(const QModelIndex& index) const
{
    const auto sourceIndex = q->mapToSource(q->createIndex(index.row(), 0));

    const auto resource = qvariant_cast<QnResourcePtr>(
        q->sourceModel()->data(sourceIndex, Qn::ResourceRole));

    if (!NX_ASSERT(resource))
        return deviceData.cend();

    return deviceData.find(resource->getId());
}

QString ImportFromDeviceDialogModel::Private::getNameText(const QModelIndex& index) const
{
    const auto sourceIndex = q->mapToSource(index);

    const auto resource =
        q->sourceModel()->data(sourceIndex, Qn::ResourceRole).value<QnResourcePtr>();
    if (!NX_ASSERT(resource))
        return {};

    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!NX_ASSERT(resource))
        return {};

    return camera->getUserDefinedName();
}

QString ImportFromDeviceDialogModel::Private::getImportedUpToText(const QModelIndex& index) const
{
    const QString kNoData = ImportFromDeviceDialogModel::tr("No data");

    const auto iter = getDeviceDataIter(index);
    if (iter == deviceData.end())
        return kNoData;

    if (State::disabled == iter->second.state)
        return kNoData;

    return nx::vms::time::toString(iter->second.importedUpTo, nx::vms::time::Format::dd_MMMM_yyyy);
}

QString ImportFromDeviceDialogModel::Private::getStatusText(const QModelIndex& index) const
{
    State state = State::disabled;

    const auto iter = getDeviceDataIter(index);
    if (iter != deviceData.end())
        state = iter->second.state;

    switch (state)
    {
        case State::allImported:
        {
            return ImportFromDeviceDialogModel::tr("All imported");
        }
        case State::inProgress:
        {
            NX_ASSERT(iter != deviceData.end());
            const auto timeLeftString = nx::vms::time::toDurationString(
                iter->second.importTimeLeft,
                nx::vms::time::Duration::hh_mm);

            return ImportFromDeviceDialogModel::tr("In progress... (%1 left)").arg(timeLeftString);
        }
        case State::disabled:
        {
            return ImportFromDeviceDialogModel::tr("Disabled");
        }
        case State::error:
        {
            return ImportFromDeviceDialogModel::tr("Error");
        }
    }

    NX_ASSERT(false, "Unexpected state");
    return {};
}

QString ImportFromDeviceDialogModel::Private::getTooltipText(const QModelIndex& index) const
{
    if (index.column() != StatusColumn)
        return {};

    const auto iter = getDeviceDataIter(index);
    if (iter != deviceData.end() && iter->second.state == State::error)
        return ImportFromDeviceDialogModel::tr("Failed to import. Retry in 1 minute.");

    return {};
}

QColor ImportFromDeviceDialogModel::Private::getCellColor(const QModelIndex& index) const
{
    State state = State::disabled;

    const auto iter = getDeviceDataIter(index);
    if (iter != deviceData.end())
        state = iter->second.state;

    switch (state)
    {
        case State::allImported:
        case State::inProgress:
        {
            return colorTheme()->color("light10");
        }
        case State::disabled:
        {
            return colorTheme()->color("dark17");
        }
        case State::error:
        {
            return index.column() == StatusColumn
                ? colorTheme()->color("red_core")
                : colorTheme()->color("light10");
        }
    }

    NX_ASSERT(false, "Unexpected state");
    return {};
}

ImportFromDeviceDialogModel::ImportFromDeviceDialogModel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new ImportFromDeviceDialogModel::Private(this))
{
}

ImportFromDeviceDialogModel::~ImportFromDeviceDialogModel()
{
}

void ImportFromDeviceDialogModel::setData(
    const nx::vms::api::RemoteArchiveSynchronizationStatusList& data)
{
    beginResetModel();

    for (const auto& deviceStatus: data)
    {
        const auto timeMs = duration_cast<milliseconds>(
            deviceStatus.importedPositionMs.time_since_epoch());

        const auto dateTime = QDateTime::fromMSecsSinceEpoch(timeMs.count());

        d->deviceData[deviceStatus.getId().deviceId] = DeviceData{
                .state = convertState(deviceStatus.code),
                .importedUpTo = dateTime.date(),
                .importTimeLeft = deviceStatus.durationToImportMs
            };
    }

    endResetModel();
}

QHash<int, QByteArray> ImportFromDeviceDialogModel::roleNames() const
{
    auto roleNames = base_type::roleNames();
    roleNames.insert(sourceModel()->roleNames());
    roleNames[Qt::ForegroundRole] = "foregroundColor";
    return roleNames;
}

QVariant ImportFromDeviceDialogModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const
{
    if (role != Qt::DisplayRole)
        return {};

    switch (section)
    {
        case NameColumn:
            return tr("Name");
        case ImportedUpToColumn:
            return tr("Imported up to");
        case StatusColumn:
            return tr("Status");
    }

    NX_ASSERT(false, "Unexpected section value");
    return {};
}

QVariant ImportFromDeviceDialogModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || rowCount() <= index.row()
        || index.column() < 0 || columnCount() <= index.column()
        || !index.isValid())
    {
        return QVariant();
    }

    switch (role)
    {
        case Qt::DisplayRole:
        {
            switch (index.column())
            {
                case NameColumn:
                    return d->getNameText(index);
                case ImportedUpToColumn:
                    return d->getImportedUpToText(index);
                case StatusColumn:
                    return d->getStatusText(index);
            }

            NX_ASSERT(false, "Unexpected column index");
            return QVariant();
        }
        case Qt::ForegroundRole:
        {
            return d->getCellColor(index);
        }
        case Qt::ToolTipRole:
        {
            return d->getTooltipText(index);
        }
    }

    return base_type::data(ImportFromDeviceDialogModel::index(index.row(), 0), role);
}

QModelIndex ImportFromDeviceDialogModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (!sourceModel())
        return proxyIndex;

    const int row = proxyIndex.row();
    if (row < 0 || row >= sourceModel()->rowCount())
        return QModelIndex();

    const int column = proxyIndex.column();
    if (column != 0)
        return QModelIndex();

    return sourceModel()->index(row, 0);
}

QModelIndex ImportFromDeviceDialogModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!sourceModel())
        return sourceIndex;

    const int row = sourceIndex.row();
    if (row < 0 || row >= sourceModel()->rowCount())
        return QModelIndex();

    return index(row, 0);
}

QModelIndex ImportFromDeviceDialogModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || rowCount(parent) <= row || column < 0 || columnCount(parent) <= column)
        return QModelIndex();

    return createIndex(row, column);
}

QModelIndex ImportFromDeviceDialogModel::parent(const QModelIndex& index) const
{
    return sourceModel()->parent(mapToSource(index));
}

int ImportFromDeviceDialogModel::rowCount(const QModelIndex& parent) const
{
    return sourceModel()->rowCount(mapToSource(parent));
}

int ImportFromDeviceDialogModel::columnCount(const QModelIndex& parent) const
{
    return ColumnCount;
}

} // namespace nx::vms::client::desktop::integrations
