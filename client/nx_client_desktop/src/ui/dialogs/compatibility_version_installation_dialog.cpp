#include "compatibility_version_installation_dialog.h"
#include "ui_compatibility_version_installation_dialog.h"

#include <QtWidgets/QPushButton>
#include <nx/client/desktop/system_update/client_update_tool.h>
#include "update/media_server_update_tool.h"
#include "ui/workbench/handlers/workbench_connect_handler.h"

#include <chrono>

using namespace std::chrono_literals;
using ClientUpdateTool = nx::client::desktop::ClientUpdateTool;
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
    m_clientUpdateTool.reset(new nx::client::desktop::ClientUpdateTool(this));
    m_clientUpdateTool->setServerUrl(connectionInfo.ecUrl, QnUuid(connectionInfo.ecsGuid));
}

CompatibilityVersionInstallationDialog::~CompatibilityVersionInstallationDialog()
{
}

bool CompatibilityVersionInstallationDialog::installationSucceeded() const
{
    return m_installationOk;
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

void CompatibilityVersionInstallationDialog::atUpdateStateChanged(int state, int progress)
{
    qDebug() << "CompatibilityVersionInstallationDialog::atUpdateStateChanged("
        << ClientUpdateTool::toString(ClientUpdateTool::State(state)) << "," << progress << ")";
    // Progress:
    // [0-20] - Requesting update info
    // [20-80] - Downloading
    // [80-100] - Installing
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
            auto contents = m_clientUpdateTool->getRemoteUpdateInfo();
            m_clientUpdateTool->downloadUpdate(contents);
            break;
        }
        case ClientUpdateTool::State::downloading:
            setMessage(tr("Downloading update package"));
            finalProgress = 20 + 60 * progress / 100;
            break;
        case ClientUpdateTool::State::readyInstall:
            finalProgress = 80;
            break;
        case ClientUpdateTool::State::installing:
            setMessage(tr("Installing"));
            finalProgress = 85;
            break;
        case ClientUpdateTool::State::complete:
            setMessage(tr("Installation completed"));
            finalProgress = 100;
            break;
        case ClientUpdateTool::State::error:
            setMessage(tr("Installation failed"));
            finalProgress = 100;
            break;
        case ClientUpdateTool::State::applauncherError:
            setMessage(tr("Installation failed"));
            finalProgress = 100;
            break;
    }

    m_ui->progressBar->setValue(finalProgress);
}

int CompatibilityVersionInstallationDialog::installUpdate()
{
    // TODO:
    // 1. Check update info from the mediaservers. Wait until info arrives or timeout occurs.
    // 2. Initiate downloading process.
    auto future = std::async(std::launch::async,
        [dialog = QPointer(this), updateTool = m_clientUpdateTool]()
        {
            auto updateInfo = updateTool->requestRemoteUpdateInfo();

            const auto waitForUpdateInfo = 20s;
            if (updateInfo.wait_for(waitForUpdateInfo) == std::future_status::timeout)
            {
                // No update. Just quit
                return false;
            }
            return true;
        });

    connect(m_clientUpdateTool, &ClientUpdateTool::updateStateChanged,
        this, &CompatibilityVersionInstallationDialog::atUpdateStateChanged);

    setMessage(tr("Installing version %1").arg(m_versionToInstall.toString()));
    m_ui->buttonBox->setStandardButtons(QDialogButtonBox::Cancel);

    //m_updateTool->startOnlineClientUpdate(m_versionToInstall);
    int result = base_type::exec();
    //m_updateTool.reset();
    return result;
}

void CompatibilityVersionInstallationDialog::setMessage(const QString& message)
{
    m_ui->progressBar->setFormat(QString("%1\t%p%").arg(message));
}
