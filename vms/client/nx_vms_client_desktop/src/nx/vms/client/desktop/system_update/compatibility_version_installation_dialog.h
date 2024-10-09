// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QDialog>

#include <nx/utils/software_version.h>
#include <nx/vms/client/desktop/system_update/client_update_tool.h>
#include <ui/dialogs/common/dialog.h>

namespace Ui { class QnCompatibilityVersionInstallationDialog; }

namespace nx::vms::api { struct ModuleInformation; }
namespace nx::vms::client::core { struct LogonData; }
namespace nx::vms::client::desktop { struct UpdateContents; }

// TODO: #dklychkov rename class in 2.4
class CompatibilityVersionInstallationDialog: public QnDialog
{
    Q_OBJECT
    using base_type = QnDialog;

public:
    CompatibilityVersionInstallationDialog(
        const nx::vms::api::ModuleInformation& moduleInformation,
        const nx::vms::client::core::LogonData& logonData,
        const nx::utils::SoftwareVersion& engineVersion,
        QWidget* parent);
    virtual ~CompatibilityVersionInstallationDialog();

    // Mirroring some states from ClientUpdateTool.
    enum class InstallResult
    {
        initial,
        downloading,
        verifying,
        installing,
        complete,
        failedInstall,
        failedDownload,
    };

    InstallResult installationResult() const;
    bool shouldAutoRestart() const;

    virtual void reject() override;
    virtual int exec() override;
    QString errorString() const;

private:
    void connectToSite();
    void startUpdate();
    void processUpdateContents(const nx::vms::client::desktop::UpdateContents& contents);
    void setMessage(const QString& message);

    // Event handlers
    void atUpdateCurrentState();
    void atUpdateStateChanged(nx::vms::client::desktop::ClientUpdateTool::State state,
        int progress,
        const nx::vms::client::desktop::ClientUpdateTool::Error& error);
    void atAutoRestartChanged(int state);

private:
    QScopedPointer<Ui::QnCompatibilityVersionInstallationDialog> ui;

    nx::utils::SoftwareVersion m_versionToInstall;
    // Timer for checking current status and timeouts.
    std::unique_ptr<QTimer> m_statusCheckTimer;

    InstallResult m_installationResult = InstallResult::initial;
    bool m_autoInstall = true;
    const nx::utils::SoftwareVersion m_engineVersion;
    QString m_errorMessage;

    struct Private;
    std::unique_ptr<Private> m_private;
};
