// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "compatibility_version_installation_dialog.h"
#include "ui_compatibility_version_installation_dialog.h"

#include <chrono>

#include <QtCore/QTimer>
#include <QtWidgets/QPushButton>

#include <client_core/client_core_module.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/core/network/logon_data.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection_error_strings.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/update/tools.h>
#include <nx/vms/update/update_check.h>
#include <ui/dialogs/common/message_box.h>

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
    core::LogonData logonData;

    std::future<UpdateContents> installedUpdateInfoMediaserver;
    std::future<UpdateContents> targetUpdateInfoMediaserver;
    std::future<UpdateContents> updateInfoInternet;
    std::future<ClientUpdateTool::SystemServersInfo> systemServersInfo;

    int requestsLeft = 0;
    bool checkingUpdates = false;

    core::RemoteConnectionFactory::ProcessPtr connectionProcess;

    std::shared_ptr<ClientUpdateTool> clientUpdateTool;

    Clock::time_point lastActionStamp;

    UpdateContents updateContents;
    ClientUpdateTool::SystemServersInfo serversInfo;
};

CompatibilityVersionInstallationDialog::CompatibilityVersionInstallationDialog(
    const nx::vms::api::ModuleInformation& moduleInformation,
    const nx::vms::client::core::LogonData& logonData,
    const nx::utils::SoftwareVersion& engineVersion,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::QnCompatibilityVersionInstallationDialog),
    m_versionToInstall(moduleInformation.version),
    m_engineVersion(engineVersion),
    m_private(new Private())
{
    ui->setupUi(this);
    ui->autoRestart->setChecked(m_autoInstall);
    connect(ui->autoRestart, &QCheckBox::stateChanged,
        this, &CompatibilityVersionInstallationDialog::atAutoRestartChanged);

    m_private->logonData = logonData;
    m_private->logonData.purpose = core::LogonData::Purpose::connectInCompatibilityMode;
    m_private->logonData.expectedServerId = moduleInformation.id;
    m_private->logonData.expectedServerVersion = moduleInformation.version;
    m_private->clientUpdateTool.reset(new nx::vms::client::desktop::ClientUpdateTool(
        appContext()->currentSystemContext(),
        this));
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
    NX_VERBOSE(this, "Connecting to the System %1...", m_private->logonData.address);
    const auto remoteConnectionFactory = qnClientCoreModule->networkModule()->connectionFactory();
    m_private->connectionProcess = remoteConnectionFactory->connect(
        m_private->logonData,
        nx::utils::guarded(this,
            [this](core::RemoteConnectionFactory::ConnectionOrError result)
            {
                if (auto error = std::get_if<core::RemoteConnectionError>(&result);
                    error && error->code != core::RemoteConnectionErrorCode::factoryServer)
                {
                    QnMessageBox::critical(
                        this,
                        tr("Cannot connect to the System"),
                        core::shortErrorDescription(error->code));

                    m_installationResult = InstallResult::failedDownload;
                    done(QDialogButtonBox::StandardButton::Ok);
                    return;
                }

                NX_VERBOSE(this, "Connected to the System %1", m_private->logonData.address);
                QMetaObject::invokeMethod(this,
                    &CompatibilityVersionInstallationDialog::startUpdate,
                    Qt::QueuedConnection);
            }));
    return base_type::exec();
}

void CompatibilityVersionInstallationDialog::processUpdateContents(const UpdateContents& contents)
{
    auto systemContext = m_private->clientUpdateTool->systemContext();
    auto verifiedContents = contents;
    // We need to supply non-zero Uuid for the client to make verification work.
    // commonModule->globalSettings()->localSystemId() is zero right now.
    ClientVerificationData clientData;
    clientData.clientId = nx::Uuid("cccccccc-cccc-cccc-cccc-cccccccccccc");
    clientData.fillDefault();
    VerificationOptions options;
    options.compatibilityMode = true;
    options.systemContext = systemContext;
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
        const auto processInfoReply =
            [this](std::future<UpdateContents>& future, const QString& logLabel)
            {
                if (!future.valid()
                    || future.wait_for(kWaitForUpdateInfo) != std::future_status::ready)
                {
                    return;
                }

                const auto updateContents = future.get();
                if (updateContents.info.version != m_versionToInstall)
                {
                    NX_DEBUG(this, "Update information from %1 mismatch. Need: %2, got: %3.",
                        logLabel, m_versionToInstall, updateContents.info.version);
                }
                else
                {
                    NX_DEBUG(this, "Got update information from %1", logLabel);
                    processUpdateContents(updateContents);
                }
                --m_private->requestsLeft;
            };

        processInfoReply(m_private->updateInfoInternet, "Internet");
        processInfoReply(m_private->installedUpdateInfoMediaserver, "Server (installed version)");
        processInfoReply(m_private->targetUpdateInfoMediaserver, "Server (target version)");

        if (m_private->systemServersInfo.valid()
            && m_private->systemServersInfo.wait_for(kWaitForUpdateInfo)
                == std::future_status::ready)
        {
            --m_private->requestsLeft;

            m_private->serversInfo = m_private->systemServersInfo.get();
        }

        // Done waiting for update information
        if (m_private->requestsLeft == 0)
        {
            if (m_private->updateContents.isValidToInstall())
            {
                m_private->clientUpdateTool->setupProxyConnections(m_private->serversInfo);
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

void CompatibilityVersionInstallationDialog::startUpdate()
{
    m_private->clientUpdateTool->setServerUrl(
        m_private->connectionProcess->context->logonData,
        m_private->connectionProcess->context->certificateCache.get());

    connect(m_private->clientUpdateTool.get(), &ClientUpdateTool::updateStateChanged,
        this, &CompatibilityVersionInstallationDialog::atUpdateStateChanged);

    const nx::utils::Url updateUrl = nx::vms::common::update::releaseListUrl(
        m_private->clientUpdateTool->systemContext());

    m_private->checkingUpdates = true;

    // Starting check for update manifest from the Internet.
    m_private->updateInfoInternet = m_private->clientUpdateTool->requestInternetUpdateInfo(
        updateUrl, nx::vms::update::CertainVersionParams{m_versionToInstall, m_engineVersion});
    if (m_private->updateInfoInternet.valid())
        ++m_private->requestsLeft;

    // Checking update info from the mediaservers.
    m_private->installedUpdateInfoMediaserver =
        m_private->clientUpdateTool->requestUpdateInfoFromServer(
            nx::vms::common::update::InstalledVersionParams{});
    if (m_private->installedUpdateInfoMediaserver.valid())
        ++m_private->requestsLeft;

    m_private->targetUpdateInfoMediaserver =
        m_private->clientUpdateTool->requestUpdateInfoFromServer(
            nx::vms::common::update::TargetVersionParams{});
    if (m_private->targetUpdateInfoMediaserver.valid())
        ++m_private->requestsLeft;

    m_private->serversInfo = {};
    m_private->systemServersInfo = std::async(
        std::launch::async,
        [this]()
        {
            return m_private->clientUpdateTool->getSystemServersInfo(m_versionToInstall);
        });
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
