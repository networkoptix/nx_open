#include "connect_to_cloud_dialog.h"
#include "ui_connect_to_cloud_dialog.h"

#include <QtWidgets/QGraphicsOpacityEffect>

#include <api/global_settings.h>
#include <api/server_rest_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/param.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <cloud/cloud_connection.h>
#include <cloud/cloud_result_info.h>

#include <client_core/client_core_settings.h>

#include <common/common_module.h>

#include <helpers/cloud_url_helper.h>
#include <utils/common/delayed.h>

#include <utils/common/html.h>

#include <ui/common/aligner.h>
#include <ui/dialogs/cloud/cloud_result_messages.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/widgets/common/busy_indicator_button.h>
#include <ui/widgets/common/input_field.h>
#include <nx/client/desktop/ui/actions/action_manager.h>

#include <watchers/cloud_status_watcher.h>

#include <utils/common/app_info.h>

using namespace nx::cdb;
using namespace nx::client::desktop::ui;

namespace {

const int kHeaderFontSizePixels = 15;
const int kHeaderFontWeight = QFont::DemiBold;

rest::QnConnectionPtr getPublicServerConnection(QnResourcePool* resourcePool)
{
    for (const QnMediaServerResourcePtr server: resourcePool->getAllServers(Qn::Online))
    {
        if (!server->getServerFlags().testFlag(Qn::SF_HasPublicIP))
            continue;

        return server->restConnection();
    }
    return rest::QnConnectionPtr();
}

} // namespace

class QnConnectToCloudDialogPrivate: public QObject, public QnConnectionContextAware
{
    QnConnectToCloudDialog *q_ptr;

    Q_DECLARE_PUBLIC(QnConnectToCloudDialog)
    Q_DECLARE_TR_FUNCTIONS(QnConnectToCloudDialogPrivate)

public:
    QnConnectToCloudDialogPrivate(QnConnectToCloudDialog *parent);

    void updateUi();
    void lockUi(bool locked);
    void bindSystem();

    void showCredentialsError(const QString& text);

    void showFailure(const QString& message);

    void showSuccess(const QString& cloudLogin);

private:
    void at_bindFinished(api::ResultCode result, const api::SystemData &systemData,
        const rest::QnConnectionPtr &connection);

public:
    std::unique_ptr<api::Connection> cloudConnection;
    bool linkedSuccessfully;
    QnBusyIndicatorButton* indicatorButton;
};

QnConnectToCloudDialog::QnConnectToCloudDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::ConnectToCloudDialog),
    d_ptr(new QnConnectToCloudDialogPrivate(this))
{
    ui->setupUi(this);
    setWindowTitle(tr("Connect to %1",
        "%1 is the cloud name (like 'Nx Cloud')").arg(nx::network::AppInfo::cloudName()));

    Q_D(QnConnectToCloudDialog);

    using nx::vms::utils::SystemUri;
    QnCloudUrlHelper urlHelper(
        SystemUri::ReferralSource::DesktopClient,
        SystemUri::ReferralContext::SettingsDialog);

    /* We replace standard button instead of simply adding our one to keep OS theme values: */
    QScopedPointer<QAbstractButton> okButton(ui->buttonBox->button(QDialogButtonBox::Ok));
    d->indicatorButton->setText(okButton->text()); // Title from OS theme
    d->indicatorButton->setIcon(okButton->icon()); // Icon from OS theme
    d->indicatorButton->setDefault(true);
    setAccentStyle(d->indicatorButton);
    ui->buttonBox->removeButton(okButton.data());
    ui->buttonBox->addButton(d->indicatorButton, QDialogButtonBox::AcceptRole);

    QFont font;
    font.setPixelSize(kHeaderFontSizePixels);
    font.setWeight(kHeaderFontWeight);
    ui->enterCloudAccountLabel->setFont(font);
    ui->enterCloudAccountLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->enterCloudAccountLabel->setText(tr("Enter %1 Account",
        "%1 is the cloud name (like 'Nx Cloud')").arg(nx::network::AppInfo::cloudName()));
    ui->enterCloudAccountLabel->setForegroundRole(QPalette::Light);

    ui->loginInputField->setTitle(tr("Email"));
    ui->loginInputField->setValidator(Qn::defaultEmailValidator(false));

    ui->passwordInputField->setTitle(tr("Password"));
    ui->passwordInputField->setEchoMode(QLineEdit::Password);
    ui->passwordInputField->setValidator(Qn::defaultPasswordValidator(false));

    ui->createAccountLabel->setText(makeHref(tr("Create account"), urlHelper.createAccountUrl()));
    ui->forgotPasswordLabel->setText(makeHref(tr("Forgot password?"), urlHelper.restorePasswordUrl()));

    auto aligner = new QnAligner(this);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
    aligner->addWidgets({ ui->loginInputField, ui->passwordInputField, ui->spacer });

    auto opacityEffect = new QGraphicsOpacityEffect(this);
    opacityEffect->setOpacity(style::Hints::kDisabledItemOpacity);
    ui->linksWidget->setGraphicsEffect(opacityEffect);

    /* Checking if we have logged in under temporary credentials. */
    auto credentials = qnCloudStatusWatcher->credentials();
    auto effectiveName = qnCloudStatusWatcher->effectiveUserName();
    ui->loginInputField->setText(effectiveName);

    connect(ui->loginInputField,    &QnInputField::textChanged, d, &QnConnectToCloudDialogPrivate::updateUi);
    connect(ui->passwordInputField, &QnInputField::textChanged, d, &QnConnectToCloudDialogPrivate::updateUi);
    connect(this, &QnConnectToCloudDialog::bindFinished,
        d, &QnConnectToCloudDialogPrivate::at_bindFinished, Qt::QueuedConnection);
    setWarningStyle(ui->invalidCredentialsLabel);

    d->lockUi(false);
    d->updateUi();
    setResizeToContentsMode(Qt::Vertical);
}

QnConnectToCloudDialog::~QnConnectToCloudDialog()
{
}

void QnConnectToCloudDialog::accept()
{
    Q_D(QnConnectToCloudDialog);

    if (!d->linkedSuccessfully)
    {
        d->bindSystem();
        return;
    }

    base_type::accept();
}

void QnConnectToCloudDialog::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    if (ui->loginInputField->text().isEmpty())
        ui->loginInputField->setFocus();
    else
        ui->passwordInputField->setFocus();
}

QnConnectToCloudDialogPrivate::QnConnectToCloudDialogPrivate(QnConnectToCloudDialog* parent):
    QObject(parent),
    q_ptr(parent),
    linkedSuccessfully(false),
    indicatorButton(new QnBusyIndicatorButton(parent))
{
}

void QnConnectToCloudDialogPrivate::updateUi()
{
    Q_Q(QnConnectToCloudDialog);
    indicatorButton->setEnabled(q->ui->loginInputField->isValid()
                             && q->ui->passwordInputField->isValid());

    const bool visible = (qnCloudStatusWatcher->status() == QnCloudStatusWatcher::LoggedOut);
    q->ui->stayLoggedInCheckBox->setVisible(visible);

    showCredentialsError(QString());
};


void QnConnectToCloudDialogPrivate::showCredentialsError(const QString& text)
{
    Q_Q(QnConnectToCloudDialog);
    q->ui->invalidCredentialsLabel->setHidden(text.isEmpty());
    q->ui->invalidCredentialsLabel->setText(text);
    q->ui->credentialsWidget->layout()->activate();
}

void QnConnectToCloudDialogPrivate::lockUi(bool locked)
{
    Q_Q(QnConnectToCloudDialog);
    const bool enabled = !locked;

    q->ui->credentialsWidget->setEnabled(enabled);
    q->ui->enterCloudAccountLabel->setEnabled(enabled);

    q->ui->linksWidget->graphicsEffect()->setEnabled(locked);

    indicatorButton->setEnabled(enabled && q->ui->invalidCredentialsLabel->isHidden());
    indicatorButton->setFocus();

    indicatorButton->showIndicator(locked);
}

void QnConnectToCloudDialogPrivate::bindSystem()
{
    Q_Q(QnConnectToCloudDialog);

    q->ui->invalidCredentialsLabel->hide();

    auto serverConnection = getPublicServerConnection(resourcePool());
    if (!serverConnection)
    {
        showFailure(tr("None of your servers is connected to the Internet."));
        return;
    }

    lockUi(true);
    indicatorButton->setEnabled(false);

    cloudConnection = qnCloudConnectionProvider->createConnection();
    cloudConnection->setCredentials(
        q->ui->loginInputField->text().trimmed().toStdString(),
        q->ui->passwordInputField->text().trimmed().toStdString());

    nx::cdb::api::SystemRegistrationData sysRegistrationData;
    sysRegistrationData.name = qnGlobalSettings->systemName().toStdString();
    sysRegistrationData.customization = QnAppInfo::customizationName().toStdString();

    const auto guard = QPointer<QObject>(this);
    const auto thread = guard->thread();
    const auto completionHandler =
        [this, serverConnection, guard, thread](api::ResultCode result, api::SystemData systemData)
        {
            if (!guard)
                return;

            Q_Q(QnConnectToCloudDialog);
            emit q->bindFinished(result, systemData, serverConnection);
        };

    cloudConnection->systemManager()->bindSystem(sysRegistrationData, completionHandler);
}

void QnConnectToCloudDialogPrivate::showSuccess(const QString& /*cloudLogin*/)
{
    Q_Q(QnConnectToCloudDialog);

    linkedSuccessfully = true;
    q->menu()->trigger(action::HideCloudPromoAction);

    QnMessageBox::success(q->parentWidget(),
        tr("System connected to %1", "%1 is the cloud name (like 'Nx Cloud')")
            .arg(nx::network::AppInfo::cloudName()));

    // Since we have QTBUG-40585 event loops of dialogs shouldn't be intersected.
    q->accept();
}

void QnConnectToCloudDialogPrivate::showFailure(const QString &message)
{
    Q_Q(QnConnectToCloudDialog);

    QnMessageBox::critical(q,
        tr("Failed to connect System to %1", "%1 is the cloud name (like 'Nx Cloud')")
            .arg(nx::network::AppInfo::cloudName()),
        message);

    lockUi(false);
}

void QnConnectToCloudDialogPrivate::at_bindFinished(
    api::ResultCode result,
    const api::SystemData &systemData,
    const rest::QnConnectionPtr &connection)
{
    Q_Q(QnConnectToCloudDialog);

    if (result != api::ResultCode::ok)
    {
        switch (result)
        {
            case api::ResultCode::badUsername:
            case api::ResultCode::notAuthorized:
                showCredentialsError(QnCloudResultMessages::invalidCredentials());
                break;

            case api::ResultCode::accountNotActivated:
                showCredentialsError(QnCloudResultMessages::accountNotActivated());
                break;

            default:
                showFailure(QnCloudResultInfo(result));
                break;
        }

        lockUi(false);
        return;
    }

    const auto& admin = resourcePool()->getAdministrator();
    if (!admin)
    {
        q->reject();
        return;
    }

    auto handleReply =
        [this,
            guard = QPointer<QnConnectToCloudDialogPrivate>(this),
            parentGuard = QPointer<QnConnectToCloudDialog>(q),
            stayLoggedIn = q->ui->stayLoggedInCheckBox->isChecked(),
            cloudLogin = q->ui->loginInputField->text().trimmed(),
            cloudPassword = q->ui->passwordInputField->text().trimmed()]

            (bool success, rest::Handle /* handleId */, const QnRestResult& reply)
        {
            if (guard && parentGuard && (!success || (reply.error != QnRestResult::NoError)))
            {
                showFailure(reply.errorString);
                return;
            }

            if (stayLoggedIn)
            {
                qnClientCoreSettings->setCloudLogin(cloudLogin);
                qnClientCoreSettings->setCloudPassword(cloudPassword);
                qnClientCoreSettings->save();
                qnCloudStatusWatcher->setCredentials(
                    QnEncodedCredentials(cloudLogin, cloudPassword));
            }

            if (guard && parentGuard)
                showSuccess(cloudLogin);
        };

    connection->saveCloudSystemCredentials(
        QString::fromStdString(systemData.id),
        QString::fromStdString(systemData.authKey),
        q->ui->loginInputField->text().trimmed(),
        handleReply,
        q->thread());
}
