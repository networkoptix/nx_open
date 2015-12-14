#include "unlink_from_cloud_dialog.h"
#include "ui_unlink_from_cloud_dialog.h"

#include <cdb/connection.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/user_resource.h>
#include <core/resource/param.h>
#include <utils/common/delayed.h>
#include <client/client_settings.h>

using namespace nx::cdb;

class QnUnlinkFromCloudDialogPrivate : public QObject
{
    QnUnlinkFromCloudDialog *q_ptr;
    Q_DECLARE_PUBLIC(QnUnlinkFromCloudDialog)
    Q_DECLARE_TR_FUNCTIONS(QnUnlinkFromCloudDialogPrivate)

    std::unique_ptr<api::ConnectionFactory, decltype(&destroyConnectionFactory)> connectionFactory;
    std::unique_ptr<api::Connection> cloudConnection;

public:
    QnUnlinkFromCloudDialogPrivate(QnUnlinkFromCloudDialog *parent);

    void unbindSystem();
    void openFailurePage(const QString &message);

private:
    void at_unbindFinished(api::ResultCode result);
};

QnUnlinkFromCloudDialog::QnUnlinkFromCloudDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::QnUnlinkFromCloudDialog)
    , d_ptr(new QnUnlinkFromCloudDialogPrivate(this))
{
    ui->setupUi(this);

    connect(ui->buttonBox,          &QDialogButtonBox::accepted,        this,   &QnUnlinkFromCloudDialog::accept);
    connect(ui->buttonBox,          &QDialogButtonBox::rejected,        this,   &QnUnlinkFromCloudDialog::reject);
}

QnUnlinkFromCloudDialog::~QnUnlinkFromCloudDialog()
{
}

void QnUnlinkFromCloudDialog::accept()
{
    if (ui->stackedWidget->currentWidget() == ui->confirmationPage)
    {
        Q_D(QnUnlinkFromCloudDialog);
        d->unbindSystem();
        return;
    }

    base_type::accept();
}

QnUnlinkFromCloudDialogPrivate::QnUnlinkFromCloudDialogPrivate(QnUnlinkFromCloudDialog *parent)
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

void QnUnlinkFromCloudDialogPrivate::unbindSystem()
{
    QnUserResourcePtr adminUser = qnResPool->getAdministrator();

    std::string systemId = adminUser->getProperty(Qn::CLOUD_SYSTEM_ID).toStdString();

    cloudConnection = connectionFactory->createConnection(
            systemId,
            adminUser->getProperty(Qn::CLOUD_SYSTEM_AUTH_KEY).toStdString());

    Q_Q(QnUnlinkFromCloudDialog);

    q->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    cloudConnection->systemManager()->unbindSystem(
            qnResPool->getAdministrator()->getProperty(Qn::CLOUD_SYSTEM_ID).toStdString(),
            [this](api::ResultCode result)
    {
        Q_Q(QnUnlinkFromCloudDialog);
        executeDelayed(
                [this, result]()
                {
                    at_unbindFinished(result);
                },
                0, q->thread()
        );
    });
}

void QnUnlinkFromCloudDialogPrivate::openFailurePage(const QString &message)
{
    Q_Q(QnUnlinkFromCloudDialog);
    q->ui->errorMessageLabel->setText(message);
    q->ui->buttonBox->setStandardButtons(QDialogButtonBox::Ok);
    q->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    q->ui->stackedWidget->setCurrentWidget(q->ui->failurePage);
}

void QnUnlinkFromCloudDialogPrivate::at_unbindFinished(api::ResultCode result)
{
    if (result != api::ResultCode::ok)
    {
        openFailurePage(tr("Can not disconnect the system from the cloud account."));
        return;
    }

    //TODO #dklychkov removing properties from settings

    const auto admin = qnResPool->getAdministrator();

    admin->setProperty(Qn::CLOUD_SYSTEM_ID, QVariant());
    admin->setProperty(Qn::CLOUD_SYSTEM_AUTH_KEY, QVariant());

    int requestId = propertyDictionary->saveParamsAsync(admin->getId());
    connect(propertyDictionary, &QnResourcePropertyDictionary::asyncSaveDone, this,
            [this, requestId](int reqId, ec2::ErrorCode errorCode)
            {
                if (reqId != requestId)
                    return;

                disconnect(propertyDictionary, nullptr, this, nullptr);

                if (errorCode == ec2::ErrorCode::ok)
                    return;

                openFailurePage(tr("Can not remove settings from the database."));
            });
}
