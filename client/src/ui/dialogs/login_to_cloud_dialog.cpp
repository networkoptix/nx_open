#include "login_to_cloud_dialog.h"
#include "ui_login_to_cloud_dialog.h"

#include <helpers/cloud_url_helper.h>
#include <watchers/cloud_status_watcher.h>
#include <common/common_module.h>
#include <utils/common/string.h>
#include <client/client_settings.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

class QnLoginToCloudDialogPrivate : public QObject
{
    QnLoginToCloudDialog *q_ptr;
    Q_DECLARE_PUBLIC(QnLoginToCloudDialog)
    Q_DECLARE_TR_FUNCTIONS(QnLinkToCloudDialogPrivate)

public:
    QnLoginToCloudDialogPrivate(QnLoginToCloudDialog *parent);

    void updateUi();
    void lockUi(bool locked = true);
    void unlockUi();
    void at_loginButton_clicked();
    void at_cloudStatusWatcher_statusChanged(QnCloudStatusWatcher::Status status);
    void at_cloudStatusWatcher_error(QnCloudStatusWatcher::ErrorCode errorCode);
};

QnLoginToCloudDialog::QnLoginToCloudDialog(QWidget *parent)
    : base_type(parent)
    , ui(new Ui::QnLoginToCloudDialog)
    , d_ptr(new QnLoginToCloudDialogPrivate(this))
{
    ui->setupUi(this);

    Q_D(QnLoginToCloudDialog);

    connect(ui->loginButton,        &QPushButton::clicked,      d,  &QnLoginToCloudDialogPrivate::at_loginButton_clicked);
    connect(ui->loginLineEdit,      &QLineEdit::textChanged,    d,  &QnLoginToCloudDialogPrivate::updateUi);
    connect(ui->passwordLineEdit,   &QLineEdit::textChanged,    d,  &QnLoginToCloudDialogPrivate::updateUi);

    ui->createAccountLabel->setText(makeHref(tr("Create account"), QnCloudUrlHelper::createAccountUrl()));
    ui->restorePasswordLabel->setText(makeHref(tr("Restore password"), QnCloudUrlHelper::createAccountUrl()));

    d->updateUi();
}

QnLoginToCloudDialog::~QnLoginToCloudDialog()
{
}

void QnLoginToCloudDialog::setLogin(const QString &login)
{
    ui->loginLineEdit->setText(login);
    ui->loginLineEdit->selectAll();
}

QnLoginToCloudDialogPrivate::QnLoginToCloudDialogPrivate(QnLoginToCloudDialog *parent)
    : QObject(parent)
    , q_ptr(parent)
{
}

void QnLoginToCloudDialogPrivate::updateUi()
{
    Q_Q(QnLoginToCloudDialog);
    q->ui->loginButton->setEnabled(
                !q->ui->loginLineEdit->text().isEmpty() &&
                !q->ui->passwordLineEdit->text().isEmpty());
}

void QnLoginToCloudDialogPrivate::lockUi(bool locked)
{
    Q_Q(QnLoginToCloudDialog);

    q->ui->loginLineEdit->setReadOnly(locked);
    q->ui->passwordLineEdit->setReadOnly(locked);
    q->ui->stayLoggedInChackBox->setEnabled(!locked);
    q->ui->loginButton->setEnabled(!locked);
}

void QnLoginToCloudDialogPrivate::unlockUi()
{
    lockUi(false);
}

void QnLoginToCloudDialogPrivate::at_loginButton_clicked()
{
    lockUi();

    Q_Q(QnLoginToCloudDialog);

    QnCloudStatusWatcher *cloudStatusWatcher = qnCommon->instance<QnCloudStatusWatcher>();
    cloudStatusWatcher->setCloudCredentials(QString(), QString());

    connect(cloudStatusWatcher,     &QnCloudStatusWatcher::statusChanged,       this,   &QnLoginToCloudDialogPrivate::at_cloudStatusWatcher_statusChanged);
    connect(cloudStatusWatcher,     &QnCloudStatusWatcher::error,               this,   &QnLoginToCloudDialogPrivate::at_cloudStatusWatcher_error);
    cloudStatusWatcher->setCloudCredentials(q->ui->loginLineEdit->text(), q->ui->passwordLineEdit->text());
}

void QnLoginToCloudDialogPrivate::at_cloudStatusWatcher_statusChanged(QnCloudStatusWatcher::Status status)
{
    if (status != QnCloudStatusWatcher::Online)
    {
        // waiting for error signal in this case
        return;
    }

    QnCloudStatusWatcher *cloudStatusWatcher = qnCommon->instance<QnCloudStatusWatcher>();
    disconnect(cloudStatusWatcher, nullptr, this, nullptr);

    Q_Q(QnLoginToCloudDialog);

    if (q->ui->stayLoggedInChackBox->isChecked())
    {
        qnSettings->setCloudLogin(q->ui->loginLineEdit->text());
        qnSettings->setCloudPassword(q->ui->passwordLineEdit->text());
    }

    q->accept();
}

void QnLoginToCloudDialogPrivate::at_cloudStatusWatcher_error(QnCloudStatusWatcher::ErrorCode errorCode)
{
    QnCloudStatusWatcher *cloudStatusWatcher = qnCommon->instance<QnCloudStatusWatcher>();
    disconnect(cloudStatusWatcher, nullptr, this, nullptr);

    QString message;

    switch (errorCode)
    {
    case QnCloudStatusWatcher::InvalidCredentials:
        message = tr("Login or password is invalid");
        break;
    case QnCloudStatusWatcher::UnknownError:
        message = tr("Can not login to cloud");
        break;
    }

    Q_Q(QnLoginToCloudDialog);

    QnMessageBox messageBox(QnMessageBox::NoIcon,
                            helpTopic(q),
                            tr("Error"),
                            message,
                            QDialogButtonBox::Ok,
                            q);

    unlockUi();
}
