#include "unlink_from_cloud_dialog.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/user_resource.h>
#include <core/resource/param.h>
#include <cdb/connection.h>

#include <utils/common/delayed.h>

#include <client/client_settings.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

using namespace nx::cdb;

class QnUnlinkFromCloudDialogPrivate : public QObject
{
    QnUnlinkFromCloudDialog *q_ptr;
    Q_DECLARE_PUBLIC(QnUnlinkFromCloudDialog)
    Q_DECLARE_TR_FUNCTIONS(QnUnlinkFromCloudDialogPrivate)

    std::unique_ptr<api::ConnectionFactory, decltype(&destroyConnectionFactory)> connectionFactory;
    std::unique_ptr<api::Connection> cloudConnection;
    bool unlinkedSuccessfully;

public:
    QnUnlinkFromCloudDialogPrivate(QnUnlinkFromCloudDialog *parent);

    void lockUi(bool lock);
    void unbindSystem();
    void showFailure(const QString &message = QString());

private:
    void at_unbindFinished(api::ResultCode result);
};

QnUnlinkFromCloudDialog::QnUnlinkFromCloudDialog(QWidget *parent)
    : base_type(parent)
    , d_ptr(new QnUnlinkFromCloudDialogPrivate(this))
{
    setWindowTitle(tr("Unlink from cloud"));
    setText(tr("You are going to disconnect this system from the cloud account."));
    setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    setDefaultButton(QDialogButtonBox::Ok);
    //TODO: #dklychkov set help topic
}

QnUnlinkFromCloudDialog::~QnUnlinkFromCloudDialog()
{
}

void QnUnlinkFromCloudDialog::accept()
{
    Q_D(QnUnlinkFromCloudDialog);

    if (!d->unlinkedSuccessfully)
    {
        d->unbindSystem();
        return;
    }

    base_type::accept();
}

QnUnlinkFromCloudDialogPrivate::QnUnlinkFromCloudDialogPrivate(QnUnlinkFromCloudDialog *parent)
    : QObject(parent)
    , q_ptr(parent)
    , connectionFactory(createConnectionFactory(), &destroyConnectionFactory)
    , unlinkedSuccessfully(false)
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

void QnUnlinkFromCloudDialogPrivate::lockUi(bool lock)
{
    Q_Q(QnUnlinkFromCloudDialog);

    const bool enabled = !lock;

    q->button(QDialogButtonBox::Ok)->setEnabled(enabled);
}

void QnUnlinkFromCloudDialogPrivate::unbindSystem()
{
    lockUi(true);

    QnUserResourcePtr adminUser = qnResPool->getAdministrator();

    std::string systemId = adminUser->getProperty(Qn::CLOUD_SYSTEM_ID).toStdString();

    cloudConnection = connectionFactory->createConnection(
            systemId,
            adminUser->getProperty(Qn::CLOUD_SYSTEM_AUTH_KEY).toStdString());

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

void QnUnlinkFromCloudDialogPrivate::showFailure(const QString &message)
{
    Q_Q(QnUnlinkFromCloudDialog);

    QnMessageBox messageBox(QnMessageBox::NoIcon,
                            helpTopic(q),
                            tr("Error"),
                            tr("Can not unlink the system from the cloud"),
                            QDialogButtonBox::Ok,
                            q);

    if (!message.isEmpty())
        messageBox.setInformativeText(message);

    messageBox.exec();

    lockUi(false);
}

void QnUnlinkFromCloudDialogPrivate::at_unbindFinished(api::ResultCode result)
{
    if (result != api::ResultCode::ok)
    {
        showFailure();
        return;
    }

    //TODO #dklychkov removing properties from settings

    Q_Q(QnUnlinkFromCloudDialog);

    const auto admin = qnResPool->getAdministrator();
    if (!admin)
    {
        q->reject();
        return;
    }

    admin->setProperty(Qn::CLOUD_SYSTEM_ID, QVariant());
    admin->setProperty(Qn::CLOUD_SYSTEM_AUTH_KEY, QVariant());

    int requestId = propertyDictionary->saveParamsAsync(admin->getId());
    connect(propertyDictionary, &QnResourcePropertyDictionary::asyncSaveDone, this,
            [this, requestId](int reqId, ec2::ErrorCode errorCode)
            {
                if (reqId != requestId)
                    return;

                disconnect(propertyDictionary, nullptr, this, nullptr);

                if (errorCode != ec2::ErrorCode::ok)
                {
                    showFailure(tr("Can not remove settings from the database."));
                    return;
                }

                unlinkedSuccessfully = true;
                Q_Q(QnUnlinkFromCloudDialog);
                q->accept();
            });
}
