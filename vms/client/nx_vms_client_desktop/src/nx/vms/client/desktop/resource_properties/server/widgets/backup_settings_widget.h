// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <core/resource/resource_fwd.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class BackupSettingsWidget; }

namespace nx::vms::client::desktop {

class ServerSettingsDialogStore;
struct ServerSettingsDialogState;
class BackupSettingsViewWidget;

class BackupSettingsWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    BackupSettingsWidget(ServerSettingsDialogStore* store, QWidget* parent = nullptr);
    virtual ~BackupSettingsWidget() override;

    void setServer(const QnMediaServerResourcePtr& server);

    virtual bool hasChanges() const override;
    virtual void applyChanges() override;
    virtual void discardChanges() override;
    virtual bool isNetworkRequestRunning() const override;

signals:
    void storageManagementShortcutClicked();

private:
    void loadState(const ServerSettingsDialogState& state);

    void setupPlaceholders();

private:
    const std::unique_ptr<Ui::BackupSettingsWidget> ui;
    BackupSettingsViewWidget* m_backupSettingsViewWidget;
    QnMediaServerResourcePtr m_server;
};

} // namespace nx::vms::client::desktop
