#pragma once

#include <QtWidgets/QDialog>

#include <utils/common/connective.h>
#include <update/updates_common.h>
#include <ui/dialogs/common/dialog.h>

#include <nx/utils/software_version.h>

class QnMediaServerUpdateTool;
class QnConnectionInfo;

namespace Ui {
class QnCompatibilityVersionInstallationDialog;
}

namespace nx::client::desktop { class ClientUpdateTool; }

// TODO: #dklychkov rename class in 2.4
class CompatibilityVersionInstallationDialog: public Connective<QnDialog>
{
    Q_OBJECT
    using base_type = Connective<QnDialog>;

public:
    CompatibilityVersionInstallationDialog(
        const QnConnectionInfo &connectionInfo, QWidget* parent = nullptr);
    virtual ~CompatibilityVersionInstallationDialog();

    bool installationSucceeded() const;

    int installUpdate();
    virtual void reject() override;
    virtual int exec() override;

protected:
    void atUpdateStateChanged(int state, int progress);
    void setMessage(const QString& message);

    QScopedPointer<Ui::QnCompatibilityVersionInstallationDialog> m_ui;

    nx::utils::SoftwareVersion m_versionToInstall;
    std::shared_ptr<nx::client::desktop::ClientUpdateTool> m_clientUpdateTool;
    // Timer for checking current status and timeouts.
    std::unique_ptr<QTimer> m_statusCheckTimer;
    bool m_installationOk = false;
};
