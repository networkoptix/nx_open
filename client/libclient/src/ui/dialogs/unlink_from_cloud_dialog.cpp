#include "unlink_from_cloud_dialog.h"

#include <api/global_settings.h>
#include <api/server_rest_connection.h>
#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <client/client_settings.h>

#include <ui/actions/action_manager.h>
#include <ui/common/aligner.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/input_field.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/app_info.h>
#include <utils/common/delayed.h>

namespace {

/** Setup width manually to correctly handle word-wrapped labels. */
const int kDialogWidth = 400;

const int kWidgetSpacing = 8;
const QMargins kWidgetMargins(8, 8, 8, 8);

}

class QnUnlinkFromCloudDialogPrivate : public QObject, public QnWorkbenchContextAware
{
    QnUnlinkFromCloudDialog *q_ptr;
    Q_DECLARE_PUBLIC(QnUnlinkFromCloudDialog)
    Q_DECLARE_TR_FUNCTIONS(QnUnlinkFromCloudDialogPrivate)

public:
    enum class Scenario
    {
        Invalid,
        LocalOwner,
        CloudOwner,
        CloudOwnerOnly
    };

    QnUnlinkFromCloudDialogPrivate(QnUnlinkFromCloudDialog *parent);

    void lockUi(bool lock);
    void unbindSystem();
    void showFailure(const QString &message = QString());

    void setupUi();

private:
    Scenario calculateScenario() const;
    QWidget* createAuthorizeWidget() const;
    QWidget* createResetPasswordWidget() const;

    QString allUsersDisabledMessage() const;
    QString enterPasswordMessage() const;
    QString disconnectWarnMessage() const;

    void setupResetPasswordPage();
    void setupConfirmationPage();
public:
    const Scenario scenario;
    QWidget* authorizeWidget;
    QWidget* resetPasswordWidget;
    QPushButton* nextButton;
    bool unlinkedSuccessfully;
};

QnUnlinkFromCloudDialog::QnUnlinkFromCloudDialog(QWidget *parent):
    base_type(parent),
    d_ptr(new QnUnlinkFromCloudDialogPrivate(this))
{
    d_ptr->setupUi();



    /*
    1) ask cloud username and password
    2) if logged as local - OK
    3) else
        3.1) if there is enabled local owner - warn, disconnect and OK
        3.2) else - create local owner, then warn, disconnect and OK
    */

}

QnUnlinkFromCloudDialog::~QnUnlinkFromCloudDialog()
{
}

void QnUnlinkFromCloudDialog::accept()
{
    Q_D(QnUnlinkFromCloudDialog);

    if (d->scenario != QnUnlinkFromCloudDialogPrivate::Scenario::Invalid
        && !d->unlinkedSuccessfully)
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
    QnWorkbenchContextAware(parent),
    q_ptr(parent),
    scenario(calculateScenario()),
    authorizeWidget(createAuthorizeWidget()),
    resetPasswordWidget(createResetPasswordWidget()),
    nextButton(nullptr),
    unlinkedSuccessfully(false)
{


}

void QnUnlinkFromCloudDialogPrivate::lockUi(bool lock)
{
    Q_Q(QnUnlinkFromCloudDialog);

    const bool enabled = !lock;

    for (auto button: q->buttons())
        button->setEnabled(enabled);
}

void QnUnlinkFromCloudDialogPrivate::unbindSystem()
{
    Q_Q(QnUnlinkFromCloudDialog);
    q->accept();
    return;

    lockUi(true);

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

void QnUnlinkFromCloudDialogPrivate::setupUi()
{
    Q_Q(QnUnlinkFromCloudDialog);
    q->setWindowTitle(tr("Disconnect from %1").arg(QnAppInfo::cloudName()));
    q->setMinimumWidth(kDialogWidth);
    q->setMaximumWidth(kDialogWidth);

    switch (scenario)
    {
        case Scenario::LocalOwner:
        {
            q->setIcon(QnMessageBox::Question);
            q->setText(tr("Disconnect system from %1").arg(QnAppInfo::cloudName()));
            q->setInformativeText(allUsersDisabledMessage()
                + L'\n'
                + enterPasswordMessage(),
                false);
            q->addCustomWidget(authorizeWidget, QnMessageBox::Layout::Main);
            q->setDefaultButton(QDialogButtonBox::Ok);
            break;
        }
        case Scenario::CloudOwner:
        {
            q->setIcon(QnMessageBox::Question);
            q->setText(tr("Disconnect system from %1").arg(QnAppInfo::cloudName()));
            q->setInformativeText(allUsersDisabledMessage()
                + L'\n'
                + disconnectWarnMessage()
                + L'\n'
                + enterPasswordMessage());
            q->addCustomWidget(authorizeWidget, QnMessageBox::Layout::Main);
            q->setDefaultButton(QDialogButtonBox::Ok);
            break;
        }
        case Scenario::CloudOwnerOnly:
        {
            q->setText(enterPasswordMessage());
            q->addCustomWidget(authorizeWidget, QnMessageBox::Layout::Main);
            q->setStandardButtons(QDialogButtonBox::NoButton);
            auto cancelButton = q->addButton(QDialogButtonBox::tr("Cancel"), QDialogButtonBox::HelpRole);
            QObject::connect(cancelButton, &QPushButton::clicked, q, &QnMessageBox::reject);
            if (!nextButton)
                nextButton = q->addButton(tr("Next"), QDialogButtonBox::ActionRole);
            disconnect(nextButton, nullptr, this, nullptr);
            connect(nextButton, &QPushButton::clicked, this,
                &QnUnlinkFromCloudDialogPrivate::setupResetPasswordPage);
            q->setDefaultButton(nextButton);
            break;
        }
        default:
            NX_ASSERT(false, "Invalid scenario");
            q->setIcon(QnMessageBox::Warning);
            q->setText(tr("Internal system error"));
            q->setStandardButtons(QDialogButtonBox::Ok);
            q->setDefaultButton(QDialogButtonBox::Ok);
            break;
    }

}

QString QnUnlinkFromCloudDialogPrivate::allUsersDisabledMessage() const
{
    return tr("All cloud users and features will be disabled.");
}

QString QnUnlinkFromCloudDialogPrivate::enterPasswordMessage() const
{
    return tr("Enter password to continue.");
}

QString QnUnlinkFromCloudDialogPrivate::disconnectWarnMessage() const
{
    return tr("You will be disconnected from this system and able to login again through local network with local account");
}

void QnUnlinkFromCloudDialogPrivate::setupResetPasswordPage()
{
    NX_ASSERT(scenario == Scenario::CloudOwnerOnly);
    Q_Q(QnUnlinkFromCloudDialog);

    q->setText(tr("Reset admin password"));
    q->setInformativeText(
        tr("You wont be able to connect to this system with your cloud account after you disconnect this system from %1.")
            .arg(QnAppInfo::cloudName())
        + L'\n'
        + tr("Enter new password for the local administrator.")
    );

    authorizeWidget->hide(); /*< we are still parent of this widget to make sure it won't leak */
    q->removeCustomWidget(authorizeWidget);
    q->addCustomWidget(resetPasswordWidget, QnMessageBox::Layout::Main);
    disconnect(nextButton, nullptr, this, nullptr);
    connect(nextButton, &QPushButton::clicked, this,
        &QnUnlinkFromCloudDialogPrivate::setupConfirmationPage);
}

void QnUnlinkFromCloudDialogPrivate::setupConfirmationPage()
{
    NX_ASSERT(scenario == Scenario::CloudOwnerOnly);
    Q_Q(QnUnlinkFromCloudDialog);

    q->setIcon(QnMessageBox::Question);
    q->setText(tr("Disconnect system from %1").arg(QnAppInfo::cloudName()));
    q->setInformativeText(allUsersDisabledMessage()
        + L'\n'
        + disconnectWarnMessage());
    q->removeCustomWidget(resetPasswordWidget);
    resetPasswordWidget->hide(); /*< we are still parent of this widget to make sure it won't leak */
    delete nextButton;
    nextButton = nullptr;
    q->setStandardButtons(QDialogButtonBox::Ok);
    q->setDefaultButton(QDialogButtonBox::Ok);
}

QWidget* QnUnlinkFromCloudDialogPrivate::createAuthorizeWidget() const
{
    QWidget* result = new QWidget();
    auto* layout = new QVBoxLayout(result);
    layout->setSpacing(kWidgetSpacing);
    layout->setContentsMargins(kWidgetMargins);

    auto loginField = new QnInputField();
    loginField->setReadOnly(true);
    loginField->setTitle(tr("Login"));
    loginField->setText(qnGlobalSettings->cloudAccountName());
    layout->addWidget(loginField);

    auto passwordField = new QnInputField();
    passwordField->setTitle(tr("Password"));
    passwordField->setEchoMode(QLineEdit::Password);
    layout->addWidget(passwordField);

    QnAligner* aligner = new QnAligner(result);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
    aligner->addWidget(loginField);
    aligner->addWidget(passwordField);

    return result;
}

QWidget* QnUnlinkFromCloudDialogPrivate::createResetPasswordWidget() const
{
    QWidget* result = new QWidget();
    auto* layout = new QVBoxLayout(result);
    layout->setSpacing(kWidgetSpacing);
    layout->setContentsMargins(kWidgetMargins);

    auto passwordField = new QnInputField();
    passwordField->setTitle(tr("Password"));
    passwordField->setEchoMode(QLineEdit::Password);
    layout->addWidget(passwordField);

    auto confirmPasswordField = new QnInputField();
    confirmPasswordField->setTitle(tr("Confirm Password"));
    confirmPasswordField->setEchoMode(QLineEdit::Password);
    layout->addWidget(confirmPasswordField);

    QnAligner* aligner = new QnAligner(result);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
    aligner->addWidget(passwordField);
    aligner->addWidget(confirmPasswordField);

    return result;
}

QnUnlinkFromCloudDialogPrivate::Scenario QnUnlinkFromCloudDialogPrivate::calculateScenario() const
{
    auto user = context()->user();

    if (!user || user->role() != Qn::UserRole::Owner)
        return Scenario::Invalid;

    if (user->isLocal())
        return Scenario::LocalOwner;

    auto localOwners = qnResPool->getResources().filtered<QnUserResource>(
        [](const QnUserResourcePtr& user)
        {
            return !user->isCloud()
                && user->role() == Qn::UserRole::Owner;
        });
    NX_ASSERT(!localOwners.empty(), "At least 'admin' user must exist");

    /* If there is at least on enabled local owner, just disconnect. */
    if (std::any_of(localOwners.cbegin(), localOwners.cend(),
        [](const QnUserResourcePtr& user)
        {
            return user->isEnabled();
        }))
        return Scenario::CloudOwner;

    /* Otherwise we must ask for new 'admin' password. */
    return Scenario::CloudOwnerOnly;
}
