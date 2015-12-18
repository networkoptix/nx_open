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
#include <utils/common/app_info.h>
#include <utils/common/delayed.h>
#include <client/client_settings.h>

using namespace nx::cdb;

namespace
{
    QString kCreateAccountPath = lit("/static/index.html#/register");

    rest::QnConnectionPtr getPublicServerConnection()
    {
        for (const QnMediaServerResourcePtr server: qnResPool->getAllServers())
        {
            if (server->getStatus() != Qn::Online)
                continue;

            if (!server->getServerFlags().testFlag(Qn::SF_HasPublicIP))
                continue;

            return server->serverRestConnection();
        }
        return rest::QnConnectionPtr();
    }
}

class QnLinkToCloudDialogPrivate : public QObject
{
public:
    class QnLinkToCloudDialog *q_ptr;

    QnLinkToCloudDialogPrivate(QnLinkToCloudDialog *parent);

    void updateButtonBox();
    void bindSystem();

    void openSuccessPage();
    void openFailurePage(const QString &message);

private:
    void at_bindFinished(api::ResultCode result, const api::SystemData &systemData, const rest::QnConnectionPtr &connection);

public:
    std::unique_ptr<
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> connectionFactory;
    std::unique_ptr<api::Connection> cloudConnection;

    Q_DECLARE_PUBLIC(QnLinkToCloudDialog)
    Q_DECLARE_TR_FUNCTIONS(QnLinkToCloudDialogPrivate)
};

QnLinkToCloudDialog::QnLinkToCloudDialog(QWidget *parent)
    : base_type(parent)
    , ui(new Ui::QnLinkToCloudDialog)
    , d_ptr(new QnLinkToCloudDialogPrivate(this))
{
    ui->setupUi(this);

    Q_D(QnLinkToCloudDialog);

    connect(ui->accountLineEdit,    &QLineEdit::textChanged,            d,  &QnLinkToCloudDialogPrivate::updateButtonBox);
    connect(ui->passwordLineEdit,   &QLineEdit::textChanged,            d,  &QnLinkToCloudDialogPrivate::updateButtonBox);
    connect(ui->stackedWidget,      &QStackedWidget::currentChanged,    d,  &QnLinkToCloudDialogPrivate::updateButtonBox);
    connect(ui->buttonBox,          &QDialogButtonBox::accepted,        this,   &QnLinkToCloudDialog::accept);
    connect(ui->buttonBox,          &QDialogButtonBox::rejected,        this,   &QnLinkToCloudDialog::reject);

    const QString createAccountUrl = QnAppInfo::cloudPortalUrl() + kCreateAccountPath;
    const QString createAccountText = tr("Create account");
    ui->createAccountLabel->setText(lit("<a href=\"%2\">%1</a>").arg(createAccountText, createAccountUrl));
}

QnLinkToCloudDialog::~QnLinkToCloudDialog()
{
}

void QnLinkToCloudDialog::accept()
{
    if (ui->stackedWidget->currentWidget() == ui->credentialsPage)
    {
        Q_D(QnLinkToCloudDialog);
        d->bindSystem();
        return;
    }

    base_type::accept();
}

QnLinkToCloudDialogPrivate::QnLinkToCloudDialogPrivate(QnLinkToCloudDialog *parent)
    : QObject(parent)
    , q_ptr(parent)
    , connectionFactory(createConnectionFactory(), &destroyConnectionFactory)
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

void QnLinkToCloudDialogPrivate::updateButtonBox()
{
    Q_Q(QnLinkToCloudDialog);
    QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok;
    bool okEnabled = true;

    if (q->ui->stackedWidget->currentWidget() == q->ui->credentialsPage)
    {
        buttons |= QDialogButtonBox::Cancel;
        okEnabled = !q->ui->accountLineEdit->text().isEmpty() &&
                    !q->ui->passwordLineEdit->text().isEmpty();
        q->ui->createAccountLabel->show();
    }
    else
    {
        q->ui->createAccountLabel->hide();
    }

    q->ui->buttonBox->setStandardButtons(buttons);
    q->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(okEnabled);
}

void QnLinkToCloudDialogPrivate::bindSystem()
{
    auto serverConnection = getPublicServerConnection();
    if (!serverConnection)
    {
        openFailurePage(tr("None of your servers is connected to the Internet."));
        return;
    }

    Q_Q(QnLinkToCloudDialog);

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

void QnLinkToCloudDialogPrivate::openSuccessPage()
{
    Q_Q(QnLinkToCloudDialog);
    q->ui->successLabel->setText(tr("The system is successfully linked to %1").arg(q->ui->accountLineEdit->text()));
    q->ui->stackedWidget->setCurrentWidget(q->ui->successPage);
    updateButtonBox();
}

void QnLinkToCloudDialogPrivate::openFailurePage(const QString &message)
{
    Q_Q(QnLinkToCloudDialog);
    q->ui->errorMessageLabel->setText(message);
    q->ui->stackedWidget->setCurrentWidget(q->ui->failurePage);
    updateButtonBox();
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
            openFailurePage(tr("Invalid login or password."));
            break;
        default:
            openFailurePage(tr("Can not connect the system to the cloud account."));
            break;
        }
        return;
    }

    const auto &admin = qnResPool->getAdministrator();
    admin->setProperty(Qn::CLOUD_ACCOUNT_NAME, q->ui->accountLineEdit->text());
    propertyDictionary->saveParamsAsync(admin->getId());

    auto handleReply = [this](bool success, rest::Handle handleId, rest::ServerConnection::EmptyResponseType)
    {
        Q_UNUSED(handleId)

        if (success)
            openSuccessPage();
        else
            openFailurePage(tr("Can not save information to database"));
    };

    connection->saveCloudSystemCredentials(systemData.id.toString(), QString::fromStdString(systemData.authKey),
                                           handleReply, q->thread());
}
