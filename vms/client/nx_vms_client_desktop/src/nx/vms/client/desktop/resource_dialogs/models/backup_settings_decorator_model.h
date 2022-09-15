// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QIdentityProxyModel>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/backup_settings.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class BackupSettingsDecoratorModel:
    public QIdentityProxyModel,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QIdentityProxyModel;

public:
    using BackupContentTypes = nx::vms::api::BackupContentTypes;
    using CameraBackupQuality = nx::vms::api::CameraBackupQuality;

public:
    BackupSettingsDecoratorModel(SystemContext* systemContext);
    virtual ~BackupSettingsDecoratorModel() override;

    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    QnVirtualCameraResourcePtr cameraResource(const QModelIndex& index) const;

    nx::vms::api::BackupSettings globalBackupSettings() const;

    void setBackupContentTypes(const QModelIndexList& indexes, BackupContentTypes contentTypes);
    void setBackupQuality(const QModelIndexList& indexes, CameraBackupQuality quality);
    void setBackupEnabled(const QModelIndexList& indexes, bool enabled);

    bool hasChanges() const;
    void applyChanges();
    void discardChanges();

signals:
    void hasChangesChanged();
    void globalBackupSettingsChanged();

private:
    BackupContentTypes backupContentTypes(const QnVirtualCameraResourcePtr& camera) const;
    CameraBackupQuality backupQuality(const QnVirtualCameraResourcePtr& camera) const;
    bool backupEnabled(const QnVirtualCameraResourcePtr& camera) const;

    void setGlobalBackupSettings(const nx::vms::api::BackupSettings& backupSettings);

    QnVirtualCameraResourceList camerasToApplySettings() const;
    void onRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last);

    QVariant nothingToBackupWarningData(const QModelIndex& index) const;

private:
    QHash<QnVirtualCameraResourcePtr, BackupContentTypes> m_changedContentTypes;
    QHash<QnVirtualCameraResourcePtr, CameraBackupQuality> m_changedQuality;
    QHash<QnVirtualCameraResourcePtr, bool> m_changedEnabledState;
    std::optional<nx::vms::api::BackupSettings> m_changedGlobalBackupSettings;
};

} // namespace nx::vms::client::desktop
