// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <api/server_rest_connection_fwd.h>
#include <nx/vms/api/data/watermark_settings.h>
#include <nx/vms/client/desktop/system_administration/widgets/abstract_system_settings_widget.h>

namespace Ui { class SecuritySettingsWidget; }

namespace nx::vms::client::desktop {

class RepeatedPasswordDialog;

class SecuritySettingsWidget: public AbstractSystemSettingsWidget
{
    Q_OBJECT

public:
    explicit SecuritySettingsWidget(
        api::SaveableSystemSettings* editableSystemSettings, QWidget* parent = nullptr);
    virtual ~SecuritySettingsWidget() override;

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual void discardChanges() override;
    virtual bool hasChanges() const override;
    virtual bool isNetworkRequestRunning() const override;

    void resetWarnings();
    void resetChanges();

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
    std::chrono::seconds calculateSessionLimit() const;
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
