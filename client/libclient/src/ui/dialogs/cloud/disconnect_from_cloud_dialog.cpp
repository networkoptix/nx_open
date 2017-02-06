#include "disconnect_from_cloud_dialog.h"

#include <api/global_settings.h>
#include <api/server_rest_connection.h>
#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <cloud/cloud_connection.h>

#include <client/client_settings.h>

#include <ui/actions/action_manager.h>
#include <ui/common/aligner.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/busy_indicator_button.h>
#include <ui/widgets/common/input_field.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/app_info.h>
#include <utils/common/delayed.h>

namespace {

/** Setup width manually to correctly handle word-wrapped labels. */
const int kDialogWidth = 400;

}

class QnDisconnectFromCloudDialogPrivate : public QObject, public QnWorkbenchContextAware
{
    QnDisconnectFromCloudDialog *q_ptr;
    Q_DECLARE_PUBLIC(QnDisconnectFromCloudDialog)
    Q_DECLARE_TR_FUNCTIONS(QnDisconnectFromCloudDialogPrivate)

public:
    enum class Scenario
    {
        Invalid,
        LocalOwner,
        CloudOwner,
        CloudOwnerOnly
    };

    QnDisconnectFromCloudDialogPrivate(QnDisconnectFromCloudDialog *parent);

    void lockUi(bool lock);
    void unbindSystem();
    void showFailure(const QString &message = QString());

    void setupUi();
    enum class VisibleButton
    {
        Ok,
        Next
    };

    void setVisibleButton(VisibleButton button);

    bool validateAuth();
private:
    Scenario calculateScenario() const;
    void createAuthorizeWidget();
    void createResetPasswordWidget();

    QString disconnectQuestionMessage() const;
    QString allUsersDisabledMessage() const;
    QString enterPasswordMessage() const;
    QString disconnectWarnMessage() const;

    void validateCloudPassword();
    void setupResetPasswordPage();
    void setupConfirmationPage();
public:
    const Scenario scenario;
    QWidget* authorizeWidget;
    QnInputField* authorizePasswordField;
    QWidget* resetPasswordWidget;
    QnInputField* resetPasswordField;
    QnInputField* confirmPasswordField;
    QnBusyIndicatorButton* nextButton;
    QnBusyIndicatorButton* okButton;
    bool unlinkedSuccessfully;

private:
    QHash<QString, bool> m_cloudPasswordCache;
    std::unique_ptr<nx::cdb::api::Connection> m_connection;
};

QnDisconnectFromCloudDialog::QnDisconnectFromCloudDialog(QWidget *parent):
    base_type(parent),
    d_ptr(new QnDisconnectFromCloudDialogPrivate(this))
{
    d_ptr->setupUi();
}

QnDisconnectFromCloudDialog::~QnDisconnectFromCloudDialog()
{
}

void QnDisconnectFromCloudDialog::accept()
{
    Q_D(QnDisconnectFromCloudDialog);

    if (d->unlinkedSuccessfully)
    {
        base_type::accept();
        return;
    }

    switch (d->scenario)
    {
        case QnDisconnectFromCloudDialogPrivate::Scenario::Invalid:
        {
            base_type::accept();
            break;
        }
        case QnDisconnectFromCloudDialogPrivate::Scenario::LocalOwner:
        {
            if (!d->validateAuth())
            {
                d->okButton->setFocus(Qt::OtherFocusReason);
                return;
            }
            else
            {
                d->unbindSystem();
            }
            break;
        }
        case QnDisconnectFromCloudDialogPrivate::Scenario::CloudOwner:
        {
            d->validateCloudPassword();
            break;
        }
        case QnDisconnectFromCloudDialogPrivate::Scenario::CloudOwnerOnly:
        {
            d->unbindSystem();
            break;
        }
        default:
            break;
    }
}

QnDisconnectFromCloudDialogPrivate::QnDisconnectFromCloudDialogPrivate(QnDisconnectFromCloudDialog *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    q_ptr(parent),
    scenario(calculateScenario()),
    authorizeWidget(nullptr),
    authorizePasswordField(nullptr),
    resetPasswordWidget(nullptr),
    resetPasswordField(nullptr),
    confirmPasswordField(nullptr),
    nextButton(nullptr),
    okButton(nullptr),
    unlinkedSuccessfully(false),
    m_connection(qnCloudConnectionProvider->createConnection())
{
    createAuthorizeWidget();
    createResetPasswordWidget();
}

void QnDisconnectFromCloudDialogPrivate::lockUi(bool lock)
{
    const bool enabled = !lock;

    authorizeWidget->setEnabled(enabled);

    okButton->setEnabled(enabled);
    okButton->showIndicator(lock);

    nextButton->setEnabled(enabled);
    nextButton->showIndicator(lock);
}

void QnDisconnectFromCloudDialogPrivate::unbindSystem()
{
    Q_Q(QnDisconnectFromCloudDialog);
    auto guard = QPointer<QnDisconnectFromCloudDialog>(q);
    auto handleReply =
        [this, guard](bool success, rest::Handle /*handleId*/, const QnRestResult& reply)
        {
            if (!guard)
                return;

            if (success && reply.error == QnRestResult::NoError)
            {
                unlinkedSuccessfully = true;
                guard->accept();
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

    lockUi(true);

    QString updatedPassword = scenario == Scenario::CloudOwnerOnly
        ? resetPasswordField->text()
        : QString();

    serverConnection->detachSystemFromCloud(updatedPassword, handleReply, q->thread());
}

void QnDisconnectFromCloudDialogPrivate::showFailure(const QString &message)
{
    Q_Q(QnDisconnectFromCloudDialog);

    QnMessageBox::critical(q,
        tr("Failed to disconnect the System from %1", "%1 is name of cloud (like 'Nx Cloud')")
            .arg(QnAppInfo::cloudName()),
        message);

    lockUi(false);
}

void QnDisconnectFromCloudDialogPrivate::setVisibleButton(VisibleButton button)
{
    const bool okButtonVisible = (button == VisibleButton::Ok);
    const auto style = static_cast<QnButtonAccent>(okButtonVisible
        ? QnButtonAccent::Warning
        : QnButtonAccent::Standard);
    const auto defaultButton = (okButtonVisible ? okButton : nextButton);

    okButton->setVisible(okButtonVisible);
    nextButton->setVisible(!okButtonVisible);

    Q_Q(QnDisconnectFromCloudDialog);
    q->setDefaultButton(defaultButton, style);
}

void QnDisconnectFromCloudDialogPrivate::setupUi()
{
    Q_Q(QnDisconnectFromCloudDialog);
    q->setFixedWidth(kDialogWidth);

    okButton = new QnBusyIndicatorButton(q);
    okButton->setText(tr("Disconnect"));
    q->addButton(okButton, QDialogButtonBox::AcceptRole);

    nextButton = new QnBusyIndicatorButton(q);
    nextButton->setText(tr("Next")); // Title from OS theme
    q->addButton(nextButton, QDialogButtonBox::ActionRole);

    setVisibleButton(VisibleButton::Ok);

    switch (scenario)
    {
        case Scenario::LocalOwner:
        {
            q->setIcon(QnMessageBoxIcon::Question);
            q->setText(disconnectQuestionMessage());
            q->setInformativeText(allUsersDisabledMessage()
                + L'\n'
                + enterPasswordMessage());
            q->addCustomWidget(authorizeWidget, QnMessageBox::Layout::Main,
                0, Qt::Alignment(), true);
            setVisibleButton(VisibleButton::Ok);
            break;
        }
        case Scenario::CloudOwner:
        {
            q->setIcon(QnMessageBoxIcon::Question);
            q->setText(disconnectQuestionMessage());
            q->setInformativeText(allUsersDisabledMessage()
                + L'\n'
                + disconnectWarnMessage()
                + L'\n'
                + enterPasswordMessage());
            q->addCustomWidget(authorizeWidget, QnMessageBox::Layout::Main,
                0, Qt::Alignment(), true);
            setVisibleButton(VisibleButton::Ok);
            break;
        }
        case Scenario::CloudOwnerOnly:
        {
            q->setText(enterPasswordMessage());
            q->addCustomWidget(authorizeWidget, QnMessageBox::Layout::Main,
                0, Qt::Alignment(), true);
            setVisibleButton(VisibleButton::Next);
            nextButton->disconnect(this);
            connect(nextButton, &QPushButton::clicked, this,
                &QnDisconnectFromCloudDialogPrivate::validateCloudPassword);
            break;
        }
        default:
            NX_ASSERT(false, "Invalid scenario");
            q->setIcon(QnMessageBoxIcon::Warning);
            q->setText(tr("Internal system error"));
            q->setStandardButtons(QDialogButtonBox::NoButton);
            q->setDefaultButton(okButton, QnButtonAccent::Warning);
            break;
    }

}

bool QnDisconnectFromCloudDialogPrivate::validateAuth()
{
    NX_ASSERT(authorizePasswordField);
    return authorizePasswordField->validate();
}

QString QnDisconnectFromCloudDialogPrivate::disconnectQuestionMessage() const
{
    return tr("Disconnect the System from %1?",
        "%1 is name of cloud (like 'Nx Cloud')").arg(QnAppInfo::cloudName());
}

QString QnDisconnectFromCloudDialogPrivate::allUsersDisabledMessage() const
{
    return tr("All cloud features will be disabled, cloud users will be deleted from the System.");
}

QString QnDisconnectFromCloudDialogPrivate::enterPasswordMessage() const
{
    return tr("Enter password to continue.");
}

QString QnDisconnectFromCloudDialogPrivate::disconnectWarnMessage() const
{
    return tr("You will be disconnected from this system and able to login again through local network with local account");
}

void QnDisconnectFromCloudDialogPrivate::validateCloudPassword()
{
    NX_ASSERT(scenario == Scenario::CloudOwnerOnly || scenario == Scenario::CloudOwner);
    const QString password = authorizePasswordField->text();

    m_connection->setCredentials(
        context()->user()->getName().toStdString(),
        password.toStdString());

    Q_Q(QnDisconnectFromCloudDialog);
    auto guard = QPointer<QnDisconnectFromCloudDialog>(q);
    auto handler =
        [guard, this, password](nx::cdb::api::ResultCode code, nx::cdb::api::AccountData /*data*/)
        {
            if (!guard)
                return;

            bool success = (code == nx::cdb::api::ResultCode::ok);
            executeDelayed(
                [guard, this, success, password]
                {
                    if (!guard)
                        return;

                    m_cloudPasswordCache[password] = success;
                    lockUi(false);
                    if (success)
                    {
                        if (scenario == Scenario::CloudOwnerOnly)
                            setupResetPasswordPage();
                        else
                            unbindSystem();
                    }
                    else
                    {
                        authorizePasswordField->validate();
                        if (scenario == Scenario::CloudOwnerOnly)
                            nextButton->setFocus(Qt::OtherFocusReason);
                        else
                            okButton->setFocus(Qt::OtherFocusReason);
                    }

                }, kDefaultDelay, guard->thread());
        };

    lockUi(true);
    m_connection->accountManager()->getAccount(handler);
}

void QnDisconnectFromCloudDialogPrivate::setupResetPasswordPage()
{
    NX_ASSERT(scenario == Scenario::CloudOwnerOnly);

    Q_Q(QnDisconnectFromCloudDialog);

    q->setText(tr("Set local owner password"));
    q->setInformativeText(
        tr("You wont be able to connect to this system with your %1 account after you disconnect this system from %1.",
            "%1 is name of cloud (like 'Nx Cloud')")
            .arg(QnAppInfo::cloudName()));

    authorizeWidget->hide(); /*< we are still parent of this widget to make sure it won't leak */
    q->removeCustomWidget(authorizeWidget);
    q->addCustomWidget(resetPasswordWidget, QnMessageBox::Layout::Main,
        0, Qt::Alignment(), true);
    nextButton->disconnect(this);
    connect(nextButton, &QPushButton::clicked, this,
        &QnDisconnectFromCloudDialogPrivate::setupConfirmationPage);
}

void QnDisconnectFromCloudDialogPrivate::setupConfirmationPage()
{
    nextButton->setFocus(Qt::OtherFocusReason);
    if (!resetPasswordField->isValid())
    {
        resetPasswordField->validate();
        return;
    }

    if (!confirmPasswordField->isValid())
    {
        confirmPasswordField->validate();
        return;
    }

    NX_ASSERT(scenario == Scenario::CloudOwnerOnly);
    Q_Q(QnDisconnectFromCloudDialog);

    q->setIcon(QnMessageBoxIcon::Question);
    q->setText(disconnectQuestionMessage());
    q->setInformativeText(allUsersDisabledMessage()
        + L'\n'
        + disconnectWarnMessage());
    q->removeCustomWidget(resetPasswordWidget);
    resetPasswordWidget->hide(); /*< we are still parent of this widget to make sure it won't leak */

    setVisibleButton(VisibleButton::Ok);
}

void QnDisconnectFromCloudDialogPrivate::createAuthorizeWidget()
{
    authorizeWidget = new QWidget();
    auto* layout = new QVBoxLayout(authorizeWidget);
    layout->setSpacing(style::Metrics::kDefaultLayoutSpacing.height());
    layout->setContentsMargins(style::Metrics::kDefaultTopLevelMargins);

    auto loginField = new QnInputField();
    loginField->setReadOnly(true);
    loginField->setTitle(tr("Login"));
    loginField->setText(context()->user()->getName());
    layout->addWidget(loginField);

    authorizePasswordField = new QnInputField();
    authorizePasswordField->setTitle(tr("Password"));
    authorizePasswordField->setEchoMode(QLineEdit::Password);
    authorizePasswordField->setValidator(
        [this](const QString& password)
        {
            auto user = context()->user();
            NX_ASSERT(user);
            if (!user)
                return Qn::ValidationResult(tr("Internal Error"));

            switch (scenario)
            {
                case Scenario::LocalOwner:
                {
                    NX_ASSERT(user->isLocal());
                    return user->checkLocalUserPassword(password)
                        ? Qn::kValidResult
                        : Qn::ValidationResult(tr("Wrong Password"));
                }
                case Scenario::CloudOwner:
                case Scenario::CloudOwnerOnly:
                {
                    NX_ASSERT(user->isCloud());
                    if (!m_cloudPasswordCache.contains(password))
                        return Qn::kValidResult;

                    return m_cloudPasswordCache[password]
                        ? Qn::kValidResult
                        : Qn::ValidationResult(tr("Wrong Password"));
                }
                default:
                    break;
            }
            NX_ASSERT(false, "Should never get here");
            return Qn::ValidationResult(tr("Internal Error"));
        });

    authorizeWidget->setFocusPolicy(Qt::TabFocus);
    authorizeWidget->setFocusProxy(authorizePasswordField);

    layout->addWidget(authorizePasswordField);

    QnAligner* aligner = new QnAligner(authorizeWidget);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
    aligner->addWidget(loginField);
    aligner->addWidget(authorizePasswordField);
}

void QnDisconnectFromCloudDialogPrivate::createResetPasswordWidget()
{
    resetPasswordWidget = new QWidget();
    auto* layout = new QVBoxLayout(resetPasswordWidget);
    layout->setSpacing(style::Metrics::kDefaultLayoutSpacing.height());
    layout->setContentsMargins(style::Metrics::kDefaultTopLevelMargins);

    auto owner = qnResPool->getAdministrator();
    NX_ASSERT(owner);

    auto loginField = new QnInputField(resetPasswordWidget);
    loginField->setTitle(tr("Login"));
    loginField->setReadOnly(true);
    loginField->setText(owner->getName());
    layout->addWidget(loginField);
    layout->addSpacing(style::Metrics::kDefaultLayoutSpacing.height());

    resetPasswordField = new QnInputField();
    resetPasswordField->setTitle(tr("Password"));
    resetPasswordField->setEchoMode(QLineEdit::Password);
    resetPasswordField->setPasswordIndicatorEnabled(true);
    resetPasswordField->setValidator(Qn::defaultPasswordValidator(false));
    layout->addWidget(resetPasswordField);

    confirmPasswordField = new QnInputField();
    confirmPasswordField->setTitle(tr("Confirm Password"));
    confirmPasswordField->setEchoMode(QLineEdit::Password);
    confirmPasswordField->setValidator(Qn::defaultConfirmationValidator(
        [this]() { return resetPasswordField->text(); },
        tr("Passwords do not match.")));
    layout->addWidget(confirmPasswordField);

    connect(resetPasswordField, &QnInputField::textChanged, this, [this]()
    {
        if (!confirmPasswordField->text().isEmpty())
            confirmPasswordField->validate();
    });

    connect(resetPasswordField, &QnInputField::editingFinished,
        confirmPasswordField, &QnInputField::validate);

    resetPasswordWidget->setFocusPolicy(Qt::TabFocus);
    resetPasswordWidget->setFocusProxy(resetPasswordField);

    QnAligner* aligner = new QnAligner(resetPasswordWidget);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
    aligner->addWidgets({
        loginField,
        resetPasswordField,
        confirmPasswordField });
}

QnDisconnectFromCloudDialogPrivate::Scenario QnDisconnectFromCloudDialogPrivate::calculateScenario() const
{
    auto user = context()->user();

    if (!user || user->userRole() != Qn::UserRole::Owner)
        return Scenario::Invalid;

    if (user->isLocal())
        return Scenario::LocalOwner;

    auto localOwners = qnResPool->getResources().filtered<QnUserResource>(
        [](const QnUserResourcePtr& user)
        {
            return !user->isCloud()
                && user->userRole() == Qn::UserRole::Owner;
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
