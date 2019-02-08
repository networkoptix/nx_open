#include "login_to_cloud_dialog.h"
#include "ui_login_to_cloud_dialog.h"

#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtWidgets/QLineEdit>

#include <nx/network/app_info.h>

#include <client_core/client_core_settings.h>

#include <helpers/cloud_url_helper.h>

#include <nx/vms/client/core/settings/client_core_settings.h>

#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/cloud/cloud_result_messages.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <nx/vms/client/desktop/common/widgets/input_field.h>

#include <watchers/cloud_status_watcher.h>

#include <utils/common/app_info.h>
#include <utils/common/html.h>

using namespace nx::vms::client::desktop;

namespace
{
    const int kWelcomeFontPixelSize = 24;
    const int kWelcomeFontWeight = QFont::Light;
}

class QnLoginToCloudDialogPrivate: public QObject
{
    QnLoginToCloudDialog* q_ptr;
    Q_DECLARE_PUBLIC(QnLoginToCloudDialog)
    Q_DECLARE_TR_FUNCTIONS(QnLinkToCloudDialogPrivate)

public:
    QnLoginToCloudDialogPrivate(QnLoginToCloudDialog* parent);

    void updateUi();
    void resetErrorState();
    void lockUi(bool locked = true);
    void unlockUi();
    void at_loginButton_clicked();
    void at_cloudStatusWatcher_statusChanged(QnCloudStatusWatcher::Status status);
    void at_cloudStatusWatcher_error();
};

QnLoginToCloudDialog::QnLoginToCloudDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::QnLoginToCloudDialog),
    d_ptr(new QnLoginToCloudDialogPrivate(this))
{
    ui->setupUi(this);

    Q_D(QnLoginToCloudDialog);

    setWindowTitle(tr("Log in to %1", "%1 is the cloud name (like Nx Cloud)")
        .arg(nx::network::AppInfo::cloudName()));

    ui->loginInputField->setTitle(tr("Email"));
    ui->loginInputField->setValidator(defaultEmailValidator(false));

    ui->passwordInputField->setTitle(tr("Password"));
    ui->passwordInputField->setEchoMode(QLineEdit::Password);
    ui->passwordInputField->setValidator(defaultPasswordValidator(false));

    connect(ui->loginButton,        &QPushButton::clicked,      d, &QnLoginToCloudDialogPrivate::at_loginButton_clicked);
    connect(ui->loginInputField,    &InputField::textChanged, d, &QnLoginToCloudDialogPrivate::updateUi);
    connect(ui->passwordInputField, &InputField::textChanged, d, &QnLoginToCloudDialogPrivate::updateUi);

    using nx::vms::utils::SystemUri;
    QnCloudUrlHelper urlHelper(
        SystemUri::ReferralSource::DesktopClient,
        SystemUri::ReferralContext::SettingsDialog);

    ui->createAccountLabel->setText(makeHref(tr("Create account"), urlHelper.createAccountUrl()));
    ui->restorePasswordLabel->setText(makeHref(tr("Forgot password?"), urlHelper.restorePasswordUrl()));
    ui->learnMoreLabel->setText(makeHref(tr("Learn more about"), urlHelper.aboutUrl()));

    ui->cloudWelcomeLabel->setText(tr("Welcome to %1!", "%1 is the cloud name (like Nx Cloud)")
        .arg(nx::network::AppInfo::cloudName()));
    ui->cloudImageLabel->setPixmap(qnSkin->pixmap("cloud/cloud_64.png"));

    QFont welcomeFont(ui->cloudWelcomeLabel->font());
    welcomeFont.setPixelSize(kWelcomeFontPixelSize);
    welcomeFont.setWeight(kWelcomeFontWeight);
    ui->cloudWelcomeLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->cloudWelcomeLabel->setFont(welcomeFont);

    const QColor nxColor(qApp->palette().color(QPalette::Normal, QPalette::BrightText));
    setPaletteColor(ui->cloudWelcomeLabel, QPalette::WindowText, nxColor);

    auto aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    aligner->addWidgets({ ui->loginInputField, ui->passwordInputField, ui->spacer });

    auto opacityEffect = new QGraphicsOpacityEffect(this);
    opacityEffect->setOpacity(style::Hints::kDisabledItemOpacity);
    ui->logoPanel->setGraphicsEffect(opacityEffect);
    opacityEffect = new QGraphicsOpacityEffect(this);
    opacityEffect->setOpacity(style::Hints::kDisabledItemOpacity);
    ui->linksWidget->setGraphicsEffect(opacityEffect);
    ui->hintLabel->setVisible(false);

    d->updateUi();
    d->lockUi(false);

    setAccentStyle(ui->loginButton);

    setResizeToContentsMode(Qt::Vertical | Qt::Horizontal);
}

QnLoginToCloudDialog::~QnLoginToCloudDialog()
{
}

void QnLoginToCloudDialog::setLogin(const QString& login)
{
    ui->loginInputField->setText(login);
}

void QnLoginToCloudDialog::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    if (ui->loginInputField->text().isEmpty())
        ui->loginInputField->setFocus();
    else
        ui->passwordInputField->setFocus();
}

QnLoginToCloudDialogPrivate::QnLoginToCloudDialogPrivate(QnLoginToCloudDialog* parent) :
    QObject(parent),
    q_ptr(parent)
{
}

void QnLoginToCloudDialogPrivate::updateUi()
{
    Q_Q(QnLoginToCloudDialog);

    resetErrorState();

    q->ui->loginButton->setEnabled(
        q->ui->loginInputField->isValid() &&
                q->ui->passwordInputField->isValid());
}

void QnLoginToCloudDialogPrivate::resetErrorState()
{
    Q_Q(QnLoginToCloudDialog);

    q->ui->loginInputField->setCustomHint(QString());
    q->ui->passwordInputField->setCustomHint(QString());
    resetStyle(q->ui->loginInputField);
    resetStyle(q->ui->passwordInputField);
}

void QnLoginToCloudDialogPrivate::lockUi(bool locked)
{
    Q_Q(QnLoginToCloudDialog);

    const bool enabled = !locked;

    q->ui->logoPanel->setEnabled(enabled);
    q->ui->containerWidget->setEnabled(enabled);
    q->ui->loginButton->setEnabled(enabled && q->ui->passwordInputField->isValid());

    q->ui->logoPanel->graphicsEffect()->setEnabled(locked);
    q->ui->linksWidget->graphicsEffect()->setEnabled(locked);

    q->ui->loginButton->showIndicator(locked);
}

void QnLoginToCloudDialogPrivate::unlockUi()
{
    lockUi(false);
}

void QnLoginToCloudDialogPrivate::at_loginButton_clicked()
{
    lockUi();
    resetErrorState();

    Q_Q(QnLoginToCloudDialog);

    qnCloudStatusWatcher->resetCredentials();

    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged,
        this, &QnLoginToCloudDialogPrivate::at_cloudStatusWatcher_statusChanged);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::errorChanged,
        this, &QnLoginToCloudDialogPrivate::at_cloudStatusWatcher_error);

    qnCloudStatusWatcher->setCredentials({
        q->ui->loginInputField->text().trimmed(),
        q->ui->passwordInputField->text().trimmed()});
}

void QnLoginToCloudDialogPrivate::at_cloudStatusWatcher_statusChanged(QnCloudStatusWatcher::Status status)
{
    if (status == QnCloudStatusWatcher::LoggedOut)
    {
        // waiting for error signal in this case
        return;
    }

    qnCloudStatusWatcher->disconnect(this);

    Q_Q(QnLoginToCloudDialog);

    // TODO: #GDM Store temporary credentials?
    const bool stayLoggedIn = q->ui->stayLoggedInCheckBox->isChecked();
    nx::vms::client::core::settings()->cloudCredentials = {
        q->ui->loginInputField->text().trimmed(),
        stayLoggedIn
            ? q->ui->passwordInputField->text().trimmed()
            : QString()};

    q->accept();
}

void QnLoginToCloudDialogPrivate::at_cloudStatusWatcher_error()
{
    Q_Q(QnLoginToCloudDialog);

    const auto errorCode = qnCloudStatusWatcher->error();
    qnCloudStatusWatcher->disconnect(this);

    QWidget* focusWidget = nullptr;

    bool showHint = false;

    switch (errorCode)
    {
        case QnCloudStatusWatcher::NoError:
            break;

        case QnCloudStatusWatcher::InvalidEmail:
            q->ui->passwordInputField->clear();
            q->ui->passwordInputField->reset();
            q->ui->loginInputField->setCustomHint(QnCloudResultMessages::accountNotFound());
            setWarningStyle(q->ui->loginInputField);
            focusWidget = q->ui->loginInputField;
            break;

        case QnCloudStatusWatcher::AccountNotActivated:
            q->ui->passwordInputField->clear();
            q->ui->passwordInputField->reset();
            q->ui->loginInputField->setCustomHint(QnCloudResultMessages::accountNotActivated());
            setWarningStyle(q->ui->loginInputField);
            focusWidget = q->ui->loginInputField;
            break;

        case QnCloudStatusWatcher::InvalidPassword:
            q->ui->passwordInputField->clear();
            q->ui->passwordInputField->setCustomHint(QnCloudResultMessages::invalidPassword());
            setWarningStyle(q->ui->passwordInputField);
            focusWidget = q->ui->passwordInputField;
            break;

        case QnCloudStatusWatcher::UserTemporaryLockedOut:
            q->ui->passwordInputField->clear();
            q->ui->passwordInputField->reset();
            showHint = true;
            q->ui->hintLabel->setText(QnCloudResultMessages::userLockedOut());
            setWarningStyle(q->ui->hintLabel);
            focusWidget = q->ui->loginButton;
            break;
        case QnCloudStatusWatcher::UnknownError:
        default:
        {
            QnMessageBox::critical(q, tr("Failed to login to %1",
                "%1 is the cloud name (like Nx Cloud)").arg(nx::network::AppInfo::cloudName()));
            break;
        }
    }

    unlockUi();

    q->ui->hintLabel->setVisible(showHint);

    if (focusWidget)
        focusWidget->setFocus();
}
