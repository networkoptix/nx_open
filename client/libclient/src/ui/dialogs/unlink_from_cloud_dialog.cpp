#include "unlink_from_cloud_dialog.h"
#include "ui_unlink_from_cloud_dialog.h"

#include <api/global_settings.h>
#include <api/server_rest_connection.h>
#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>


#include <client/client_settings.h>

#include <ui/actions/action_manager.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/skin.h>


#include <utils/common/app_info.h>
#include <utils/common/delayed.h>

class QnUnlinkFromCloudDialogPrivate : public QObject
{
    QnUnlinkFromCloudDialog *q_ptr;
    Q_DECLARE_PUBLIC(QnUnlinkFromCloudDialog)
    Q_DECLARE_TR_FUNCTIONS(QnUnlinkFromCloudDialogPrivate)

public:
    QnUnlinkFromCloudDialogPrivate(QnUnlinkFromCloudDialog *parent);

    void lockUi(bool lock);
    void unbindSystem();
    void showFailure(const QString &message = QString());

    bool unlinkedSuccessfully;
};

QnUnlinkFromCloudDialog::QnUnlinkFromCloudDialog(QWidget *parent):
    base_type(parent, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::UnlinkFromCloudDialog()),
    d_ptr(new QnUnlinkFromCloudDialogPrivate(this))
{
    ui->setupUi(this);

    QPixmap pixmap = qnSkin->pixmap(lit("standard_icons/messagebox_question.png"));
    //ui->iconLabel->setVisible(!pixmap.isNull());
    ui->iconLabel->setPixmap(pixmap);
    ui->iconLabel->resize(pixmap.size());

    setWindowTitle(tr("Disconnect from %1").arg(QnAppInfo::cloudName()));

    /*
    1) ask cloud username and password
    2) if logged as local - OK
    3) else
        3.1) if there is enabled local owner - warn, disconnect and OK
        3.2) else - create local owner, then warn, disconnect and OK
    */



//     if (d_ptr->loggedAsCloudAccount())
//     {
//
//
//         ui->mainLabel->setText(tr("You are going to disconnect this system from cloud account you logged with."));
//         ui->informativeLabel->setText(tr("You will be disconnected from this system. To connect again, you'll have to use local admin account"));
//     }
//     else
//     {
//         ui->mainLabel->setText(tr("You are going to disconnect this system from the cloud account."));
//     }

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

//     if (d->loggedAsCloudAccount())
//         menu()->trigger(QnActions::DisconnectAction);
    base_type::accept();
}

QnUnlinkFromCloudDialogPrivate::QnUnlinkFromCloudDialogPrivate(QnUnlinkFromCloudDialog *parent):
    QObject(parent),
    q_ptr(parent),
    unlinkedSuccessfully(false)
{
}

void QnUnlinkFromCloudDialogPrivate::lockUi(bool lock)
{
    Q_Q(QnUnlinkFromCloudDialog);

    const bool enabled = !lock;

    q->buttonBox()->button(QDialogButtonBox::Ok)->setEnabled(enabled);
}

void QnUnlinkFromCloudDialogPrivate::unbindSystem()
{
    lockUi(true);

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
