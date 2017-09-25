#include "login_to_cloud_dialog.h"
#include "ui_login_to_cloud_dialog.h"

#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtWidgets/QLineEdit>

#include <nx/network/app_info.h>

#include <client_core/client_core_settings.h>

#include <helpers/cloud_url_helper.h>

#include <ui/common/aligner.h>
#include <ui/common/palette.h>
#include <ui/dialogs/cloud/cloud_result_messages.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/input_field.h>

#include <watchers/cloud_status_watcher.h>

#include <utils/common/app_info.h>
#include <utils/common/html.h>

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
    void lockUi(bool locked = true);
    void unlockUi();
    void at_loginButton_clicked();
    void at_cloudStatusWatcher_statusChanged(QnCloudStatusWatcher::Status status);
    void at_cloudStatusWatcher_error();

    void showCredentialsError(const QString& text);
};

QnLoginToCloudDialog::QnLoginToCloudDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::QnLoginToCloudDialog),
    d_ptr(new QnLoginToCloudDialogPrivate(this))
{
    ui->setupUi(this);

    Q_D(QnLoginToCloudDialog);

    setWindowTitle(tr("Log in to %1", "%1 is the cloud name (like 'Nx Cloud')")
        .arg(nx::network::AppInfo::cloudName()));

    ui->loginInputField->setTitle(tr("Email"));
    ui->loginInputField->setValidator(Qn::defaultEmailValidator(false));

    ui->passwordInputField->setTitle(tr("Password"));
    ui->passwordInputField->setEchoMode(QLineEdit::Password);
    ui->passwordInputField->setValidator(Qn::defaultPasswordValidator(false));

    setWarningStyle(ui->invalidCredentialsLabel);

    connect(ui->loginButton,        &QPushButton::clicked,      d, &QnLoginToCloudDialogPrivate::at_loginButton_clicked);
    connect(ui->loginInputField,    &QnInputField::textChanged, d, &QnLoginToCloudDialogPrivate::updateUi);
    connect(ui->passwordInputField, &QnInputField::textChanged, d, &QnLoginToCloudDialogPrivate::updateUi);

    using nx::vms::utils::SystemUri;
    QnCloudUrlHelper urlHelper(
        SystemUri::ReferralSource::DesktopClient,
        SystemUri::ReferralContext::SettingsDialog);

    ui->createAccountLabel->setText(makeHref(tr("Create account"), urlHelper.createAccountUrl()));
    ui->restorePasswordLabel->setText(makeHref(tr("Forgot password?"), urlHelper.restorePasswordUrl()));
    ui->learnMoreLabel->setText(makeHref(tr("Learn more about"), urlHelper.aboutUrl()));

    ui->cloudWelcomeLabel->setText(tr("Welcome to %1!", "%1 is the cloud name (like 'Nx Cloud')")
        .arg(nx::network::AppInfo::cloudName()));
    ui->cloudImageLabel->setPixmap(qnSkin->pixmap("cloud/cloud_64.png"));

    QFont welcomeFont(ui->cloudWelcomeLabel->font());
    welcomeFont.setPixelSize(kWelcomeFontPixelSize);
    welcomeFont.setWeight(kWelcomeFontWeight);
    ui->cloudWelcomeLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->cloudWelcomeLabel->setFont(welcomeFont);

    const QColor nxColor(qApp->palette().color(QPalette::Normal, QPalette::BrightText));
    setPaletteColor(ui->cloudWelcomeLabel, QPalette::WindowText, nxColor);

    auto aligner = new QnAligner(this);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
    aligner->addWidgets({ ui->loginInputField, ui->passwordInputField, ui->spacer });

    auto opacityEffect = new QGraphicsOpacityEffect(this);
    opacityEffect->setOpacity(style::Hints::kDisabledItemOpacity);
    ui->logoPanel->setGraphicsEffect(opacityEffect);
    opacityEffect = new QGraphicsOpacityEffect(this);
    opacityEffect->setOpacity(style::Hints::kDisabledItemOpacity);
    ui->linksWidget->setGraphicsEffect(opacityEffect);

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

void QnLoginToCloudDialogPrivate::showCredentialsError(const QString& text)
{
    Q_Q(QnLoginToCloudDialog);
    q->ui->invalidCredentialsLabel->setHidden(text.isEmpty());
    q->ui->invalidCredentialsLabel->setText(text);
    q->ui->containerWidget->layout()->activate();
}

QnLoginToCloudDialogPrivate::QnLoginToCloudDialogPrivate(QnLoginToCloudDialog* parent) :
    QObject(parent),
    q_ptr(parent)
{
}

void QnLoginToCloudDialogPrivate::updateUi()
{
    Q_Q(QnLoginToCloudDialog);
    q->ui->loginButton->setEnabled(
        q->ui->loginInputField->isValid() &&
        q->ui->passwordInputField->isValid());

    showCredentialsError(QString());
}

void QnLoginToCloudDialogPrivate::lockUi(bool locked)
{
    Q_Q(QnLoginToCloudDialog);

    const bool enabled = !locked;

    q->ui->logoPanel->setEnabled(enabled);
    q->ui->containerWidget->setEnabled(enabled);
    q->ui->loginButton->setEnabled(enabled && q->ui->invalidCredentialsLabel->isHidden());

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

    Q_Q(QnLoginToCloudDialog);

    showCredentialsError(QString());

    qnCloudStatusWatcher->resetCredentials();

    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged,
        this, &QnLoginToCloudDialogPrivate::at_cloudStatusWatcher_statusChanged);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::errorChanged,
        this, &QnLoginToCloudDialogPrivate::at_cloudStatusWatcher_error);

    qnCloudStatusWatcher->setCredentials(QnEncodedCredentials(
        q->ui->loginInputField->text().trimmed(),
        q->ui->passwordInputField->text().trimmed()));
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
    qnClientCoreSettings->setCloudLogin(q->ui->loginInputField->text().trimmed());
    const bool stayLoggedIn = q->ui->stayLoggedInCheckBox->isChecked();
    qnClientCoreSettings->setCloudPassword(stayLoggedIn
        ? q->ui->passwordInputField->text().trimmed()
        : QString());
    qnClientCoreSettings->save();

    q->accept();
}

void QnLoginToCloudDialogPrivate::at_cloudStatusWatcher_error()
{
    const auto errorCode = qnCloudStatusWatcher->error();
    qnCloudStatusWatcher->disconnect(this);

    switch (errorCode)
    {
        case QnCloudStatusWatcher::NoError:
            break;

        case QnCloudStatusWatcher::InvalidCredentials:
            showCredentialsError(QnCloudResultMessages::invalidCredentials());
            break;

        case QnCloudStatusWatcher::AccountNotActivated:
            showCredentialsError(QnCloudResultMessages::accountNotActivated());
            break;

        case QnCloudStatusWatcher::UnknownError:
        default:
        {
            Q_Q(QnLoginToCloudDialog);

            QnMessageBox::critical(q, tr("Failed to login to %1",
                "%1 is the cloud name (like 'Nx Cloud')").arg(nx::network::AppInfo::cloudName()));
            break;
        }
    }
    unlockUi();
}
