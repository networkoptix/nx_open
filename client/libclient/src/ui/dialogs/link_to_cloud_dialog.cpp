#include "link_to_cloud_dialog.h"
#include "ui_link_to_cloud_dialog.h"

#include <api/server_rest_connection.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/param.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <cdb/connection.h>

#include <common/common_module.h>
#include <client/client_settings.h>

#include <helpers/cloud_url_helper.h>
#include <utils/common/delayed.h>

#include <utils/common/html.h>

#include <ui/common/aligner.h>
#include <ui/common/widget_anchor.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/widgets/common/busy_indicator.h>
#include <ui/widgets/common/input_field.h>

using namespace nx::cdb;

namespace
{
    QString kCreateAccountPath = lit("/static/index.html#/register");

    const int kHeaderFontSizePixels = 15;
    const int kHeaderFontWeight = QFont::DemiBold;

    rest::QnConnectionPtr getPublicServerConnection()
    {
        for (const QnMediaServerResourcePtr server: qnResPool->getAllServers(Qn::Online))
        {
            if (!server->getServerFlags().testFlag(Qn::SF_HasPublicIP))
                continue;

            return server->restConnection();
        }
        return rest::QnConnectionPtr();
    }
}

class QnLinkToCloudDialogPrivate : public QObject
{
    QnLinkToCloudDialog *q_ptr;

    Q_DECLARE_PUBLIC(QnLinkToCloudDialog)
    Q_DECLARE_TR_FUNCTIONS(QnLinkToCloudDialogPrivate)

public:
    QnLinkToCloudDialogPrivate(QnLinkToCloudDialog *parent);

    void updateUi();
    void lockUi(bool locked);
    void bindSystem();

    void showSuccess();
    void showFailure(const QString &message = QString());
    void showCredentialsError(bool show);

private:
    void at_bindFinished(api::ResultCode result, const api::SystemData &systemData, const rest::QnConnectionPtr &connection);

public:
    std::unique_ptr<
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> connectionFactory;
    std::unique_ptr<api::Connection> cloudConnection;
    bool linkedSuccessfully;
    QnBusyIndicatorWidget* busyIndicator;
    QString okTitle;
};

QnLinkToCloudDialog::QnLinkToCloudDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::QnLinkToCloudDialog),
    d_ptr(new QnLinkToCloudDialogPrivate(this))
{
    ui->setupUi(this);

    QFont font;
    font.setPixelSize(kHeaderFontSizePixels);
    font.setWeight(kHeaderFontWeight);
    ui->enterCloudAccountLabel->setFont(font);
    ui->enterCloudAccountLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->enterCloudAccountLabel->setText(tr("Enter %1 Account").arg(QnAppInfo::cloudName()));
    ui->enterCloudAccountLabel->setForegroundRole(QPalette::Light);

    ui->loginInputField->setTitle(tr("E-Mail"));
    ui->loginInputField->setValidator(Qn::defaultEmailValidator(false));

    ui->passwordInputField->setTitle(tr("Password"));
    ui->passwordInputField->setEchoMode(QLineEdit::Password);
    ui->passwordInputField->setValidator(Qn::defaultPasswordValidator(false));
    ui->createAccountLabel->setText(makeHref(tr("Create account"), QnCloudUrlHelper::createAccountUrl()));
    ui->forgotPasswordLabel->setText(makeHref(tr("Forgot password?"), QnCloudUrlHelper::restorePasswordUrl()));

    auto aligner = new QnAligner(this);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
    aligner->addWidgets({ ui->loginInputField, ui->passwordInputField, ui->spacer });

    auto opacityEffect = new QGraphicsOpacityEffect(this);
    opacityEffect->setOpacity(style::Hints::kDisabledItemOpacity);
    ui->linksWidget->setGraphicsEffect(opacityEffect);

    Q_D(QnLinkToCloudDialog);

    //TODO: #vkutin Create a "QnIndicatorPushButton" and use it here and in QnLoginToCloudDialog
    d->busyIndicator->setParent(ui->buttonBox->button(QDialogButtonBox::Ok));

    ui->loginInputField->setText(qnSettings->cloudLogin());
    ui->passwordInputField->setText(qnSettings->cloudPassword());

    connect(ui->loginInputField,    &QnInputField::textChanged, d, &QnLinkToCloudDialogPrivate::updateUi);
    connect(ui->passwordInputField, &QnInputField::textChanged, d, &QnLinkToCloudDialogPrivate::updateUi);

    setWarningStyle(ui->invalidCredentialsLabel);

    d->lockUi(false);
    d->updateUi();

    ui->loginInputField->setFocus();

    setResizeToContentsMode(Qt::Vertical);
}

QnLinkToCloudDialog::~QnLinkToCloudDialog()
{
}

void QnLinkToCloudDialog::accept()
{
    Q_D(QnLinkToCloudDialog);

    if (!d->linkedSuccessfully)
    {
        d->bindSystem();
        return;
    }

    base_type::accept();
}

QnLinkToCloudDialogPrivate::QnLinkToCloudDialogPrivate(QnLinkToCloudDialog* parent) :
    QObject(parent),
    q_ptr(parent),
    connectionFactory(createConnectionFactory(), &destroyConnectionFactory),
    linkedSuccessfully(false),
    busyIndicator(new QnBusyIndicatorWidget(parent))
{
    new QnWidgetAnchor(busyIndicator);

    const auto cdbEndpoint = qnSettings->cdbEndpoint();
    if (!cdbEndpoint.isEmpty())
    {
        const auto hostAndPort = cdbEndpoint.split(lit(":"));
        if (hostAndPort.size() == 2)
        {
            connectionFactory->setCloudEndpoint(
                    hostAndPort[0].toStdString(),
                    hostAndPort[1].toInt());
        }
    }
}

void QnLinkToCloudDialogPrivate::updateUi()
{
    Q_Q(QnLinkToCloudDialog);
    auto okButton = q->ui->buttonBox->button(QDialogButtonBox::Ok);
    okButton->setEnabled(q->ui->loginInputField->isValid()
                      && q->ui->passwordInputField->isValid());

    showCredentialsError(false);
};


void QnLinkToCloudDialogPrivate::showCredentialsError(bool show)
{
    Q_Q(QnLinkToCloudDialog);
    q->ui->invalidCredentialsLabel->setVisible(show);
    q->ui->credentialsWidget->layout()->activate();
}

void QnLinkToCloudDialogPrivate::lockUi(bool locked)
{
    Q_Q(QnLinkToCloudDialog);
    const bool enabled = !locked;

    q->ui->credentialsWidget->setEnabled(enabled);
    q->ui->enterCloudAccountLabel->setEnabled(enabled);

    q->ui->linksWidget->graphicsEffect()->setEnabled(locked);

    auto okButton = q->ui->buttonBox->button(QDialogButtonBox::Ok);
    okButton->setEnabled(enabled && q->ui->invalidCredentialsLabel->isHidden());
    okButton->setFocus();

    if (!okButton->text().isEmpty())
        okTitle = okButton->text();

    okButton->setText(locked ? QString() : okTitle);
    busyIndicator->setVisible(locked);
}

void QnLinkToCloudDialogPrivate::bindSystem()
{
    Q_Q(QnLinkToCloudDialog);

    lockUi(true);
    q->ui->invalidCredentialsLabel->hide();

    auto serverConnection = getPublicServerConnection();
    if (!serverConnection)
    {
        showFailure(tr("None of your servers is connected to the Internet."));
        return;
    }

    q->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    cloudConnection = connectionFactory->createConnection();
    cloudConnection->setCredentials(
        q->ui->loginInputField->text().trimmed().toStdString(),
        q->ui->passwordInputField->text().trimmed().toStdString());

    nx::cdb::api::SystemRegistrationData sysRegistrationData;
    sysRegistrationData.name = qnCommon->localSystemName().toStdString();

    cloudConnection->systemManager()->bindSystem(
                sysRegistrationData,
                [this, serverConnection](api::ResultCode result, api::SystemData systemData)
    {
        Q_Q(QnLinkToCloudDialog);

        executeDelayed(
                [this, result, systemData, serverConnection]()
                {
                    at_bindFinished(result, systemData, serverConnection);
                },
                0, q->thread()
        );
    });
}

void QnLinkToCloudDialogPrivate::showSuccess()
{
    Q_Q(QnLinkToCloudDialog);
    QnMessageBox messageBox(QnMessageBox::NoIcon,
                            helpTopic(q),
                            q->windowTitle(),
                            tr("The system is successfully linked to %1").arg(q->ui->loginInputField->text().trimmed()),
                            QDialogButtonBox::Ok,
                            q->parentWidget());

    messageBox.exec();
    linkedSuccessfully = true;
    q->accept();
}

void QnLinkToCloudDialogPrivate::showFailure(const QString &message)
{
    Q_Q(QnLinkToCloudDialog);

    QnMessageBox messageBox(QnMessageBox::NoIcon,
                            helpTopic(q),
                            tr("Error"),
                            tr("Could not link the system to the cloud"),
                            QDialogButtonBox::Ok,
                            q);

    if (!message.isEmpty())
        messageBox.setInformativeText(message);

    messageBox.exec();
}

void QnLinkToCloudDialogPrivate::at_bindFinished(
        api::ResultCode result,
        const api::SystemData &systemData,
        const rest::QnConnectionPtr &connection)
{
    Q_Q(QnLinkToCloudDialog);

    if (result != api::ResultCode::ok)
    {
        switch (result)
        {
        case api::ResultCode::badUsername:
        case api::ResultCode::notAuthorized:
            showCredentialsError(true);
            break;
        default:
            showFailure(QString()); // TODO: #dklychkov More detailed diagnostics
            break;
        }
        lockUi(false);
        return;
    }

    const auto& admin = qnResPool->getAdministrator();
    if (!admin)
    {
        q->reject();
        return;
    }

    auto handleReply = [this](bool success, rest::Handle handleId, const QnRestResult& reply)
    {
        Q_UNUSED(handleId)

        if (success && reply.error == QnRestResult::NoError)
            showSuccess();
        else
            showFailure(reply.errorString);
    };

    connection->saveCloudSystemCredentials(
        QString::fromStdString(systemData.id),
        QString::fromStdString(systemData.authKey),
        q->ui->loginInputField->text().trimmed(),
        handleReply,
        q->thread());
}
