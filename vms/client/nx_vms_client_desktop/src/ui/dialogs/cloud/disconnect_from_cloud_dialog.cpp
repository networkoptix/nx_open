#include "disconnect_from_cloud_dialog.h"

#include <list>

#include <api/global_settings.h>
#include <api/server_rest_connection.h>
#include <api/app_server_connection.h>
#include <client_core/client_core_module.h>
#include <client/client_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator_button.h>
#include <nx/vms/client/desktop/common/widgets/input_field.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/app_info.h>
#include <utils/common/delayed.h>

#include <nx/network/app_info.h>

using namespace nx::vms::client::desktop;
using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

namespace {

using namespace std::chrono_literals;

// Limits for user lockout.
// This sort of user lockout happens only for Scenario::LocalOwner scenario.
// Number of failed attemts to be tracked for timing. User will be locked out if
// it makes following number of attempts in a specified period of time.
const int kInvalidAttemptsKept = 5;
// Time limit for invalid attempts. Is used with kInvalidAttemptsKeeped.
const auto kInvalidAttemptsPeriod = 60s;
// Lockout activates after this number of attempts. It is irrelevant to time checks.
// NOTE: right now maximum allowed attempts is equal to attempts tracked, but this
// numbers can be changed in future.
const int kMaxInvalidAttemptsAllowed = 5;
// Period of user lockout.
const auto kUserLockoutPeriod = 60s;

// NOTE: The following values should be kept between several instances of
// DisconnectFromCloudDialog. It prevents user from skipping local lockout
// by reopening this dialog.

// Time points of the invalid password attempts.
// We keep max of kInvalidAttemptsKeeped attempts here.
std::list<TimePoint> invalidAttempts;
// Total number of invalid attemts.
int totalInvalidAttempts = 0;

// Time of start of a lockout.
TimePoint lockoutEndTime;
bool lockoutActivated = false;

TimePoint::duration getTotalDuration(const std::list<TimePoint>& timepoints)
{
    if (timepoints.size() <= 1)
        return 0s;
    return timepoints.back() - timepoints.front();
}

} // namespace

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

    void onCloudPasswordValidated(
        CredentialCheckResult result,
        const QString& password);

    ValidationResult getValidationResult(CredentialCheckResult check);
    // Activates lockout timer.
    void activateLocalLockoutMode(TimePoint end);

    void at_lockoutExpired();

public:
    const Scenario scenario;
    QWidget* authorizeWidget;
    InputField* authorizePasswordField;
    QWidget* resetPasswordWidget;
    InputField* resetPasswordField;
    InputField* confirmPasswordField;
    BusyIndicatorButton* nextButton;
    BusyIndicatorButton* okButton;
    bool unlinkedSuccessfully;

private:
    QHash<QString, CredentialCheckResult> m_passwordCache;
};

QnDisconnectFromCloudDialog::QnDisconnectFromCloudDialog(QWidget *parent):
    base_type(parent),
    d_ptr(new QnDisconnectFromCloudDialogPrivate(this))
{
    d_ptr->setupUi();

    Q_D(QnDisconnectFromCloudDialog);
    connect(this, &QnDisconnectFromCloudDialog::cloudPasswordValidated,
        d, &QnDisconnectFromCloudDialogPrivate::onCloudPasswordValidated, Qt::QueuedConnection);
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
    unlinkedSuccessfully(false)
{
    qRegisterMetaType<CredentialCheckResult>("CredentialCheckResult");
    createAuthorizeWidget();
    createResetPasswordWidget();

    if (lockoutActivated)
        activateLocalLockoutMode(lockoutEndTime);
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

    if (!commonModule()->currentServer())
        return;

    auto serverConnection = commonModule()->currentServer()->restConnection();
    if (!serverConnection)
        return;

    lockUi(true);

    QString updatedPassword = scenario == Scenario::CloudOwnerOnly
        ? resetPasswordField->text()
        : QString();
    const QString currentPassword = authorizePasswordField->text();

    serverConnection->detachSystemFromCloud(
        currentPassword, updatedPassword, handleReply, q->thread());
}

void QnDisconnectFromCloudDialogPrivate::showFailure(const QString &message)
{
    Q_Q(QnDisconnectFromCloudDialog);

    QnMessageBox::critical(q,
        tr("Failed to disconnect System from %1", "%1 is the cloud name (like Nx Cloud)")
            .arg(nx::network::AppInfo::cloudName()),
        message);

    lockUi(false);
}

void QnDisconnectFromCloudDialogPrivate::setVisibleButton(VisibleButton button)
{
    const bool okButtonVisible = (button == VisibleButton::Ok);
    const auto style = okButtonVisible
        ? Qn::ButtonAccent::Warning
        : Qn::ButtonAccent::Standard;
    const auto defaultButton = (okButtonVisible ? okButton : nextButton);

    okButton->setVisible(okButtonVisible);
    nextButton->setVisible(!okButtonVisible);

    Q_Q(QnDisconnectFromCloudDialog);
    q->setDefaultButton(defaultButton, style);
}

void QnDisconnectFromCloudDialogPrivate::setupUi()
{
    Q_Q(QnDisconnectFromCloudDialog);
    q->setStandardButtons(QDialogButtonBox::Cancel);

    okButton = new BusyIndicatorButton(q);
    okButton->setText(tr("Disconnect"));
    q->addButton(okButton, QDialogButtonBox::AcceptRole);

    nextButton = new BusyIndicatorButton(q);
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
            q->setText(lit("Internal system error"));
            q->setStandardButtons(QDialogButtonBox::NoButton);
            q->setDefaultButton(okButton, Qn::ButtonAccent::Warning);
            break;
    }

}

bool QnDisconnectFromCloudDialogPrivate::validateAuth()
{
    NX_ASSERT(authorizePasswordField);

    auto user = context()->user();
    NX_ASSERT(user);
    if (!user)
        return false;

    CredentialCheckResult result = CredentialCheckResult::Ok;
    QString password = authorizePasswordField->text();

    if (!user->checkLocalUserPassword(password))
    {
        result = CredentialCheckResult::NotAuthorized;
        const auto now = TimePoint::clock::now();

        // Limiting maximum number of invalid attempts.
        totalInvalidAttempts++;
        if (totalInvalidAttempts > kMaxInvalidAttemptsAllowed)
            result = CredentialCheckResult::UserLockedOut;

        invalidAttempts.push_back(now);
        // Limiting the rate, at which the user can check passwords.
        if (invalidAttempts.size() > kInvalidAttemptsKept)
        {
            invalidAttempts.pop_front();
            // Not allowing to try another passwords too fast.
            if (getTotalDuration(invalidAttempts) >= kInvalidAttemptsPeriod)
                result = CredentialCheckResult::UserLockedOut;
        }
    }

    if (result == CredentialCheckResult::UserLockedOut && !lockoutActivated)
        activateLocalLockoutMode(TimePoint::clock::now() + kUserLockoutPeriod);

    m_passwordCache[password] = result;

    return authorizePasswordField->validate();
}

QString QnDisconnectFromCloudDialogPrivate::disconnectQuestionMessage() const
{
    return tr("Disconnect System from %1?",
        "%1 is the cloud name (like Nx Cloud)").arg(nx::network::AppInfo::cloudName());
}

QString QnDisconnectFromCloudDialogPrivate::allUsersDisabledMessage() const
{
    return setWarningStyleHtml(tr("All %1 users will be deleted.",
        "%1 is the short cloud name (like Cloud)")
        .arg(nx::network::AppInfo::shortCloudName()));
}

QString QnDisconnectFromCloudDialogPrivate::enterPasswordMessage() const
{
    return tr("Enter password to continue.");
}

QString QnDisconnectFromCloudDialogPrivate::disconnectWarnMessage() const
{
    return tr("You will be disconnected from this System and able to login again through local network with local account");
}

void QnDisconnectFromCloudDialogPrivate::onCloudPasswordValidated(
    CredentialCheckResult result,
    const QString& password)
{
    m_passwordCache[password] = result;
    lockUi(false);
    if (result == CredentialCheckResult::Ok)
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
}

CredentialCheckResult convertCode(ec2::ErrorCode errorCode)
{
    switch(errorCode)
    {
        case ec2::ErrorCode::ok:
            return CredentialCheckResult::Ok;
        case ec2::ErrorCode::userLockedOut:
            return CredentialCheckResult::UserLockedOut;
        default:
            return CredentialCheckResult::NotAuthorized;
    }
    return CredentialCheckResult::NotAuthorized;
}

void QnDisconnectFromCloudDialogPrivate::validateCloudPassword()
{
    NX_ASSERT(scenario == Scenario::CloudOwnerOnly || scenario == Scenario::CloudOwner);
    const QString password = authorizePasswordField->text();

    Q_Q(QnDisconnectFromCloudDialog);
    auto guard = QPointer<QnDisconnectFromCloudDialog>(q);
    auto handler =
        [guard, password]
        (int /*handle*/, ec2::ErrorCode errorCode, const QnConnectionInfo& /*info*/)
        {
            if (!guard)
                return;

            CredentialCheckResult result = convertCode(errorCode);

            emit guard->cloudPasswordValidated(result, password);
        };

    // Current url may be authorized using temporary credentials, so update both username and pass.
    auto url = commonModule()->currentUrl();
    url.setUserName(context()->user()->getName());
    url.setPassword(password);
    lockUi(true);
    qnClientCoreModule->connectionFactory()->testConnection(url, this, handler);
}

void QnDisconnectFromCloudDialogPrivate::setupResetPasswordPage()
{
    NX_ASSERT(scenario == Scenario::CloudOwnerOnly);

    Q_Q(QnDisconnectFromCloudDialog);

    q->setText(tr("Set local owner password"));
    q->setInformativeText(
        tr("You will not be able to connect to this System with your %1 account after you disconnect this System from %1.",
            "%1 is the cloud name (like Nx Cloud)")
            .arg(nx::network::AppInfo::cloudName()));

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

    auto loginField = new InputField();
    loginField->setReadOnly(true);
    loginField->setTitle(tr("Login"));
    loginField->setText(context()->user()->getName());
    layout->addWidget(loginField);

    authorizePasswordField = new InputField();
    authorizePasswordField->setTitle(tr("Password"));
    authorizePasswordField->setEchoMode(QLineEdit::Password);
    authorizePasswordField->setValidator(
        [this](const QString& password)
        {
            auto user = context()->user();
            NX_ASSERT(user);
            if (!user)
                return ValidationResult(tr("Internal Error"));

            switch (scenario)
            {
                case Scenario::LocalOwner:
                {
                    NX_ASSERT(user->isLocal());
                    if (lockoutActivated)
                        return getValidationResult(CredentialCheckResult::UserLockedOut);

                    auto check = m_passwordCache.value(password, CredentialCheckResult::Ok);
                    return getValidationResult(check);
                }
                case Scenario::CloudOwner:
                case Scenario::CloudOwnerOnly:
                {
                    NX_ASSERT(user->isCloud());
                    auto check = m_passwordCache.value(password, CredentialCheckResult::Ok);
                    return getValidationResult(check);
                }
                default:
                    break;
            }
            NX_ASSERT(false, "Should never get here");
            return ValidationResult(tr("Internal Error"));
        });

    authorizeWidget->setFocusPolicy(Qt::TabFocus);
    authorizeWidget->setFocusProxy(authorizePasswordField);

    layout->addWidget(authorizePasswordField);

    Aligner* aligner = new Aligner(authorizeWidget);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    aligner->addWidget(loginField);
    aligner->addWidget(authorizePasswordField);
}

void QnDisconnectFromCloudDialogPrivate::createResetPasswordWidget()
{
    resetPasswordWidget = new QWidget();
    auto* layout = new QVBoxLayout(resetPasswordWidget);
    layout->setSpacing(style::Metrics::kDefaultLayoutSpacing.height());
    layout->setContentsMargins(style::Metrics::kDefaultTopLevelMargins);

    auto owner = resourcePool()->getAdministrator();
    NX_ASSERT(owner);

    auto loginField = new InputField(resetPasswordWidget);
    loginField->setTitle(tr("Login"));
    loginField->setReadOnly(true);
    loginField->setText(owner->getName());
    layout->addWidget(loginField);
    layout->addSpacing(style::Metrics::kDefaultLayoutSpacing.height());

    resetPasswordField = new InputField();
    resetPasswordField->setTitle(tr("Password"));
    resetPasswordField->setEchoMode(QLineEdit::Password);
    resetPasswordField->setPasswordIndicatorEnabled(true);
    resetPasswordField->setValidator(defaultPasswordValidator(false));
    layout->addWidget(resetPasswordField);

    confirmPasswordField = new InputField();
    confirmPasswordField->setTitle(tr("Confirm Password"));
    confirmPasswordField->setEchoMode(QLineEdit::Password);
    confirmPasswordField->setValidator(defaultConfirmationValidator(
        [this]() { return resetPasswordField->text(); },
        tr("Passwords do not match.")));
    layout->addWidget(confirmPasswordField);

    connect(resetPasswordField, &InputField::textChanged, this, [this]()
        {
            if (!confirmPasswordField->text().isEmpty())
                confirmPasswordField->validate();
        });

    connect(resetPasswordField, &InputField::editingFinished,
        confirmPasswordField, &InputField::validate);

    resetPasswordWidget->setFocusPolicy(Qt::TabFocus);
    resetPasswordWidget->setFocusProxy(resetPasswordField);

    Aligner* aligner = new Aligner(resetPasswordWidget);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    aligner->addWidgets({
        loginField,
        resetPasswordField,
        confirmPasswordField });
}

QnDisconnectFromCloudDialogPrivate::Scenario QnDisconnectFromCloudDialogPrivate::calculateScenario() const
{
    auto user = context()->user();

    if (!user || user->userRole() != Qn::UserRole::owner)
        return Scenario::Invalid;

    if (user->isLocal())
        return Scenario::LocalOwner;

    auto localOwners = resourcePool()->getResources().filtered<QnUserResource>(
        [](const QnUserResourcePtr& user)
        {
            return !user->isCloud()
                && user->userRole() == Qn::UserRole::owner;
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

void QnDisconnectFromCloudDialogPrivate::activateLocalLockoutMode(TimePoint end)
{
    auto now = TimePoint::clock::now();

    if (now < end)
    {
        lockoutActivated = true;
        lockoutEndTime = end;
        int duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count();
        QTimer::singleShot(duration, this, &QnDisconnectFromCloudDialogPrivate::at_lockoutExpired);
    }
    else
    {
        lockoutEndTime = now;
        lockoutActivated = false;
        if (authorizePasswordField)
            authorizePasswordField->clear();
    }
}

void QnDisconnectFromCloudDialogPrivate::at_lockoutExpired()
{
    lockoutActivated = false;
    invalidAttempts.clear();
    totalInvalidAttempts = 0;
}

ValidationResult QnDisconnectFromCloudDialogPrivate::getValidationResult(CredentialCheckResult check)
{
    switch (check)
    {
        case CredentialCheckResult::Ok:
            return ValidationResult::kValid;
        case CredentialCheckResult::UserLockedOut:
            return ValidationResult(tr("Too many attempts. Try again in a minute."));
        case CredentialCheckResult::NotAuthorized:
            return ValidationResult(tr("Wrong Password"));
        default:
            NX_ASSERT("Should not be here");
    }
    return ValidationResult(tr("Internal Error"));
}
