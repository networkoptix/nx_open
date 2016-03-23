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
#include <utils/common/string.h>

#include <ui/style/custom_style.h>
#include <ui/dialogs/message_box.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

using namespace nx::cdb;

namespace
{
    QString kCreateAccountPath = lit("/static/index.html#/register");

    rest::QnConnectionPtr getPublicServerConnection()
    {
        for (const QnMediaServerResourcePtr server: qnResPool->getAllServers(Qn::AnyStatus))
        {
            if (server->getStatus() != Qn::Online)
                continue;

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

    void lockUi(bool lock);
    void bindSystem();

    void showSuccess();
    void showFailure(const QString &message = QString());
    void showCredentialsFailure();

private:
    void at_bindFinished(api::ResultCode result, const api::SystemData &systemData, const rest::QnConnectionPtr &connection);

public:
    std::unique_ptr<
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> connectionFactory;
    std::unique_ptr<api::Connection> cloudConnection;
    bool linkedSuccessfully;
};

QnLinkToCloudDialog::QnLinkToCloudDialog(QWidget *parent)
    : base_type(parent)
    , ui(new Ui::QnLinkToCloudDialog)
    , d_ptr(new QnLinkToCloudDialogPrivate(this))
{
    ui->setupUi(this);

    Q_D(QnLinkToCloudDialog);

    auto *okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    auto updateOkButton = [this, okButton]()
    {
        okButton->setEnabled(!ui->accountLineEdit->text().isEmpty() &&
                             !ui->passwordLineEdit->text().isEmpty());
    };

    ui->accountLineEdit->setText(qnSettings->cloudLogin());
    ui->passwordLineEdit->setText(qnSettings->cloudPassword());

    connect(ui->accountLineEdit,    &QLineEdit::textChanged,            d,      updateOkButton);
    connect(ui->passwordLineEdit,   &QLineEdit::textChanged,            d,      updateOkButton);

    if (!ui->accountLineEdit->text().isEmpty() && !ui->passwordLineEdit->text().isEmpty())
        okButton->setFocus();
    else
        ui->accountLineEdit->selectAll();

    ui->createAccountLabel->setText(makeHref(tr("Create account"), QnCloudUrlHelper::createAccountUrl()));
    setWarningStyle(ui->invalidCredentialsLabel);
    ui->invalidCredentialsLabel->hide();
    updateOkButton();
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

QnLinkToCloudDialogPrivate::QnLinkToCloudDialogPrivate(QnLinkToCloudDialog *parent)
    : QObject(parent)
    , q_ptr(parent)
    , connectionFactory(createConnectionFactory(), &destroyConnectionFactory)
    , linkedSuccessfully(false)
{
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

void QnLinkToCloudDialogPrivate::lockUi(bool lock)
{
    Q_Q(QnLinkToCloudDialog);

    const bool enabled = !lock;

    q->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enabled);
    q->ui->createAccountLabel->setEnabled(enabled);
    q->ui->credentialsWidget->setEnabled(enabled);

    if (enabled)
        q->ui->buttonBox->button(QDialogButtonBox::Ok)->setFocus();
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

    cloudConnection = connectionFactory->createConnection(
                          q->ui->accountLineEdit->text().toStdString(),
                          q->ui->passwordLineEdit->text().toStdString());

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
                            tr("The system is successfully linked to %1").arg(q->ui->accountLineEdit->text()),
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

    lockUi(false);
}

void QnLinkToCloudDialogPrivate::showCredentialsFailure()
{
    Q_Q(QnLinkToCloudDialog);

    q->ui->invalidCredentialsLabel->show();

    lockUi(false);
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
            showCredentialsFailure();
            break;
        default:
            showFailure(QString()); // TODO: #dklychkov More detailed diagnostics
            break;
        }
        return;
    }

    const auto &admin = qnResPool->getAdministrator();

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
        q->ui->accountLineEdit->text(),
        handleReply,
        q->thread());
}
