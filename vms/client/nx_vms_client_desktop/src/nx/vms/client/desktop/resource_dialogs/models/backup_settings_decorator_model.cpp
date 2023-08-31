// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_settings_decorator_model.h"

#include <map>
#include <set>
#include <utility>

#include <client/client_globals.h>
#include <client_core/client_core_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/utils.h>
#include <nx/vms/api/data/backup_settings.h>
#include <nx/vms/api/data/system_settings.h>
#include <nx/vms/client/core/settings/system_settings_manager.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource_dialogs/backup_settings_view_common.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_properties/server/flux/server_settings_dialog_store.h>
#include <nx/vms/client/desktop/resource_views/data/resource_extra_status.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_service_usage_helper.h>
#include <nx/vms/common/system_settings.h>

namespace {

/**
 * Scoped guard that calls the given callback at the end of a scope if value returned by the
 * provided function wrapper at the scope begin differs to the one returned at the end of a scope.
 */
template <typename ValueType>
class [[nodiscard]] ScopedChangeNotifier
{
public:
    using ValueGetter = std::function<ValueType()>;
    using NotificationCallback = std::function<void()>;

    ScopedChangeNotifier(
        const ValueGetter& valueGetter,
        const NotificationCallback& notificationCallback)
        :
        m_valueGetter(valueGetter),
        m_notificationCallback(notificationCallback),
        m_initialValue(m_valueGetter())
    {
    }

    ScopedChangeNotifier() = delete;
    ScopedChangeNotifier(const ScopedChangeNotifier&) = delete;
    ScopedChangeNotifier& operator=(const ScopedChangeNotifier&) = delete;
    ~ScopedChangeNotifier()
    {
        if (m_initialValue != m_valueGetter())
            m_notificationCallback();
    }

private:
    const ValueGetter m_valueGetter;
    const NotificationCallback m_notificationCallback;
    const ValueType m_initialValue;
};

/**
 * Scoped guard that accumulates sequence of single row 'dataChanged' notifications that should be
 * sent by the given item model. At the end of a scope notifications will be clustered and sent
 * using range-based notification form whenever it possible. It aims to the minimize number of
 * notifications even at the cost of some amount of false change notifications. It's expected that
 * there will be no operations that change the structure or layout of the given model within the
 * scope.
 */
class [[nodiscard]] ModelDataChangeScopedAccumulator
{
public:
    ModelDataChangeScopedAccumulator(QAbstractItemModel* model):
        m_model(model)
    {
    };

    ModelDataChangeScopedAccumulator() = delete;
    ModelDataChangeScopedAccumulator(const ModelDataChangeScopedAccumulator&) = delete;
    ModelDataChangeScopedAccumulator& operator=(const ModelDataChangeScopedAccumulator&) = delete;
    ~ModelDataChangeScopedAccumulator()
    {
        for (auto const& [parent, rowsSet]: m_parentsAndChangedRows)
        {
            const auto firstRow = *rowsSet.cbegin();
            const auto lastRow = *rowsSet.crbegin();

            const auto firstColumn = 0;
            const auto lastColumn = m_model->columnCount() - 1;

            emit m_model->dataChanged(
                m_model->index(firstRow, firstColumn, parent),
                m_model->index(lastRow, lastColumn, parent));
        }
    };

    void rowDataChanged(const QModelIndex& index)
    {
        m_parentsAndChangedRows[index.siblingAtColumn(0).parent()].insert(index.row());
    };

private:
    QAbstractItemModel* m_model;
    std::map<QModelIndex, std::set<int>> m_parentsAndChangedRows;
};

bool cameraHasDefaultBackupSettings(const QnVirtualCameraResourcePtr& camera)
{
    return camera->getBackupQuality() == nx::vms::api::CameraBackupQuality::CameraBackup_Default
        || camera->getBackupPolicy() == nx::vms::api::BackupPolicy::byDefault;
}

bool cameraHasNotRemovedFlag(const QnVirtualCameraResourcePtr& camera)
{
    return !camera->hasFlags(Qn::ResourceFlag::removed);
}

int cameraMaxResolutionMegaPixels(const QnVirtualCameraResourcePtr& camera)
{
    return camera->cameraMediaCapability().maxResolution.megaPixels();
}

ScopedChangeNotifier<bool> makeHasChangesNotifier(
    nx::vms::client::desktop::BackupSettingsDecoratorModel* decoratorModel)
{
    return ScopedChangeNotifier<bool>(
        [decoratorModel]{ return decoratorModel->hasChanges(); },
        [decoratorModel]{ emit decoratorModel->hasChangesChanged(); });
}

ScopedChangeNotifier<nx::vms::api::BackupSettings> makeGlobalBackupSettingsChangesNotifier(
    nx::vms::client::desktop::BackupSettingsDecoratorModel* decoratorModel)
{
    return ScopedChangeNotifier<nx::vms::api::BackupSettings>(
        [decoratorModel]{ return decoratorModel->globalBackupSettings(); },
        [decoratorModel]{ emit decoratorModel->globalBackupSettingsChanged(); });
}

using MessageTexts = std::pair<QString, QString>;
using MessageTextsList = QVector<MessageTexts>;

} // namespace

namespace nx::vms::client::desktop {

using namespace nx::vms::api;
using namespace backup_settings_view;

//-------------------------------------------------------------------------------------------------
// BackupSettingsDecoratorModel::Private declaration.

struct BackupSettingsDecoratorModel::Private
{
    BackupSettingsDecoratorModel* const q;

    QHash<QnVirtualCameraResourcePtr, BackupContentTypes> changedContentTypes;
    QHash<QnVirtualCameraResourcePtr, CameraBackupQuality> changedQuality;
    QHash<QnVirtualCameraResourcePtr, bool> changedEnabledState;
    std::optional<nx::vms::api::BackupSettings> changedGlobalBackupSettings;

    bool isCloudBackupStorage = false;
    std::unique_ptr<nx::vms::common::saas::CloudStorageServiceUsageHelper> cloudStorageUsageHelper;

    BackupContentTypes backupContentTypes(const QnVirtualCameraResourcePtr& camera) const;
    CameraBackupQuality backupQuality(const QnVirtualCameraResourcePtr& camera) const;
    bool backupEnabled(const QnVirtualCameraResourcePtr& camera) const;

    void setGlobalBackupSettings(const nx::vms::api::BackupSettings& backupSettings);

    QnVirtualCameraResourceList camerasToApplySettings() const;
    void onRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last);

    std::optional<MessageTexts> nothingToBackupWarning(const QModelIndex& index) const;
    std::optional<MessageTexts> servicesOveruseWarning(const QModelIndex& index) const;

    int availableCloudStorageServices(const QnVirtualCameraResourcePtr& camera) const;
    void proposeServicesUsageChange();

    bool hasChanges() const;
    void applyChanges();
    void discardChanges();
};

//-------------------------------------------------------------------------------------------------
// BackupSettingsDecoratorModel::Private definition.

BackupContentTypes BackupSettingsDecoratorModel::Private::backupContentTypes(
    const QnVirtualCameraResourcePtr& camera) const
{
    if (changedContentTypes.contains(camera))
        return changedContentTypes.value(camera);

    return camera->getBackupContentType();
}

CameraBackupQuality BackupSettingsDecoratorModel::Private::backupQuality(
    const QnVirtualCameraResourcePtr& camera) const
{
    if (!camera->hasDualStreaming())
        return CameraBackupQuality::CameraBackup_Both;

    if (changedQuality.contains(camera))
        return changedQuality.value(camera);

    return camera->getActualBackupQualities();
}

bool BackupSettingsDecoratorModel::Private::backupEnabled(
    const QnVirtualCameraResourcePtr& camera) const
{
    if (changedEnabledState.contains(camera))
        return changedEnabledState.value(camera);

    return camera->isBackupEnabled();
}

void BackupSettingsDecoratorModel::Private::setGlobalBackupSettings(
    const BackupSettings& backupSettings)
{
    auto globalBackupSettingsChangesNotifier = makeGlobalBackupSettingsChangesNotifier(q);
    auto hasChangesNotifier = makeHasChangesNotifier(q);

    const auto savedGlobalBackupSettings = q->systemContext()->systemSettings()->backupSettings;
    if (savedGlobalBackupSettings == backupSettings)
        changedGlobalBackupSettings.reset();
    else
        changedGlobalBackupSettings = backupSettings;
}

QnVirtualCameraResourceList BackupSettingsDecoratorModel::Private::camerasToApplySettings() const
{
    QnVirtualCameraResourceList cameras =
        changedContentTypes.keys() + changedQuality.keys() + changedEnabledState.keys();

    if (changedGlobalBackupSettings)
    {
        cameras.append(q->resourcePool()->getAllCameras(QnResourcePtr(), /*ignoreDesktopCameras*/ true)
            .filtered(cameraHasDefaultBackupSettings));
    }

    std::sort(cameras.begin(), cameras.end());
    const auto newEnd = std::unique(cameras.begin(), cameras.end());
    cameras.erase(newEnd, cameras.end());

    return cameras.filtered(cameraHasNotRemovedFlag);
}

void BackupSettingsDecoratorModel::Private::onRowsAboutToBeRemoved(
    const QModelIndex& parent, int first, int last)
{
    auto hasChangesNotifier = makeHasChangesNotifier(q);

    for (int row = first; row <= last; ++row)
    {
        if (const auto camera = q->cameraResource(q->index(row, ResourceColumn, parent)))
        {
            changedContentTypes.remove(camera);
            changedQuality.remove(camera);
            changedEnabledState.remove(camera);
        }
    }
}

std::optional<MessageTexts> BackupSettingsDecoratorModel::Private::nothingToBackupWarning(
    const QModelIndex& index) const
{
    const auto warningCaption = tr("Nothing to backup");

    if (index.column() != ContentTypesColumn && index.column() != WarningIconColumn)
        return {};

    const auto camera = q->cameraResource(index);
    if (camera.isNull() || !backupEnabled(camera))
        return {};

    const auto resourceExtraStatus = q->mapToSource(index).siblingAtColumn(ResourceColumn)
        .data(Qn::ResourceExtraStatusRole).value<ResourceExtraStatus>();

    if (!resourceExtraStatus.testFlag(ResourceExtraStatusFlag::hasArchive)
        && !resourceExtraStatus.testFlag(ResourceExtraStatusFlag::scheduled)
        && !resourceExtraStatus.testFlag(ResourceExtraStatusFlag::recording))
    {
        return {{
            warningCaption,
            tr("The camera has neither recorded footage nor recording scheduled")}};
    }

    const auto cameraHasAnalytics = !camera->supportedObjectTypes().empty();
    const auto noMotionWarningText = tr("Motion detection is disabled");
    const auto noAnalyticsWarningText = tr("No analytics plugins");

    if (backupContentTypes(camera) == BackupContentTypes(BackupContentType::motion)
        && !camera->isMotionDetectionActive())
    {
        return {{warningCaption, noMotionWarningText}};
    }

    if (backupContentTypes(camera) == BackupContentTypes(BackupContentType::analytics)
        && !cameraHasAnalytics)
    {
        return {{warningCaption, noAnalyticsWarningText}};
    }

    if (backupContentTypes(camera) == BackupContentTypes(
        {BackupContentType::motion, BackupContentType::analytics})
            && (!camera->isMotionDetectionActive() && !cameraHasAnalytics))
    {
        return {{warningCaption,
            QStringList({noMotionWarningText, noAnalyticsWarningText}).join(QChar::LineFeed)}};
    }

    return {};
}

std::optional<MessageTexts> BackupSettingsDecoratorModel::Private::servicesOveruseWarning(
    const QModelIndex& index) const
{
    if (!isCloudBackupStorage)
        return {};

    const auto camera = q->cameraResource(index);
    if (camera.isNull() || !backupEnabled(camera))
        return {};

    const auto availableServices = availableCloudStorageServices(camera);
    if (availableServices < 0) //< Services overflow.
    {
        return {{
            tr("Insufficient services"),
            tr("%n suitable cloud storage services are required", "", -availableServices)}};
    }

    return {};
}

int BackupSettingsDecoratorModel::Private::availableCloudStorageServices(
    const QnVirtualCameraResourcePtr& camera) const
{
    const auto resolution = cameraMaxResolutionMegaPixels(camera);
    const auto servicesSummary = cloudStorageUsageHelper->allInfoForResolution(resolution);
    return servicesSummary.available - servicesSummary.inUse;
}

void BackupSettingsDecoratorModel::Private::proposeServicesUsageChange()
{
    std::set<QnUuid> toAdd;
    std::set<QnUuid> toRemove;
    for (auto i = changedEnabledState.begin(); i != changedEnabledState.end(); ++i)
    {
        if (i.value())
            toAdd.insert(i.key()->getId());
        else
            toRemove.insert(i.key()->getId());
    }
    cloudStorageUsageHelper->proposeChange(toAdd, toRemove);
}

bool BackupSettingsDecoratorModel::Private::hasChanges() const
{
    return !changedContentTypes.isEmpty()
        || !changedQuality.isEmpty()
        || !changedEnabledState.isEmpty()
        || changedGlobalBackupSettings;
}

void BackupSettingsDecoratorModel::Private::applyChanges()
{
    const auto cameras = camerasToApplySettings();
    if (!cameras.isEmpty())
    {
        qnResourcesChangesManager->saveCameras(cameras,
            [this](const auto& camera)
            {
                camera->setBackupContentType(backupContentTypes(camera));
                camera->setBackupQuality(backupQuality(camera));
                camera->setBackupPolicy(backupEnabled(camera)
                    ? BackupPolicy::on
                    : BackupPolicy::off);
            });
    }

    if (changedGlobalBackupSettings)
    {
        q->systemContext()->systemSettings()->backupSettings = changedGlobalBackupSettings.value();
        q->systemContext()->systemSettingsManager()->saveSystemSettings();
    }

    changedContentTypes.clear();
    changedQuality.clear();
    changedEnabledState.clear();
    changedGlobalBackupSettings.reset();
}

void BackupSettingsDecoratorModel::Private::discardChanges()
{
    auto hasChangesNotifier = makeHasChangesNotifier(q);

    changedContentTypes.clear();
    changedQuality.clear();
    changedEnabledState.clear();
    changedGlobalBackupSettings.reset();
}

//-------------------------------------------------------------------------------------------------
// BackupSettingsDecoratorModel definition.

BackupSettingsDecoratorModel::BackupSettingsDecoratorModel(
    ServerSettingsDialogStore* store,
    SystemContext* systemContext)
    :
    base_type(nullptr),
    SystemContextAware(systemContext),
    d(new Private{this})
{
    connect(this, &BackupSettingsDecoratorModel::rowsAboutToBeRemoved,
        [this](const QModelIndex& parent, int first, int last)
        {
            d->onRowsAboutToBeRemoved(parent, first, last);
        });

    connect(store, &ServerSettingsDialogStore::stateChanged,
        this, &BackupSettingsDecoratorModel::loadState);
}

BackupSettingsDecoratorModel::~BackupSettingsDecoratorModel()
{
}

QVariant BackupSettingsDecoratorModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    const auto column = index.column();

    // Data for 'New added cameras' pinned row. Doesn't make sense for cloud storage backup
    // settings since model doesn't contain that row in that case.
    if (isNewAddedCamerasRow(mapToSource(index)))
    {
        switch (role)
        {
            case Qt::DisplayRole:
                if (column == ContentTypesColumn)
                    return backupContentTypesToString(BackupContentType::archive);
                break;
            case Qt::ToolTipRole:
                if (column == ContentTypesColumn)
                    return tr("Cannot be modified for new added cameras");
                break;
            case BackupEnabledRole:
                if (column == SwitchColumn)
                    return globalBackupSettings().backupNewCameras;
                break;
            case BackupQualityRole:
                if (column == QualityColumn)
                    return globalBackupSettings().quality;
                break;
            case HasDropdownMenuRole:
                return column == QualityColumn;

            default:
                return base_type::data(index, role);
        }
    }

    // No additional data for rows that doesn't contain camera resource.
    const auto camera = cameraResource(index);
    if (camera.isNull())
        return base_type::data(index, role);

    // Data for rows that contain camera resource which doesn't support backup, lile NVRs.
    if (!isBackupSupported(camera) && column == ContentTypesColumn)
    {
        switch (role)
        {
            case Qt::DisplayRole:
                return tr("Not supported");

            case Qt::ToolTipRole:
                return tr("Backup is not supported for this device");

            default:
                return base_type::data(index, role);;
        }
    }

    // Data for rows that contain camera resource which supports backup.
    const auto backupEnabled = d->backupEnabled(camera);
    const auto cameraResolution = cameraMaxResolutionMegaPixels(camera);

    const auto availableServices = d->isCloudBackupStorage
        ? d->availableCloudStorageServices(camera) : 0;
    const auto insufficientServices = d->isCloudBackupStorage && availableServices <= 0;

    const auto nothingToBackupWarning = d->nothingToBackupWarning(index);
    const auto servicesOveruseWarning = d->servicesOveruseWarning(index);

    switch (role)
    {
        case Qt::DisplayRole:
            if (column == ResolutionColumn && cameraResolution > 0)
                return tr("%n MP", "", cameraResolution);
            break;

        case Qt::ToolTipRole:
            if (column == ResolutionColumn)
            {
                return cameraResolution > 0
                    ? tr("%n Megapixels", "", cameraResolution)
                    : tr("Unknown resolution");
            }

            if (column == QualityColumn && !camera->hasVideo())
                return tr("Stream setting is not applicable to this device type");

            if (column == QualityColumn && !camera->hasDualStreaming())
                return tr("This device provides only one data stream");

            if (column == SwitchColumn && insufficientServices && !backupEnabled)
                return tr("No suitable cloud storage services available");
            break;

        case Qn::ItemMouseCursorRole:
            if (column == SwitchColumn && insufficientServices && !backupEnabled)
                return QVariant::fromValue<int>(Qt::ForbiddenCursor);
            break;

        case IsItemHighlightedRole:
            if (column == ResourceColumn)
                return backupEnabled;
            break;

        case BackupEnabledRole:
            return backupEnabled;

        case BackupContentTypesRole:
            return static_cast<int>(d->backupContentTypes(camera));

        case BackupQualityRole:
            return static_cast<int>(d->backupQuality(camera));

        case HasDropdownMenuRole:
            if (!isDropdownColumn(index))
                return {};
            return column != QualityColumn || camera->hasDualStreaming();

        case InfoMessageRole:
            if (d->isCloudBackupStorage && !backupEnabled && !servicesOveruseWarning)
            {
                return availableServices > 0
                    ? tr("%n suitable cloud storage services available", "", availableServices)
                    : tr("No suitable cloud storage services available");
            }
            break;

        case IsItemWarningStyleRole:
            if ((column == ContentTypesColumn || column == WarningIconColumn)
                && nothingToBackupWarning)
            {
                return true;
            }
            if (column == ResourceColumn && servicesOveruseWarning)
            {
                return true;
            }
            break;

        case WarningMessagesRole:
            {
                MessageTextsList warningMessages;

                if (servicesOveruseWarning)
                    warningMessages.append(servicesOveruseWarning.value());

                if ((column == ContentTypesColumn || column == WarningIconColumn)
                    && nothingToBackupWarning)
                {
                    warningMessages.append(nothingToBackupWarning.value());
                }
                return QVariant::fromValue(warningMessages);
            }
            break;

        case ResolutionMegaPixelRole:
            return cameraResolution;

        case AvailableCloudStorageServices:
            if (d->isCloudBackupStorage)
                return d->availableCloudStorageServices(camera);
            break;

        default:
            break;
    }

    return base_type::data(index, role);
}

BackupSettings BackupSettingsDecoratorModel::globalBackupSettings() const
{
    return d->changedGlobalBackupSettings
        ? *d->changedGlobalBackupSettings
        : systemContext()->systemSettings()->backupSettings;
}

void BackupSettingsDecoratorModel::setBackupContentTypes(
    const QModelIndexList& indexes, BackupContentTypes contentTypes)
{
    auto hasChangesNotifier = makeHasChangesNotifier(this);
    ModelDataChangeScopedAccumulator dataChangedScopedAccumulator(this);

    for (const auto& index: indexes)
    {
        const auto camera = cameraResource(index);
        if (!camera)
            continue;

        if (!NX_ASSERT(isBackupSupported(camera)))
            continue;

        if (d->backupContentTypes(camera) != contentTypes)
        {
            d->changedContentTypes.insert(camera, contentTypes);
            dataChangedScopedAccumulator.rowDataChanged(index);
        }
    }
}

void BackupSettingsDecoratorModel::setBackupQuality(
    const QModelIndexList& indexes, CameraBackupQuality quality)
{
    auto hasChangesNotifier = makeHasChangesNotifier(this);
    ModelDataChangeScopedAccumulator dataChangedScopedAccumulator(this);

    for (const auto& index: indexes)
    {
        const auto camera = cameraResource(index);

        if (isNewAddedCamerasRow(mapToSource(index)))
        {
            auto backupSettings = globalBackupSettings();
            if (backupSettings.quality != quality)
            {
                backupSettings.quality = quality;
                d->setGlobalBackupSettings(backupSettings);
                dataChangedScopedAccumulator.rowDataChanged(index);
            }
        }
        else if (!camera.isNull())
        {
            if (!NX_ASSERT(isBackupSupported(camera)))
                continue;

            if (!camera->hasDualStreaming())
                continue;

            if (d->backupQuality(camera) != quality)
            {
                d->changedQuality.insert(camera, quality);
                dataChangedScopedAccumulator.rowDataChanged(index);
            }
        }
    }
}

void BackupSettingsDecoratorModel::setBackupEnabled(
    const QModelIndexList& indexes, bool enabled)
{
    auto hasChangesNotifier = makeHasChangesNotifier(this);
    ModelDataChangeScopedAccumulator dataChangedScopedAccumulator(this);

    for (const auto& index: indexes)
    {
        const auto camera = cameraResource(index);

        if (isNewAddedCamerasRow(mapToSource(index)))
        {
            auto backupSettings = globalBackupSettings();
            if (backupSettings.backupNewCameras != enabled)
            {
                backupSettings.backupNewCameras = enabled;
                d->setGlobalBackupSettings(backupSettings);
                dataChangedScopedAccumulator.rowDataChanged(index);
            }
        }
        else if (!camera.isNull())
        {
            if (!NX_ASSERT(isBackupSupported(camera)))
                continue;

            if (enabled
                && d->isCloudBackupStorage
                && d->availableCloudStorageServices(camera) < 1)
            {
                continue;
            }

            if (d->backupEnabled(camera) != enabled)
            {
                if (d->changedEnabledState.contains(camera))
                    d->changedEnabledState.remove(camera);
                else
                    d->changedEnabledState.insert(camera, enabled);
                
                dataChangedScopedAccumulator.rowDataChanged(index);
            }

            if (d->isCloudBackupStorage)
                d->proposeServicesUsageChange();
        }
    }
}

QVariant BackupSettingsDecoratorModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const
{
    if (orientation == Qt::Vertical || role != Qt::DisplayRole)
        return base_type::headerData(section, orientation, role);

    switch (section)
    {
        case ResourceColumn:
            // TODO: #vbreus Find smarter way of header text aligning.
            return tr("Cameras").prepend(QString(10, QChar::Space));
        case ResolutionColumn:
            return tr("Resolution");
        case ContentTypesColumn:
            return tr("What to backup");
        case QualityColumn:
            return tr("Quality");
        default:
            break;
    }

    return base_type::headerData(section, orientation, role);
}

QnVirtualCameraResourcePtr BackupSettingsDecoratorModel::cameraResource(
    const QModelIndex& index) const
{
    if (!index.isValid() || !(index.model() == this))
        return {};

    const auto resourceIndex = mapToSource(index).siblingAtColumn(ResourceColumn);
    const auto resourceData = resourceIndex.data(Qn::ResourceRole);
    if (resourceData.isNull())
        return {};

    return resourceData.value<QnResourcePtr>().dynamicCast<QnVirtualCameraResource>();
}

void BackupSettingsDecoratorModel::loadState(const ServerSettingsDialogState& state)
{
    if (d->isCloudBackupStorage == state.backupStoragesStatus.usesCloudBackupStorage)
        return;

    d->isCloudBackupStorage = state.backupStoragesStatus.usesCloudBackupStorage;

    d->cloudStorageUsageHelper.reset(d->isCloudBackupStorage
        ? new nx::vms::common::saas::CloudStorageServiceUsageHelper(systemContext())
        : nullptr);
}

bool BackupSettingsDecoratorModel::hasChanges() const
{
    return d->hasChanges();
}

void BackupSettingsDecoratorModel::discardChanges()
{
    d->discardChanges();

    if (d->isCloudBackupStorage)
        d->proposeServicesUsageChange();

    if (rowCount() > 0)
    {
        emit dataChanged(
            index(0, WarningIconColumn),
            index(rowCount() - 1, ColumnCount - 1));
    }
}

void BackupSettingsDecoratorModel::applyChanges()
{
    d->applyChanges();
}

} // namespace nx::vms::client::desktop
