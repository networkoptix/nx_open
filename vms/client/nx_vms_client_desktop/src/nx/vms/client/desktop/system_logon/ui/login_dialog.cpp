// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "login_dialog.h"
#include "ui_login_dialog.h"

#include <thread>

#include <QtCore/QEvent>
#include <QtCore/QUrlQuery>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QApplication>

#include <client_core/client_core_module.h>
#include <network/system_helpers.h>
#include <nx/build_info.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/input_field.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/remote_session.h>
#include <nx/vms/utils/system_uri.h>
#include <ui/graphics/opengl/gl_functions.h>
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
    setHelpTopic(this, HelpTopic::Id::Login);

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
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &LoginDialog::updateAcceptibility);
    connect(ui->linkLineEdit, &InputField::textChanged, this, &LoginDialog::updateAcceptibility);


    const auto url = nx::utils::Url(appContext()->localSettings()->lastLocalConnectionUrl());
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
        const auto introWidget = new QQuickWidget(appContext()->qmlEngine(), this);
        introWidget->rootContext()->setContextProperty("maxTextureSize",
            QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE));

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

ConnectionInfo LoginDialog::connectionInfo() const
{
    if (isCredentialsTab())
    {
        const auto host = ui->hostnameLineEdit->text().trimmed().toStdString();
        uint16_t port = ui->portSpinBox->value();

        if (!nx::utils::Url::isValidHost(host) || port == 0)
            return {};

        nx::network::SocketAddress address(host, port);

        nx::network::http::PasswordCredentials credentials(
            ui->loginLineEdit->text().trimmed().toLower().toStdString(),
            ui->passwordEdit->text().toStdString());

        return {address, credentials};
    }
    else if (isLinkTab())
    {
        const auto uri = nx::vms::utils::SystemUri::fromTemporaryUserLink(ui->linkLineEdit->text());

        if (!uri.isValid())
            return {};

        return {uri.systemAddress.toStdString(), uri.credentials};
    }

    return {};
}

bool LoginDialog::isValid() const
{
    const ConnectionInfo info = connectionInfo();
    const nx::network::http::AuthToken authToken = info.credentials.authToken;
    return !info.address.isNull()
        && !authToken.empty()
        && (authToken.isBearerToken() || !info.credentials.username.empty());
}

void LoginDialog::sendTestConnectionRequest()
{
    const ConnectionInfo info = connectionInfo();

    d->scenarioGuard = statisticsModule()->certificates()->beginScenario(
        QnCertificateStatisticsModule::Scenario::connectFromDialog);

    auto showError =
        [this](const RemoteConnectionError& error)
        {
            if (isCredentialsTab())
            {
                QnConnectionDiagnosticsHelper::showConnectionErrorMessage(
                    windowContext(),
                    error,
                    d->connectionProcess->context->moduleInformation,
                    appContext()->version(),
                    this);
            }
            else
            {
                ui->linkLineEdit->setInvalidValidationResult(
                    tr("The provided link is not valid or has expired"));
            }
        };

    auto callback = nx::utils::guarded(this,
        [this, showError] (RemoteConnectionFactory::ConnectionOrError result)
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
                            menu()->trigger(menu::DelayedForcedExitAction);
                        }
                        break;
                    }
                    case RemoteConnectionErrorCode::factoryServer:
                    {
                        LogonData logonData(d->connectionProcess->context->logonData);
                        logonData.expectedServerId = moduleInformation.id;
                        logonData.expectedServerVersion = moduleInformation.version;

                        menu()->trigger(menu::SetupFactoryServerAction,
                            menu::Parameters().withArgument(Qn::LogonDataRole, logonData));
                        base_type::accept();
                        break;
                    }
                    default:
                        showError(*error);
                        break;
                }
            }
            else
            {
                if (auto session = system()->session())
                    session->autoTerminateIfNeeded();
                auto connection = std::get<RemoteConnectionPtr>(result);
                menu()->trigger(menu::ConnectAction,
                    menu::Parameters().withArgument(Qn::RemoteConnectionRole, connection));
                base_type::accept();
            }

            d->resetConnectionProcess(/*async*/ false);
            updateUsability();
        });

    auto remoteConnectionFactory = qnClientCoreModule->networkModule()->connectionFactory();

    // User type will be verified during the connection.
    const core::LogonData logonData{
        .address = info.address,
        .credentials = info.credentials,
        .userType = nx::vms::api::UserType::local};
    d->connectionProcess = remoteConnectionFactory->connect(logonData, callback);

    updateUsability();
}

bool LoginDialog::isCredentialsTab() const
{
    return ui->tabWidget->currentWidget() == ui->credentialsTab;
}

bool LoginDialog::isLinkTab() const
{
    return ui->tabWidget->currentWidget() == ui->linkTab;
}

void LoginDialog::accept()
{
    if (NX_ASSERT(isValid()))
        sendTestConnectionRequest();
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
        [this]() { menu()->trigger(menu::LoginToCloud); });

    const ConnectionInfo info = connectionInfo();
    dialog->testConnection(info.address, info.credentials, appContext()->version());

    updateFocus();
    if (requestedConnection)
    {
        if (auto session = system()->session())
            session->autoTerminateIfNeeded();
        menu()->trigger(menu::ConnectAction,
            menu::Parameters().withArgument(Qn::RemoteConnectionRole, requestedConnection));
        base_type::accept();
    }
    else if (setupNewServerRequested)
    {
        LogonData logonData(core::LogonData{
            .address = info.address,
            .credentials = info.credentials,
            .expectedServerId = expectedNewServerId});

        menu()->trigger(menu::SetupFactoryServerAction,
            menu::Parameters().withArgument(Qn::LogonDataRole, logonData));
        base_type::accept();
    }
}

} // namespace nx::vms::client::desktop
