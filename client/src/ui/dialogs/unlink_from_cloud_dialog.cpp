#include "unlink_from_cloud_dialog.h"

#include <api/global_settings.h>
#include <api/server_rest_connection.h>

#include <cdb/connection.h>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>

#include <client/client_settings.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <utils/common/delayed.h>

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

    std::string systemId = qnGlobalSettings->cloudSystemID().toStdString();

    cloudConnection = connectionFactory->createConnection(
            systemId,
            qnGlobalSettings->cloudAuthKey().toStdString());

    cloudConnection->systemManager()->unbindSystem(
            systemId,
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

    Q_Q(QnUnlinkFromCloudDialog);
    auto handleReply = [this, q](bool success, rest::Handle handleId, const QnRestResult& reply)
    {
        Q_UNUSED(handleId);
        if (success && reply.error == QnRestResult::NoError)
        {
            unlinkedSuccessfully = true;
            q->accept();
        }
        else
        {
            showFailure(reply.errorString);
        }
    };

    if (!qnCommon->currentServer())
        return;

    auto serverConnection = qnCommon->currentServer()->restConnection();
    if (!serverConnection)
        return;

    serverConnection->resetCloudSystemCredentials(handleReply, q->thread());
}
