// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_testing_dialog.h"
#include "ui_connection_testing_dialog.h"

#include <QtWidgets/QPushButton>

#include <client_core/client_core_module.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/nx_network_ini.h>
#include <nx/network/socket_global.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection_error_strings.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/common/html/html.h>
#include <utils/connection_diagnostics_helper.h>

#include "../logic/connection_delegate_helper.h"

using namespace nx::vms::client::core;

namespace nx::vms::client::desktop {

namespace {

const int timerIntervalMs = 100;
const int connectionTimeoutMs = 30 * 1000;

struct DiagnosticResult
{
    enum class Status
    {
        /** Test is not started. */
        notStarted,

        /** System is connectable. */
        success,

        /** Connection error. */
        error,

        /** Request timeout. */
        timeout,

        /** Target server is not set up yet. */
        factoryServer,
    };

    Status status = Status::notStarted;
    std::optional<RemoteConnectionError> error;
    RemoteConnectionPtr connection;
    QnUuid newServerId;
};

HelpTopic::Id helpTopicId(RemoteConnectionErrorCode errorCode)
{
    switch (errorCode)
    {
        case RemoteConnectionErrorCode::cloudHostDiffers:
        case RemoteConnectionErrorCode::versionIsTooLow:
        case RemoteConnectionErrorCode::binaryProtocolVersionDiffers:
            return HelpTopic::Id::VersionMismatch;
        case RemoteConnectionErrorCode::factoryServer:
            return HelpTopic::Id::Setup_Wizard;
        default:
            return HelpTopic::Id::Login;
    }
}

/** In some cases error message is misleading, though it can be shown to developers only. */
bool alwaysShowDeveloperError(
    RemoteConnectionErrorCode errorCode,
    const nx::vms::api::ModuleInformation& targetSystem)
{
    if (errorCode != RemoteConnectionErrorCode::binaryProtocolVersionDiffers
        && errorCode != RemoteConnectionErrorCode::cloudHostDiffers)
    {
        return false; // Other error codes are self-explanatory for developers.
    }

    if (nx::build_info::publicationType() == nx::build_info::PublicationType::local)
        return true;

    return appContext()->version() == targetSystem.version
        && nx::branding::customization() == targetSystem.customization;
}

} // namespace

struct ConnectionTestingDialog::Private
{
    nx::utils::SoftwareVersion engineVersion;
    RemoteConnectionFactory::ProcessPtr connectionProcess;
    DiagnosticResult result;
    QPushButton* connectButton = nullptr;
    QPushButton* setupNewServerButton = nullptr;
    std::unique_ptr<QTimer> timeoutTimer = std::make_unique<QTimer>();
    std::unique_ptr<Ui::ConnectionTestingDialog> ui =
        std::make_unique<Ui::ConnectionTestingDialog>();

    auto pauseTimer()
    {
        timeoutTimer->stop();
        ui->progressBar->setValue(0);
        return nx::utils::ScopeGuard([this]() { timeoutTimer->start();} );
    }

    class ConnectionUserInteractionDelegate:
        public core::AbstractRemoteConnectionUserInteractionDelegate
    {
        using base_type = core::AbstractRemoteConnectionUserInteractionDelegate;

    public:
        ConnectionUserInteractionDelegate(ConnectionTestingDialog* owner):
            m_owner(owner),
            m_baseDelegate(createConnectionUserInteractionDelegate(owner))
        {
        }

        virtual bool acceptNewCertificate(
            const nx::vms::api::ModuleInformation& target,
            const nx::network::SocketAddress& primaryAddress,
            const nx::network::ssl::CertificateChain& chain) override
        {
            if (!m_owner)
                return false;

            auto guard = m_owner->d->pauseTimer();
            return m_baseDelegate->acceptNewCertificate(target, primaryAddress, chain);
        }

        virtual bool acceptCertificateAfterMismatch(
            const nx::vms::api::ModuleInformation& target,
            const nx::network::SocketAddress& primaryAddress,
            const nx::network::ssl::CertificateChain& chain) override
        {
            if (!m_owner)
                return false;

            auto guard = m_owner->d->pauseTimer();
            return m_baseDelegate->acceptCertificateAfterMismatch(target, primaryAddress, chain);
        }

        virtual bool acceptCertificateOfServerInTargetSystem(
            const nx::vms::api::ModuleInformation& target,
            const nx::network::SocketAddress& primaryAddress,
            const nx::network::ssl::CertificateChain& chain) override
        {
            if (!m_owner)
                return false;

            auto guard = m_owner->d->pauseTimer();
            return m_baseDelegate->acceptCertificateOfServerInTargetSystem(
                target,
                primaryAddress,
                chain);
        }

        virtual bool request2FaValidation(
            const nx::network::http::Credentials& credentials) override
        {
            if (!m_owner)
                return false;

            auto guard = m_owner->d->pauseTimer();
            return m_baseDelegate->request2FaValidation(credentials);
        }

    private:
        QPointer<ConnectionTestingDialog> m_owner;
        std::unique_ptr<core::AbstractRemoteConnectionUserInteractionDelegate> m_baseDelegate;
    };
};

ConnectionTestingDialog::ConnectionTestingDialog(QWidget* parent):
    QnButtonBoxDialog(parent),
    d(new Private{})
{
    d->ui->setupUi(this);
    setHelpTopic(this, HelpTopic::Id::Login);

    d->ui->buttonBox->button(QDialogButtonBox::Ok)->setVisible(false);

    d->connectButton = new QPushButton(tr("Connect"), this);
    d->ui->buttonBox->addButton(d->connectButton, QDialogButtonBox::HelpRole);
    connect(d->connectButton, &QPushButton::clicked, this,
        [this]
        {
            emit connectRequested(d->result.connection);
            accept();
        });

    d->setupNewServerButton = new QPushButton(tr("Setup"), this);
    d->ui->buttonBox->addButton(d->setupNewServerButton, QDialogButtonBox::HelpRole);
    connect(d->setupNewServerButton, &QPushButton::clicked, this,
        [this]
        {
            emit setupNewServerRequested(d->result.newServerId);
            accept();
        });

    d->connectButton->setVisible(false);
    d->setupNewServerButton->setVisible(false);

    d->ui->progressBar->setValue(0);
    d->ui->progressBar->setMaximum(connectionTimeoutMs / timerIntervalMs);

    connect(d->timeoutTimer.get(), &QTimer::timeout, this, &ConnectionTestingDialog::tick);
    d->timeoutTimer->setInterval(timerIntervalMs);
    d->timeoutTimer->setSingleShot(false);

    connect(d->ui->descriptionLabel, &QLabel::linkActivated, this,
        [this](const QString& link)
        {
            if (link == "#cloud")
                emit loginToCloudRequested();
        });
}

void ConnectionTestingDialog::testConnection(
    nx::network::SocketAddress address,
    nx::network::http::Credentials credentials,
    const nx::utils::SoftwareVersion& engineVersion)
{
    d->engineVersion = engineVersion;

    auto callback = nx::utils::guarded(this,
        [this](RemoteConnectionFactory::ConnectionOrError result)
        {
            if (!d->connectionProcess)
                return;

            d->timeoutTimer->stop();
            d->ui->progressBar->setValue(d->ui->progressBar->maximum());
            if (const auto error = std::get_if<RemoteConnectionError>(&result))
            {
                if (*error == RemoteConnectionErrorCode::factoryServer)
                {
                    d->result.status = DiagnosticResult::Status::factoryServer;
                    d->result.newServerId = d->connectionProcess->context->moduleInformation.id;
                }
                else
                {
                    d->result.status = DiagnosticResult::Status::error;
                    d->result.error = *error;
                }
            }
            else
            {
                d->result.status = DiagnosticResult::Status::success;
                d->result.connection = std::get<RemoteConnectionPtr>(result);
            }
            updateUi();
            d->connectionProcess.reset();
        });

    auto remoteConnectionFactory = qnClientCoreModule->networkModule()->connectionFactory();
    const core::LogonData info{address, credentials, nx::vms::api::UserType::local};
    d->connectionProcess = remoteConnectionFactory->connect(info, callback,
        std::make_unique<Private::ConnectionUserInteractionDelegate>(this));

    d->timeoutTimer->start();
    exec();
}

ConnectionTestingDialog::~ConnectionTestingDialog()
{
    if (d->connectionProcess)
        RemoteConnectionFactory::destroyAsync(std::move(d->connectionProcess));
}

void ConnectionTestingDialog::tick()
{
    if (d->ui->progressBar->value() != d->ui->progressBar->maximum())
    {
        d->ui->progressBar->setValue(d->ui->progressBar->value() + 1);
        return;
    }

    d->timeoutTimer->stop();
    d->connectionProcess.reset();
    d->result.status = DiagnosticResult::Status::timeout;
    updateUi();
}

void ConnectionTestingDialog::updateUi()
{
    switch (d->result.status)
    {
        case DiagnosticResult::Status::notStarted:
        {
            return;
        }
        case DiagnosticResult::Status::success:
        {
            d->ui->statusLabel->setText(tr("Success"));
            d->ui->descriptionLabel->setVisible(false);
            d->connectButton->setVisible(true);
            break;
        }
        case DiagnosticResult::Status::error:
        {
            d->ui->statusLabel->setText(tr("Test Failed"));
            d->ui->descriptionLabel->setVisible(true);
            if (d->connectionProcess)
            {
                auto targetModuleInformation = d->connectionProcess->context->moduleInformation;

                const auto error = d->result.error;
                QString description = error->externalDescription
                    ? *error->externalDescription
                    : errorDescription(
                        error->code,
                        targetModuleInformation,
                        d->engineVersion).longText;

                if (ini().developerMode
                    || alwaysShowDeveloperError(error->code, targetModuleInformation))
                {
                    description += common::html::kLineBreak
                        + QnConnectionDiagnosticsHelper::developerModeText(
                            d->connectionProcess->context->moduleInformation,
                            d->engineVersion)
                        + common::html::kHorizontalLine
                        + nx::format("<div style=\"text-indent: 0; font-size: 12px; color: #CCC;\">"
                            "Error code: <b>%1</b></div>",
                            nx::reflect::toString(error->code)).toQString();
                }

                d->ui->descriptionLabel->setText(description);
                setHelpTopic(this, helpTopicId(error->code));
            }
            break;
        }
        case DiagnosticResult::Status::timeout:
        {
            d->ui->statusLabel->setText(tr("Request timeout"));
            d->ui->descriptionLabel->setVisible(false);
            break;
        }
        case DiagnosticResult::Status::factoryServer:
        {
            d->ui->statusLabel->setText(tr("New System"));
            d->setupNewServerButton->setVisible(true);
            d->ui->descriptionLabel->setVisible(false);
            setHelpTopic(this, HelpTopic::Id::Setup_Wizard);
            break;
        }
    }

    d->ui->buttonBox->button(QDialogButtonBox::Ok)->setVisible(true);
    d->ui->buttonBox->button(QDialogButtonBox::Cancel)->setVisible(false);
}

} // namespace nx::vms::client::desktop
