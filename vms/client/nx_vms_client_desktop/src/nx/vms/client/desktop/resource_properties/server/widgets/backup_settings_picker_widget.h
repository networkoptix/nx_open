// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QModelIndexList>
#include <QtWidgets/QWidget>

#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/types/resource_types.h>

namespace Ui { class BackupSettingsPickerWidget; }

class QMenu;
class QHeaderView;

namespace nx::vms::client::desktop {

class ServerSettingsDialogStore;
struct ServerSettingsDialogState;

class BackupSettingsPickerWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    BackupSettingsPickerWidget(ServerSettingsDialogStore* store, QWidget* parent = nullptr);
    virtual ~BackupSettingsPickerWidget() override;

    static QString backupContentTypesPlaceholderText();
    static QString backupQualityPlaceholderText();

    void setupFromSelection(const QModelIndexList& indexes);
    void setupLayoutSyncWithHeaderView(const QHeaderView* headerView);

signals:
    void backupContentTypesPicked(nx::vms::api::BackupContentTypes contentTypes);
    void backupQualityPicked(nx::vms::api::CameraBackupQuality quality);
    void backupEnabledChanged(bool enabled);

private:
    void loadState(const ServerSettingsDialogState& state);
    void setupContentTypesDropdown(bool isCloudBackupStorage);
    void setupQualityDropdown();

private:
    const std::unique_ptr<Ui::BackupSettingsPickerWidget> ui;
    std::unique_ptr<QMenu> m_contentTypesDropdownMenu;
    std::unique_ptr<QMenu> m_qualityDropdownMenu;
};

} // namespace nx::vms::client::desktop
