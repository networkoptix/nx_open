// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "login_dialog.h"
#include "ui_login_dialog.h"

#include <thread>

#include <QtCore/QEvent>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QApplication>

#include <client/client_settings.h>
#include <client_core/client_core_module.h>
#include <network/system_helpers.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_logon/logic/remote_session.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/statistics/modules/certificate_statistics_module.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>
#include <utils/connection_diagnostics_helper.h>

#include "../data/logon_data.h"
#include "connection_testing_dialog.h"

using namespace nx::vms::client::core;

namespace nx::vms::client::desktop {

namespace {

static const QString kLocalHost("127.0.0.1");

static std::vector<const char*> kIntroNames {
    "intro.mkv",
    "intro.png",
};

void setEnabledExcept(const QObjectList& objects, QObject* exclude, bool enabled)
{
    for (QObject* object: objects)
    {
        if (object != exclude)
        {
            if (auto widget = qobject_cast<QWidget*>(object))
                widget->setEnabled(enabled);
        }
    }
}

class IntroContainer: public QWidget
{
public:
    IntroContainer(QWidget* intro, QWidget* parent = nullptr): QWidget(parent)
    {
        intro->setParent(this);
        anchorWidgetToParent(intro);
    }

    static constexpr qreal kAspectRatio = 16.0 / 9.0;

    virtual bool hasHeightForWidth() const override { return true; }
    virtual int heightForWidth(int width) const override { return width / kAspectRatio; }
};

} // namespace

struct LoginDialog::Private
{
    RemoteConnectionFactory::ProcessPtr connectionProcess;
    std::shared_ptr<QnStatisticsScenarioGuard> scenarioGuard;

    void resetConnectionProcess(bool async)
    {
        if (async)
            RemoteConnectionFactory::destroyAsync(std::move(connectionProcess));
        else
            connectionProcess.reset();

        scenarioGuard.reset();
    }
};

LoginDialog::LoginDialog(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private()),
    ui(new Ui::LoginDialog)
{
    ui->setupUi(this);

    setAccentStyle(ui->buttonBox->button(QDialogButtonBox::Ok));

    setWindowTitle(tr("Connect to Server..."));
    setHelpTopic(this, Qn::Login_Help);

    auto buttonBoxLayout = dynamic_cast<QHBoxLayout*>(ui->buttonBox->layout());
    if (NX_ASSERT(buttonBoxLayout))
    {
        QLabel* versionLabel = new QLabel(ui->buttonBox);
        versionLabel->setText(tr("Version %1").arg(qApp->applicationVersion()));
        QFont font = versionLabel->font();
        font.setPointSize(7);
        versionLabel->setFont(font);
        buttonBoxLayout->insertWidget(0, versionLabel);
    }

    setupIntroView();

    connect(ui->testButton, &QPushButton::clicked, this,
        &LoginDialog::at_testButton_clicked);
    connect(ui->passwordEdit, &nx::vms::client::desktop::PasswordInputField::textChanged, this,
        &LoginDialog::updateAcceptibility);
    connect(ui->loginLineEdit, &QLineEdit::textChanged, this,
        &LoginDialog::updateAcceptibility);
    connect(ui->hostnameLineEdit, &QLineEdit::textChanged, this,
        &LoginDialog::updateAcceptibility);
    connect(ui->portSpinBox, QnSpinboxIntValueChanged, this,
        &LoginDialog::updateAcceptibility);

    const auto url = nx::utils::Url(qnSettings->lastLocalConnectionUrl());
    if (url.host().isEmpty()) //< No last used connection.
    {
        ui->hostnameLineEdit->setText(kLocalHost);
        ui->portSpinBox->setValue(::helpers::kDefaultConnectionPort);
        ui->loginLineEdit->setText(::helpers::kFactorySystemUser);
    }
    else
    {
        ui->hostnameLineEdit->setText(url.host());
        ui->portSpinBox->setValue(url.port());
        ui->loginLineEdit->setText(url.userName().isEmpty()
            ? ::helpers::kFactorySystemUser  //< 90% of users have only one login - admin.
            : url.userName());
    }

    updateFocus();
}

LoginDialog::~LoginDialog()
{
}

void LoginDialog::setupIntroView()
{
    QString introPath;
    for (auto name: kIntroNames)
    {
        introPath = qnSkin->path(name);
        if (!introPath.isEmpty())
            break;
    }

    if (!NX_ASSERT(!introPath.isEmpty()))
        return;

    introPath.replace('\\', "/");

    auto layout = new QVBoxLayout(ui->videoSpacer);
    layout->setSpacing(0);
    layout->setContentsMargins({});

    QImage image;
    if (image.load(introPath))
    {
        const auto staticIntro = new QLabel(this);
        staticIntro->setAlignment(Qt::AlignCenter);
        staticIntro->setPixmap(QPixmap::fromImage(image));
        layout->addWidget(new IntroContainer(staticIntro));
    }
    else
    {
        const auto introWidget = new QQuickWidget(qnClientCoreModule->mainQmlEngine(), this);
        introWidget->rootContext()->setContextProperty("maxTextureSize",
            QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE));

        introWidget->setObjectName("LoginDialog_IntroWidget");
        introWidget->setSource({ "Nx/Intro/Intro.qml" });
        introWidget->rootObject()->setProperty("introPath", QString("file:///") + introPath);
        introWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
        layout->addWidget(new IntroContainer(introWidget));
    }
}

void LoginDialog::updateFocus()
{
    ui->passwordEdit->setFocus();
}

nx::utils::Url LoginDialog::currentUrl() const
{
    nx::utils::Url url;
    url.setScheme(nx::network::http::kSecureUrlSchemeName);
    url.setHost(ui->hostnameLineEdit->text().trimmed());
    url.setPort(ui->portSpinBox->value());
    url.setUserName(ui->loginLineEdit->text().trimmed());
    url.setPassword(ui->passwordEdit->text());
    return url;
}

bool LoginDialog::isValid() const
{
    nx::utils::Url url = currentUrl();
    return !ui->passwordEdit->text().isEmpty()
        && !ui->loginLineEdit->text().trimmed().isEmpty()
        && !ui->hostnameLineEdit->text().trimmed().isEmpty()
        && ui->portSpinBox->value() != 0
        && url.isValid()
        && !url.host().isEmpty();
}

void LoginDialog::sendTestConnectionRequest(const nx::utils::Url& url)
{
    nx::network::SocketAddress address(url.host(), url.port());
    nx::network::http::Credentials credentials(
        url.userName().toLower().toStdString(),
        nx::network::http::PasswordAuthToken(url.password().toStdString()));

    d->scenarioGuard = statisticsModule()->certificates()->beginScenario(
            QnCertificateStatisticsModule::Scenario::connectFromDialog);

    auto callback = nx::utils::guarded(this,
        [url, this] (RemoteConnectionFactory::ConnectionOrError result)
        {
            if (!d->connectionProcess)
                return;

            if (const auto error = std::get_if<RemoteConnectionError>(&result))
            {
                const auto& moduleInformation = d->connectionProcess->context->moduleInformation;
                switch (error->code)
                {
                    case RemoteConnectionErrorCode::binaryProtocolVersionDiffers:
                    case RemoteConnectionErrorCode::cloudHostDiffers:
                    {
                        if (QnConnectionDiagnosticsHelper::downloadAndRunCompatibleVersion(
                                this,
                                moduleInformation,
                                d->connectionProcess->context->logonData,
                                appContext()->version()))
                        {
                            menu()->trigger(ui::action::DelayedForcedExitAction);
                        }
                        break;
                    }
                    case RemoteConnectionErrorCode::factoryServer:
                    {
                        LogonData logonData(d->connectionProcess->context->logonData);
                        logonData.expectedServerId = moduleInformation.id;
                        logonData.expectedServerVersion = moduleInformation.version;

                        menu()->trigger(ui::action::SetupFactoryServerAction,
                            ui::action::Parameters().withArgument(Qn::LogonDataRole, logonData));
                        base_type::accept();
                        break;
                    }
                    default:
                        QnConnectionDiagnosticsHelper::showConnectionErrorMessage(
                            context(),
                            *error,
                            d->connectionProcess->context->moduleInformation,
                            appContext()->version(),
                            this);
                        break;
                }
            }
            else
            {
                if (auto session = RemoteSession::instance())
                    session->autoTerminateIfNeeded();
                auto connection = std::get<RemoteConnectionPtr>(result);
                menu()->trigger(ui::action::ConnectAction,
                    ui::action::Parameters().withArgument(Qn::RemoteConnectionRole, connection));
                base_type::accept();
            }

            d->resetConnectionProcess(/*async*/ false);
            updateUsability();
        });

    auto remoteConnectionFactory = qnClientCoreModule->networkModule()->connectionFactory();
    const core::LogonData logonData{
        .address = address,
        .credentials = credentials,
        .userType = nx::vms::api::UserType::local};
    d->connectionProcess = remoteConnectionFactory->connect(logonData, callback);

    updateUsability();
}

void LoginDialog::accept()
{
    if (NX_ASSERT(isValid()))
        sendTestConnectionRequest(currentUrl());
}

void LoginDialog::reject()
{
    if (!d->connectionProcess)
    {
        base_type::reject();
        return;
    }
    else
    {
        d->resetConnectionProcess(/*async*/ true);
    }
    updateUsability();
}

void LoginDialog::showEvent(QShowEvent *event)
{
    base_type::showEvent(event);

    if (nx::build_info::isMacOsX())
    {
        if (focusWidget())
            focusWidget()->activateWindow();
    }
}

void LoginDialog::updateAcceptibility()
{
    bool acceptable = isValid();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(acceptable);
    ui->testButton->setEnabled(acceptable);
}

void LoginDialog::updateUsability()
{
    if (!d->connectionProcess)
    {
        setEnabledExcept(children(), ui->buttonBox, true);
        setEnabledExcept(
            ui->buttonBox->children(),
            ui->buttonBox->button(QDialogButtonBox::Cancel),
            true);
        unsetCursor();
        ui->buttonBox->button(QDialogButtonBox::Cancel)->unsetCursor();

        updateAcceptibility();
        updateFocus();
    }
    else
    {
        setEnabledExcept(children(), ui->buttonBox, false);
        setEnabledExcept(
            ui->buttonBox->children(),
            ui->buttonBox->button(QDialogButtonBox::Cancel),
            false);
        setCursor(Qt::BusyCursor);
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setCursor(Qt::ArrowCursor);
    }
}

void LoginDialog::at_testButton_clicked()
{
    NX_ASSERT(isValid());
    if (!isValid())
        return;

    auto connectScenario =
        statisticsModule()->certificates()->beginScenario(
            ConnectScenario::connectFromDialog);

    nx::vms::client::core::RemoteConnectionPtr requestedConnection;
    bool setupNewServerRequested = false;
    QnUuid expectedNewServerId;

    auto dialog = std::make_unique<ConnectionTestingDialog>(this);
    connect(dialog.get(), &ConnectionTestingDialog::connectRequested, this,
        [&requestedConnection](nx::vms::client::core::RemoteConnectionPtr connection)
        {
            requestedConnection = connection;
        });
    connect(dialog.get(), &ConnectionTestingDialog::setupNewServerRequested, this,
        [&setupNewServerRequested, &expectedNewServerId](QnUuid expectedServerId)
        {
            setupNewServerRequested = true;
            expectedNewServerId = expectedServerId;
        });
    connect(dialog.get(), &ConnectionTestingDialog::loginToCloudRequested, this,
        [this]() { menu()->trigger(ui::action::LoginToCloud); });

    nx::utils::Url url = currentUrl();
    nx::network::SocketAddress address(url.host(), url.port());
    nx::network::http::PasswordCredentials credentials(
        url.userName().toLower().toStdString(),
        url.password().toStdString());
    dialog->testConnection(address, credentials, appContext()->version());

    updateFocus();
    if (requestedConnection)
    {
        if (auto session = RemoteSession::instance())
            session->autoTerminateIfNeeded();
        menu()->trigger(ui::action::ConnectAction,
            ui::action::Parameters().withArgument(Qn::RemoteConnectionRole, requestedConnection));
        base_type::accept();
    }
    else if (setupNewServerRequested)
    {
        LogonData logonData(core::LogonData{
            .address = address,
            .credentials = credentials,
            .expectedServerId = expectedNewServerId});

        menu()->trigger(ui::action::SetupFactoryServerAction,
            ui::action::Parameters().withArgument(Qn::LogonDataRole, logonData));
        base_type::accept();
    }
}

} // namespace nx::vms::client::desktop
