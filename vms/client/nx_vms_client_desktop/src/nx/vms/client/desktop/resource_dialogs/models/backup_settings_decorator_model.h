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

class ServerSettingsDialogStore;
struct ServerSettingsDialogState;

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
    BackupSettingsDecoratorModel(ServerSettingsDialogStore* store, SystemContext* systemContext);
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
    bool isNetworkRequestRunning() const;

private:
    void loadState(const ServerSettingsDialogState& state);

signals:
    void hasChangesChanged();
    void globalBackupSettingsChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
