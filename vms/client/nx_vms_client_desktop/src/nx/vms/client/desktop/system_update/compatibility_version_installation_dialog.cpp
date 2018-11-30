#include "compatibility_version_installation_dialog.h"
#include "ui_compatibility_version_installation_dialog.h"

#include <QtWidgets/QPushButton>

#include <nx/update/update_check.h>
#include <nx/vms/client/desktop/system_update/client_update_tool.h>
#include <client/client_settings.h>
#include <nx/utils/log/log.h>
#include <nx/utils/guarded_callback.h>
#include "update/media_server_update_tool.h"
#include "ui/workbench/handlers/workbench_connect_handler.h"


#include <chrono>

using namespace std::chrono_literals;
using ClientUpdateTool = nx::vms::client::desktop::ClientUpdateTool;
using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Dialog = CompatibilityVersionInstallationDialog;

CompatibilityVersionInstallationDialog::CompatibilityVersionInstallationDialog(
    const QnConnectionInfo& connectionInfo,
    QWidget* parent)
    :
    base_type(parent),
    m_ui(new Ui::QnCompatibilityVersionInstallationDialog),
    m_versionToInstall(connectionInfo.version)
{
    m_ui->setupUi(this);
    m_ui->autoRestart->setChecked(m_autoInstall);
    connect(m_ui->autoRestart, &QCheckBox::stateChanged,
        this, &CompatibilityVersionInstallationDialog::atAutoRestartChanged);
    m_clientUpdateTool.reset(new nx::vms::client::desktop::ClientUpdateTool(this));
    m_clientUpdateTool->setServerUrl(connectionInfo.ecUrl, QnUuid(connectionInfo.ecsGuid));
}

CompatibilityVersionInstallationDialog::~CompatibilityVersionInstallationDialog()
{
}

void CompatibilityVersionInstallationDialog::atAutoRestartChanged(int state)
{
    m_autoInstall = (state == Qt::Checked);
}

CompatibilityVersionInstallationDialog::InstallResult CompatibilityVersionInstallationDialog::installationResult() const
{
    return m_installationResult;
}

bool CompatibilityVersionInstallationDialog::shouldAutoRestart() const
{
    return m_autoInstall;
}

void CompatibilityVersionInstallationDialog::reject()
{
    m_clientUpdateTool->resetState();
    QDialog::reject();
}

int CompatibilityVersionInstallationDialog::exec()
{
    // Will do exec there
    return installUpdate();
}

void CompatibilityVersionInstallationDialog::atReceivedUpdateContents(
    const nx::update::UpdateContents& contents)
{
    auto commonModule = m_clientUpdateTool->commonModule();
    nx::update::UpdateContents verifiedContents = contents;
    if (nx::vms::client::desktop::verifyUpdateContents(commonModule, verifiedContents, {}))
    {
        m_clientUpdateTool->downloadUpdate(verifiedContents);
    }
    else
    {
        NX_ERROR(this) << "atRecievedUpdateContents() got invalid update contents from" << contents.source;
        m_installationResult = InstallResult::failedDownload;
        setMessage(tr("Installation failed"));
        done(QDialogButtonBox::StandardButton::Ok);
    }
}

void CompatibilityVersionInstallationDialog::atUpdateStateChanged(int state, int progress)
{
    qDebug() << "CompatibilityVersionInstallationDialog::atUpdateStateChanged("
        << ClientUpdateTool::toString(ClientUpdateTool::State(state)) << "," << progress << ")";
    // Progress:
    // [0-20] - Requesting update info
    // [20-90] - Downloading
    // [90-100] - Installing
    int finalProgress = 0;
    switch (ClientUpdateTool::State(state))
    {
        case ClientUpdateTool::State::initial:
            break;
        case ClientUpdateTool::State::pendingUpdateInfo:
            setMessage(tr("Getting update information from the server"));
            finalProgress = 10;
            break;
        case ClientUpdateTool::State::readyDownload:
        {
            finalProgress = 20;
            setMessage(tr("Downloading update package"));
            auto contents = m_clientUpdateTool->getRemoteUpdateInfo();
            m_clientUpdateTool->downloadUpdate(contents);
            break;
        }
        case ClientUpdateTool::State::downloading:
            m_installationResult = InstallResult::downloading;
            setMessage(tr("Downloading update package"));
            finalProgress = 20 + 70 * progress / 100;
            break;
        case ClientUpdateTool::State::readyInstall:
            finalProgress = 90;
            // TODO: We should wrap it inside some thread. This call can be long.
            m_clientUpdateTool->installUpdate();
            break;
        case ClientUpdateTool::State::installing:
            setMessage(tr("Installing"));
            m_installationResult = InstallResult::installing;
            finalProgress = 95;
            break;
        case ClientUpdateTool::State::complete:
            m_installationResult = InstallResult::complete;
            setMessage(tr("Installation completed"));
            finalProgress = 100;
            done(QDialogButtonBox::StandardButton::Ok);
            break;
        case ClientUpdateTool::State::error:
            if (m_installationResult == InstallResult::installing)
                m_installationResult = InstallResult::failedInstall;
            else
                m_installationResult = InstallResult::failedDownload;
            setMessage(tr("Installation failed"));
            finalProgress = 100;
            done(QDialogButtonBox::StandardButton::Ok);
            break;
        case ClientUpdateTool::State::applauncherError:
            m_installationResult = InstallResult::failedInstall;
            setMessage(tr("Installation failed"));
            finalProgress = 100;
            done(QDialogButtonBox::StandardButton::Ok);
            break;
    }

    m_ui->progressBar->setValue(finalProgress);
}

int CompatibilityVersionInstallationDialog::installUpdate()
{
    connect(m_clientUpdateTool, &ClientUpdateTool::updateStateChanged,
        this, &CompatibilityVersionInstallationDialog::atUpdateStateChanged);

    // Should request specific build from the internet
    QString updateUrl = qnSettings->updateFeedUrl();
    auto callback = nx::utils::guarded(this,
        [this](const nx::update::UpdateContents& contents)
        {
            this->atReceivedUpdateContents(contents);
        });
    QString build = QString::number(m_versionToInstall.build());
    // Callback will be called on this thread.
    auto future = nx::update::checkSpecificChangeset(updateUrl, build, callback);

    setMessage(tr("Installing version %1").arg(m_versionToInstall.toString()));
    m_ui->buttonBox->setStandardButtons(QDialogButtonBox::Cancel);

    int result = base_type::exec();
    return result;
}

void CompatibilityVersionInstallationDialog::setMessage(const QString& message)
{
    m_ui->progressBar->setFormat(QString("%1\t%p%").arg(message));
}
