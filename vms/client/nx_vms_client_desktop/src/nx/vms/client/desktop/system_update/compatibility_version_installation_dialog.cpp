#include <chrono>

#include <QtWidgets/QPushButton>

#include "compatibility_version_installation_dialog.h"
#include "ui_compatibility_version_installation_dialog.h"

#include <nx/update/update_check.h>
#include <nx/vms/client/desktop/system_update/client_update_tool.h>
#include <client/client_settings.h>
#include <nx/utils/log/log.h>
#include <nx/utils/guarded_callback.h>
#include "ui/workbench/handlers/workbench_connect_handler.h"

using namespace std::chrono_literals;
using ClientUpdateTool = nx::vms::client::desktop::ClientUpdateTool;
using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Dialog = CompatibilityVersionInstallationDialog;

namespace {
const auto kWaitForUpdateInfo = std::chrono::milliseconds(5);
}

struct CompatibilityVersionInstallationDialog::Private
{
    // Update info from mediaservers.
    std::future<UpdateContents> updateInfoMediaserver;
    // Update info from the internet.
    std::future<UpdateContents> updateInfoInternet;
    int requestsLeft = 0;
    bool checkingUpdates = false;

    std::shared_ptr<ClientUpdateTool> clientUpdateTool;

    Clock::time_point lastActionStamp;

    nx::update::UpdateContents updateContents;
};

CompatibilityVersionInstallationDialog::CompatibilityVersionInstallationDialog(
    const QnConnectionInfo& connectionInfo,
    QWidget* parent,
    const nx::vms::api::SoftwareVersion& engineVersion)
    :
    base_type(parent),
    m_ui(new Ui::QnCompatibilityVersionInstallationDialog),
    m_versionToInstall(connectionInfo.version),
    m_engineVersion(engineVersion),
    m_private(new Private())
{
    m_ui->setupUi(this);
    m_ui->autoRestart->setChecked(m_autoInstall);
    connect(m_ui->autoRestart, &QCheckBox::stateChanged,
        this, &CompatibilityVersionInstallationDialog::atAutoRestartChanged);

    m_private->clientUpdateTool.reset(new nx::vms::client::desktop::ClientUpdateTool(this));
    m_private->clientUpdateTool->setServerUrl(connectionInfo.ecUrl, QnUuid(connectionInfo.ecsGuid));
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
    m_private->clientUpdateTool->resetState();
    QDialog::reject();
}

int CompatibilityVersionInstallationDialog::exec()
{
    // Will do exec there
    return startUpdate();
}

void CompatibilityVersionInstallationDialog::processUpdateContents(
    const nx::update::UpdateContents& contents)
{
    auto commonModule = m_private->clientUpdateTool->commonModule();
    nx::update::UpdateContents verifiedContents = contents;
    if (nx::vms::client::desktop::verifyUpdateContents(commonModule, verifiedContents, {}))
    {
        //m_private->clientUpdateTool->setUpdateTarget(verifiedContents);
        if (m_private->updateContents.preferOtherUpdate(verifiedContents))
            m_private->updateContents = verifiedContents;
        else
        {
            NX_ERROR(this) << "processUpdateContents() rejected update information from" << contents.source;
        }
    }
    else
    {
        NX_ERROR(this) << "processUpdateContents() got invalid update contents from" << contents.source;
        //m_installationResult = InstallResult::failedDownload;
        //setMessage(tr("Installation failed"));
        //done(QDialogButtonBox::StandardButton::Ok);
    }
}

void CompatibilityVersionInstallationDialog::atUpdateStateChanged(int state, int progress)
{
    NX_DEBUG(this, "CompatibilityVersionInstallationDialog::atUpdateStateChanged(%1, %2)",
        ClientUpdateTool::toString(ClientUpdateTool::State(state)), progress);
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
            auto contents = m_private->clientUpdateTool->getRemoteUpdateInfo();
            m_private->clientUpdateTool->setUpdateTarget(contents);
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
            m_private->clientUpdateTool->installUpdateAsync();
            break;
        case ClientUpdateTool::State::installing:
            setMessage(tr("Installing"));
            m_installationResult = InstallResult::installing;
            finalProgress = 95;
            break;
        case ClientUpdateTool::State::readyRestart:
            m_installationResult = InstallResult::complete;
            setMessage(tr("Installation completed"));
            finalProgress = 100;
            done(QDialogButtonBox::StandardButton::Ok);
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

void CompatibilityVersionInstallationDialog::atUpdateCurrentState()
{
    if (m_private->checkingUpdates)
    {
        if (m_private->updateInfoInternet.valid()
            && m_private->updateInfoInternet.wait_for(kWaitForUpdateInfo) == std::future_status::ready)
        {
            NX_DEBUG(this, "atUpdateCurrentState() - done with updateInfoInternet");
            processUpdateContents(m_private->updateInfoInternet.get());
            --m_private->requestsLeft;
        }

        if (m_private->updateInfoMediaserver.valid()
            && m_private->updateInfoMediaserver.wait_for(kWaitForUpdateInfo) == std::future_status::ready)
        {
            NX_DEBUG(this, "atUpdateCurrentState() - done with updateInfoMediaserver");
            processUpdateContents(m_private->updateInfoMediaserver.get());
            --m_private->requestsLeft;
        }

        // Done waiting for update information
        if (m_private->requestsLeft == 0)
        {
            if (m_private->updateContents.isValidToInstall())
            {
                m_private->clientUpdateTool->setUpdateTarget(m_private->updateContents);
            }
            else
            {
                m_installationResult = InstallResult::failedDownload;
                setMessage(tr("Installation failed"));
                done(QDialogButtonBox::StandardButton::Ok);
            }
            m_private->checkingUpdates = false;
        }
    }

    m_private->clientUpdateTool->checkInternalState();
}

int CompatibilityVersionInstallationDialog::startUpdate()
{
    connect(m_private->clientUpdateTool, &ClientUpdateTool::updateStateChanged,
        this, &CompatibilityVersionInstallationDialog::atUpdateStateChanged);

    // Should request specific build from the internet.
    QString updateUrl = qnSettings->updateFeedUrl();
    QString build = QString::number(m_versionToInstall.build());

    m_private->checkingUpdates = true;
    m_private->updateInfoInternet = nx::update::checkSpecificChangeset(
        updateUrl, m_engineVersion, build);
    if (m_private->updateInfoInternet.valid())
        ++m_private->requestsLeft;
    // Checking update info from the mediaservers.
    m_private->updateInfoMediaserver = m_private->clientUpdateTool->requestRemoteUpdateInfo();
    if (m_private->updateInfoMediaserver.valid())
        ++m_private->requestsLeft;

    m_private->lastActionStamp = Clock::now();

    m_statusCheckTimer.reset(new QTimer(this));
    m_statusCheckTimer->setSingleShot(false);
    m_statusCheckTimer->start(1000);
    connect(m_statusCheckTimer.get(), &QTimer::timeout,
        this, &CompatibilityVersionInstallationDialog::atUpdateCurrentState);

    setMessage(tr("Installing version %1").arg(m_versionToInstall.toString()));
    m_ui->buttonBox->setStandardButtons(QDialogButtonBox::Cancel);

    int result = base_type::exec();
    return result;
}

void CompatibilityVersionInstallationDialog::setMessage(const QString& message)
{
    m_ui->progressBar->setFormat(QString("%1\t%p%").arg(message));
}
