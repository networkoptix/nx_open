// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_settings_decorator_model.h"

#include <map>
#include <set>
#include <utility>

#include <client/client_globals.h>
#include <client_core/client_core_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <nx/analytics/utils.h>
#include <nx/vms/api/data/backup_settings.h>
#include <nx/vms/client/desktop/resource_dialogs/backup_settings_view_common.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_views/data/camera_extra_status.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
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
    return camera->getBackupQuality() == nx::vms::api::CameraBackupQuality::CameraBackupDefault
        || camera->getBackupPolicy() == nx::vms::api::BackupPolicy::byDefault;
}

bool cameraHasNotRemovedFlag(const QnVirtualCameraResourcePtr& camera)
{
    return !camera->hasFlags(Qn::ResourceFlag::removed);
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

} // namespace

namespace nx::vms::client::desktop {

using namespace nx::vms::api;
using namespace backup_settings_view;

BackupSettingsDecoratorModel::BackupSettingsDecoratorModel(SystemContext* systemContext):
    base_type(nullptr),
    SystemContextAware(systemContext)
{
    connect(this, &BackupSettingsDecoratorModel::rowsAboutToBeRemoved,
        this, &BackupSettingsDecoratorModel::onRowsAboutToBeRemoved);
}

BackupSettingsDecoratorModel::~BackupSettingsDecoratorModel()
{
}

QVariant BackupSettingsDecoratorModel::data(const QModelIndex& index, int role) const
{
    const auto camera = cameraResource(index);

    if (isNewAddedCamerasRow(mapToSource(index)))
    {
        switch (role)
        {
            case Qt::DisplayRole:
                if (index.column() == ContentTypesColumn)
                    return backupContentTypesToString(BackupContentType::archive);
                break;
            case Qt::ToolTipRole:
                if (index.column() == ContentTypesColumn)
                    return tr("Cannot be modified for new added cameras");
                break;
            case BackupEnabledRole:
                if (index.column() == SwitchColumn)
                    return globalBackupSettings().backupNewCameras;
                break;
            case BackupQualityRole:
                if (index.column() == QualityColumn)
                    return globalBackupSettings().quality;
                break;
            case HasDropdownMenuRole:
                if (!isDropdownColumn(index))
                    return {};
                return index.column() == QualityColumn;
            default:
                break;
        }
    }
    else if (!camera.isNull())
    {
        if (!isBackupSupported(camera))
        {
            if (index.column() == ContentTypesColumn)
            {
                if (role == Qt::DisplayRole)
                    return tr("Not supported");
                if (role == Qt::ToolTipRole)
                    return tr("Backup is not supported for this device");
            }
            return base_type::data(index, role);
        }

        switch (role)
        {
            case Qt::ToolTipRole:
                if (index.column() == QualityColumn && !camera->hasVideo())
                    return tr("Stream setting is not applicable to this device type");
                if (index.column() == QualityColumn && !camera->hasDualStreaming())
                    return tr("This device provides only one data stream");
                break;
            case BackupEnabledRole:
                return backupEnabled(camera);
            case BackupContentTypesRole:
                return static_cast<int>(backupContentTypes(camera));
            case BackupQualityRole:
                return static_cast<int>(backupQuality(camera));
            case HasDropdownMenuRole:
                if (!isDropdownColumn(index))
                    return {};
                return index.column() != QualityColumn || camera->hasDualStreaming();
            case NothingToBackupWarningRole:
                return nothingToBackupWarningData(index);
            case IsItemHighlightedRole:
                if (index.column() == ResourceColumn)
                    return backupEnabled(camera);
                break;

            default:
                break;
        }
    }

    return base_type::data(index, role);
}

BackupContentTypes BackupSettingsDecoratorModel::backupContentTypes(
    const QnVirtualCameraResourcePtr& camera) const
{
    if (m_changedContentTypes.contains(camera))
        return m_changedContentTypes.value(camera);

    return camera->getBackupContentType();
}

CameraBackupQuality BackupSettingsDecoratorModel::backupQuality(
    const QnVirtualCameraResourcePtr& camera) const
{
    if (!camera->hasDualStreaming())
        return CameraBackupQuality::CameraBackupBoth;

    if (m_changedQuality.contains(camera))
        return m_changedQuality.value(camera);

    return camera->getActualBackupQualities();
}

bool BackupSettingsDecoratorModel::backupEnabled(
    const QnVirtualCameraResourcePtr& camera) const
{
    if (m_changedEnabledState.contains(camera))
        return m_changedEnabledState.value(camera);

    return camera->isBackupEnabled();
}

QVariant BackupSettingsDecoratorModel::nothingToBackupWarningData(const QModelIndex& index) const
{
    if (index.column() != ContentTypesColumn)
        return {};

    const auto camera = cameraResource(index);
    if (camera.isNull() || !backupEnabled(camera))
        return {};

    const auto cameraExtraStatus = mapToSource(index).siblingAtColumn(ResourceColumn)
        .data(Qn::CameraExtraStatusRole).value<CameraExtraStatus>();

    if (!cameraExtraStatus.testFlag(CameraExtraStatusFlag::hasArchive)
        && !cameraExtraStatus.testFlag(CameraExtraStatusFlag::scheduled)
        && !cameraExtraStatus.testFlag(CameraExtraStatusFlag::recording))
    {
        return tr("The camera has neither recorded footage nor recording scheduled");
    }

    const auto cameraHasAnalytics = !camera->supportedObjectTypes().empty();
    const auto noMotionWarningText = tr("Motion detection is disabled");
    const auto noAnalyticsWarningText = tr("No analytics plugins");

    if (backupContentTypes(camera) == BackupContentTypes(BackupContentType::motion)
        && !camera->isMotionDetectionActive())
    {
        return noMotionWarningText;
    }

    if (backupContentTypes(camera) == BackupContentTypes(BackupContentType::analytics)
        && !cameraHasAnalytics)
    {
        return noAnalyticsWarningText;
    }

    if (backupContentTypes(camera) == BackupContentTypes(
        {BackupContentType::motion, BackupContentType::analytics})
            && (!camera->isMotionDetectionActive() && !cameraHasAnalytics))
    {
        return QStringList({noMotionWarningText, noAnalyticsWarningText}).join(QChar::LineFeed);
    }

    return {};
}

BackupSettings BackupSettingsDecoratorModel::globalBackupSettings() const
{
    return m_changedGlobalBackupSettings
        ? *m_changedGlobalBackupSettings
        : systemSettings()->backupSettings();
}

void BackupSettingsDecoratorModel::setGlobalBackupSettings(const BackupSettings& backupSettings)
{
    auto globalBackupSettingsChangesNotifier = makeGlobalBackupSettingsChangesNotifier(this);
    auto hasChangesNotifier = makeHasChangesNotifier(this);

    const auto savedGlobalBackupSettings = systemSettings()->backupSettings();
    if (savedGlobalBackupSettings == backupSettings)
        m_changedGlobalBackupSettings.reset();
    else
        m_changedGlobalBackupSettings = backupSettings;
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

        if (backupContentTypes(camera) != contentTypes)
        {
            m_changedContentTypes.insert(camera, contentTypes);
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
                setGlobalBackupSettings(backupSettings);
                dataChangedScopedAccumulator.rowDataChanged(index);
            }
        }
        else if (!camera.isNull())
        {
            if (!NX_ASSERT(isBackupSupported(camera)))
                continue;

            if (!camera->hasDualStreaming())
                continue;

            if (backupQuality(camera) != quality)
            {
                m_changedQuality.insert(camera, quality);
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
                setGlobalBackupSettings(backupSettings);
                dataChangedScopedAccumulator.rowDataChanged(index);
            }
        }
        else if (!camera.isNull())
        {
            if (!NX_ASSERT(isBackupSupported(camera)))
                continue;

            if (backupEnabled(camera) != enabled)
            {
                m_changedEnabledState.insert(camera, enabled);
                dataChangedScopedAccumulator.rowDataChanged(index);
            }
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

bool BackupSettingsDecoratorModel::hasChanges() const
{
    return !m_changedContentTypes.isEmpty()
        || !m_changedQuality.isEmpty()
        || !m_changedEnabledState.isEmpty()
        || m_changedGlobalBackupSettings;
}

void BackupSettingsDecoratorModel::discardChanges()
{
    auto hasChangesNotifier = makeHasChangesNotifier(this);

    m_changedContentTypes.clear();
    m_changedQuality.clear();
    m_changedEnabledState.clear();
    m_changedGlobalBackupSettings.reset();

    if (rowCount() > 0)
        emit dataChanged(index(0, ContentTypesColumn), index(rowCount() - 1, SwitchColumn));
}

void BackupSettingsDecoratorModel::applyChanges()
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

    if (m_changedGlobalBackupSettings)
    {
        systemSettings()->setBackupSettings(*m_changedGlobalBackupSettings);
        systemSettings()->synchronizeNow();
    }

    m_changedContentTypes.clear();
    m_changedQuality.clear();
    m_changedEnabledState.clear();
    m_changedGlobalBackupSettings.reset();
}

QnVirtualCameraResourceList BackupSettingsDecoratorModel::camerasToApplySettings() const
{
    QnVirtualCameraResourceList cameras =
        m_changedContentTypes.keys() + m_changedQuality.keys() + m_changedEnabledState.keys();

    if (m_changedGlobalBackupSettings)
    {
        cameras.append(resourcePool()->getAllCameras(QnResourcePtr(), /*ignoreDesktopCameras*/ true)
            .filtered(cameraHasDefaultBackupSettings));
    }

    std::sort(cameras.begin(), cameras.end());
    const auto newEnd = std::unique(cameras.begin(), cameras.end());
    cameras.erase(newEnd, cameras.end());

    return cameras.filtered(cameraHasNotRemovedFlag);
}

void BackupSettingsDecoratorModel::onRowsAboutToBeRemoved(
    const QModelIndex& parent, int first, int last)
{
    auto hasChangesNotifier = makeHasChangesNotifier(this);

    for (int row = first; row <= last; ++row)
    {
        if (const auto camera = cameraResource(index(row, ResourceColumn, parent)))
        {
            m_changedContentTypes.remove(camera);
            m_changedQuality.remove(camera);
            m_changedEnabledState.remove(camera);
        }
    }
}

} // namespace nx::vms::client::desktop
