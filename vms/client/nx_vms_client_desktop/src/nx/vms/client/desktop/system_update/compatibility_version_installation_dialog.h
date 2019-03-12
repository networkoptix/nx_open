#pragma once

#include <QtWidgets/QDialog>

#include <utils/common/connective.h>
#include <ui/dialogs/common/dialog.h>

#include <nx/utils/software_version.h>
#include <nx/vms/api/data/software_version.h>

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
    int startUpdate();
    void processUpdateContents(const nx::update::UpdateContents& contents);
    void setMessage(const QString& message);

protected:
    // Event handlers
    void atUpdateCurrentState();
    void atUpdateStateChanged(int state, int progress);
    void atAutoRestartChanged(int state);

protected:
    QScopedPointer<Ui::QnCompatibilityVersionInstallationDialog> m_ui;

    nx::utils::SoftwareVersion m_versionToInstall;
    // Timer for checking current status and timeouts.
    std::unique_ptr<QTimer> m_statusCheckTimer;

    InstallResult m_installationResult = InstallResult::initial;
    bool m_autoInstall = true;
    const nx::vms::api::SoftwareVersion m_engineVersion;

private:
    struct Private;
    std::unique_ptr<Private> m_private;
};
