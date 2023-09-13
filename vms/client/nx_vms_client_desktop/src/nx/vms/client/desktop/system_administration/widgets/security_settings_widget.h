// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <api/server_rest_connection_fwd.h>
#include <nx/vms/api/data/watermark_settings.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class SecuritySettingsWidget; }

namespace nx::vms::client::desktop {

class RepeatedPasswordDialog;

class SecuritySettingsWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    explicit SecuritySettingsWidget(QWidget* parent);
    virtual ~SecuritySettingsWidget() override;

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual void discardChanges() override;
    virtual bool hasChanges() const override;
    virtual bool isNetworkRequestRunning() const override;

    void resetWarnings();

signals:
    void manageUsers();

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;
    virtual void showEvent(QShowEvent* event) override;

private:
    enum class ArchivePasswordState
    {
        notSet,
        set,
        changed,
        failedToSet,
    };

private:
    std::optional<std::chrono::seconds> calculateSessionLimit() const;
    void showArchiveEncryptionPasswordDialog(bool viaButton);
    void updateLimitSessionControls();

    void loadEncryptionSettingsToUi();
    void loadUserInfoToUi();

private:
    QScopedPointer<Ui::SecuritySettingsWidget> ui;
    api::WatermarkSettings m_watermarkSettings;
    RepeatedPasswordDialog* const m_archiveEncryptionPasswordDialog;
    ArchivePasswordState m_archivePasswordState = ArchivePasswordState::notSet;
    bool m_archiveEncryptionPasswordDialogOpenedViaButton = false;
    rest::Handle m_currentRequest = 0;
};

} // namespace nx::vms::client::desktop
