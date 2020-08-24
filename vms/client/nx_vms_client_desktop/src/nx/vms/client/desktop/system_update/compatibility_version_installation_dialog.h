#pragma once

#include <QtWidgets/QDialog>

#include <utils/common/connective.h>
#include <ui/dialogs/common/dialog.h>

#include <nx/utils/software_version.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/vms/client/desktop/system_update/client_update_tool.h>

class QnMediaServerUpdateTool;
struct QnConnectionInfo;

namespace Ui {
class QnCompatibilityVersionInstallationDialog;
}

namespace nx::update { struct UpdateContents; }

// TODO: #dklychkov rename class in 2.4
class CompatibilityVersionInstallationDialog:
    public Connective<QnDialog>
{
    Q_OBJECT
    using base_type = Connective<QnDialog>;
    using UpdateContents = nx::update::UpdateContents;

public:
    CompatibilityVersionInstallationDialog(
        const QnConnectionInfo& connectionInfo, QWidget* parent,
        const nx::vms::api::SoftwareVersion& engineVersion);
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

protected:
    int startUpdate();
    void processUpdateContents(const nx::update::UpdateContents& contents);
    void setMessage(const QString& message);

protected:
    // Event handlers
    void atUpdateCurrentState();
    void atUpdateStateChanged(
        int state, int progress, const nx::vms::client::desktop::ClientUpdateTool::Error& error);
    void atAutoRestartChanged(int state);

protected:
    QScopedPointer<Ui::QnCompatibilityVersionInstallationDialog> m_ui;

    nx::utils::SoftwareVersion m_versionToInstall;
    // Timer for checking current status and timeouts.
    std::unique_ptr<QTimer> m_statusCheckTimer;

    InstallResult m_installationResult = InstallResult::initial;
    bool m_autoInstall = true;
    const nx::vms::api::SoftwareVersion m_engineVersion;
    QString m_errorMessage;

private:
    struct Private;
    std::unique_ptr<Private> m_private;
};
