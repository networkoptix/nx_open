// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <core/resource/resource_fwd.h>

namespace Ui { class BackupSettingsWidget; }

namespace nx::vms::client::desktop {

class BackupSettingsViewWidget;

class BackupSettingsWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    BackupSettingsWidget(QWidget* parent = nullptr);
    virtual ~BackupSettingsWidget() override;

    void setServer(const QnMediaServerResourcePtr& server);

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual void discardChanges() override;

signals:
    void storageManagementShortcutClicked();

private:
    void setupPlaceholderPageAppearance();
    void updateBackupSettingsAvailability();
    void updateMessageBarText();

private:
    const std::unique_ptr<Ui::BackupSettingsWidget> ui;
    BackupSettingsViewWidget* m_backupSettingsViewWidget;
    QnMediaServerResourcePtr m_server;
};

} // namespace nx::vms::client::desktop
