// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "compatibility_version_installation_dialog.h"
#include "ui_compatibility_version_installation_dialog.h"

#include <chrono>

#include <QtCore/QTimer>
#include <QtWidgets/QPushButton>

#include <client/client_settings.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/core/network/connection_info.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/common/update/tools.h>
#include <nx/vms/update/update_check.h>

#include "update_verification.h"

using namespace std::chrono;
using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

namespace {
const auto kWaitForUpdateInfo = std::chrono::milliseconds(5);
}

struct CompatibilityVersionInstallationDialog::Private
{
    core::ConnectionInfo connectionInfo;
    QnUuid expectedServerId;

    // Update info from mediaservers.
    std::future<UpdateContents> updateInfoMediaserver;
    // Update info from the internet.
    std::future<UpdateContents> updateInfoInternet;
    int requestsLeft = 0;
    bool checkingUpdates = false;

    core::RemoteConnectionFactory::ProcessPtr connectionProcess;

    std::shared_ptr<ClientUpdateTool> clientUpdateTool;

    Clock::time_point lastActionStamp;

    UpdateContents updateContents;
};

CompatibilityVersionInstallationDialog::CompatibilityVersionInstallationDialog(
    const nx::vms::api::ModuleInformation& moduleInformation,
    const nx::vms::client::core::ConnectionInfo& connectionInfo,
    const nx::vms::api::SoftwareVersion& engineVersion,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::QnCompatibilityVersionInstallationDialog),
    m_versionToInstall(moduleInformation.version),
    m_engineVersion(engineVersion),
    m_private(new Private{
        .connectionInfo = connectionInfo,
        .expectedServerId = moduleInformation.id})
{
    ui->setupUi(this);
    ui->autoRestart->setChecked(m_autoInstall);
    connect(ui->autoRestart, &QCheckBox::stateChanged,
        this, &CompatibilityVersionInstallationDialog::atAutoRestartChanged);

    m_private->clientUpdateTool.reset(new nx::vms::client::desktop::ClientUpdateTool(this));
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
    NX_VERBOSE(this, "Connecting to the System %1...", m_private->connectionInfo.address);
    const auto remoteConnectionFactory = qnClientCoreModule->networkModule()->connectionFactory();
    m_private->connectionProcess = remoteConnectionFactory->connect(
        m_private->connectionInfo,
        m_private->expectedServerId,
        nx::utils::guarded(this,
            [this](core::RemoteConnectionFactory::ConnectionOrError result)
            {
                if (auto error = std::get_if<core::RemoteConnectionError>(&result))
                {
                    m_installationResult = InstallResult::failedDownload;
                    setMessage(tr("Cannot connect to the System"));
                    done(QDialogButtonBox::StandardButton::Ok);
                    return;
                }

                NX_VERBOSE(this, "Connected to the System %1", m_private->connectionInfo.address);
                QMetaObject::invokeMethod(this,
                    &CompatibilityVersionInstallationDialog::startUpdate,
                    Qt::QueuedConnection);
            }),
        nx::vms::common::ServerCompatibilityValidator::Purpose::connectInCompatibilityMode);
    return base_type::exec();
}

void CompatibilityVersionInstallationDialog::processUpdateContents(const UpdateContents& contents)
{
    auto commonModule = m_private->clientUpdateTool->commonModule();
    auto verifiedContents = contents;
    // We need to supply non-zero Uuid for the client to make verification work.
    // commonModule->globalSettings()->localSystemId() is zero right now.
    ClientVerificationData clientData;
    clientData.clientId = QnUuid("cccccccc-cccc-cccc-cccc-cccccccccccc");
    clientData.fillDefault();
    VerificationOptions options;
    options.compatibilityMode = true;
    options.commonModule = commonModule;
    options.downloadAllPackages = false;

    if (verifyUpdateContents(verifiedContents, /*servers=*/{}, clientData, options))
    {
        if (m_private->updateContents.sourceType != UpdateSourceType::internetSpecific
            && m_private->updateContents.preferOtherUpdate(verifiedContents))
        {
            m_private->updateContents = verifiedContents;
        }
        else
        {
            NX_DEBUG(this, "processUpdateContents() rejected update information from %1",
                contents.source);
        }
    }
    else
    {
        NX_ERROR(this, "processUpdateContents() got invalid update contents from %1",
            contents.source);
    }
}

void CompatibilityVersionInstallationDialog::atUpdateStateChanged(
    ClientUpdateTool::State state, int progress, const ClientUpdateTool::Error& error)
{
    NX_DEBUG(this, "CompatibilityVersionInstallationDialog::atUpdateStateChanged(%1, %2)",
        ClientUpdateTool::toString(state), progress);
    // Progress:
    // [0-20] - Requesting update info
    // [20-90] - Downloading
    // [90-100] - Installing
    int finalProgress = 0;
    switch (state)
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
            break;
        }
        case ClientUpdateTool::State::downloading:
            m_installationResult = InstallResult::downloading;
            setMessage(tr("Downloading update package"));
            finalProgress = 20 + 65 * progress / 100;
            break;
        case ClientUpdateTool::State::verifying:
            m_installationResult = InstallResult::verifying;
            setMessage(tr("Verifying update package"));
            finalProgress = 85;
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
            m_errorMessage = ClientUpdateTool::errorString(error);
            finalProgress = 100;
            done(QDialogButtonBox::StandardButton::Ok);
            break;
        case ClientUpdateTool::State::exiting:
            break;
    }

    ui->progressBar->setValue(finalProgress);
}

QString CompatibilityVersionInstallationDialog::errorString() const
{
    return m_errorMessage;
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
            auto updateContents = m_private->updateInfoMediaserver.get();
            if (updateContents.info.version != m_versionToInstall)
            {
                NX_DEBUG(this, "atUpdateCurrentState() - updateInfoMediaserver mismatch. "
                    "We need %1 but server returned %2.",
                    m_versionToInstall, updateContents.info.version);
            }
            else
            {
                NX_DEBUG(this, "atUpdateCurrentState() - done with updateInfoMediaserver");
                processUpdateContents(updateContents);
            }
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
    m_private->clientUpdateTool->checkServersInSystem();
}

void CompatibilityVersionInstallationDialog::startUpdate()
{
    m_private->clientUpdateTool->setServerUrl(
        m_private->expectedServerId,
        m_private->connectionProcess->context->info,
        m_private->connectionProcess->context->certificateCache.get());
    connect(m_private->clientUpdateTool, &ClientUpdateTool::updateStateChanged,
        this, &CompatibilityVersionInstallationDialog::atUpdateStateChanged);

    const QString updateUrl = nx::vms::common::update::updateFeedUrl();

    m_private->checkingUpdates = true;
    // Starting check for update manifest from the internet.
    m_private->updateInfoInternet = m_private->clientUpdateTool->requestInternetUpdateInfo(
        updateUrl, nx::vms::update::CertainVersionParams{m_versionToInstall, m_engineVersion});
    if (m_private->updateInfoInternet.valid())
        ++m_private->requestsLeft;
    // Checking update info from the mediaservers.
    m_private->updateInfoMediaserver = m_private->clientUpdateTool->requestUpdateInfoFromServer(
        nx::vms::common::update::InstalledVersionParams{});
    if (m_private->updateInfoMediaserver.valid())
        ++m_private->requestsLeft;

    m_private->lastActionStamp = Clock::now();

    m_statusCheckTimer.reset(new QTimer(this));
    m_statusCheckTimer->setSingleShot(false);
    m_statusCheckTimer->start(1000);
    connect(m_statusCheckTimer.get(), &QTimer::timeout,
        this, &CompatibilityVersionInstallationDialog::atUpdateCurrentState);

    setMessage(tr("Installing version %1").arg(m_versionToInstall.toString()));
    ui->buttonBox->setStandardButtons(QDialogButtonBox::Cancel);
}

void CompatibilityVersionInstallationDialog::setMessage(const QString& message)
{
    ui->progressBar->setFormat(QString("%1\t%p%").arg(message));
}
