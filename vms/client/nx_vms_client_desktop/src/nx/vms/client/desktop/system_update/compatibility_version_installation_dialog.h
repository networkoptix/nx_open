#pragma once

#include <QtWidgets/QDialog>

#include <utils/common/connective.h>
#include <update/updates_common.h>
#include <ui/dialogs/common/dialog.h>

#include <nx/utils/software_version.h>
#include <nx/vms/api/data/software_version.h>

class QnMediaServerUpdateTool;
struct QnConnectionInfo;

namespace Ui {
class QnCompatibilityVersionInstallationDialog;
}

namespace nx::update { struct UpdateContents; }
namespace nx::vms::client::desktop { class ClientUpdateTool; }

// TODO: #dklychkov rename class in 2.4
class CompatibilityVersionInstallationDialog:
    public Connective<QnDialog>
{
    Q_OBJECT
    using base_type = Connective<QnDialog>;

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
        installing,
        complete,
        failedInstall,
        failedDownload,
    };

    InstallResult installationResult() const;
    bool shouldAutoRestart() const;

    virtual void reject() override;
    virtual int exec() override;

protected:
    int installUpdate();
    void atUpdateStateChanged(int state, int progress);
    void atAutoRestartChanged(int state);
    void atReceivedUpdateContents(const nx::update::UpdateContents& contents);
    void setMessage(const QString& message);

    QScopedPointer<Ui::QnCompatibilityVersionInstallationDialog> m_ui;

    nx::utils::SoftwareVersion m_versionToInstall;
    std::shared_ptr<nx::vms::client::desktop::ClientUpdateTool> m_clientUpdateTool;
    // Timer for checking current status and timeouts.
    std::unique_ptr<QTimer> m_statusCheckTimer;

    InstallResult m_installationResult = InstallResult::initial;
    bool m_autoInstall = true;
    const nx::vms::api::SoftwareVersion m_engineVersion;
};
