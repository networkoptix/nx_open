#include "setup_wizard_dialog.h"

#include <ui/dialogs/private/setup_wizard_dialog_p.h>

#include <QtWidgets/QBoxLayout>

#include <nx/utils/log/log.h>

namespace
{
    QUrl constructUrl(const QUrl &baseUrl)
    {
        QUrl url(baseUrl);
        url.setScheme(baseUrl.scheme());
        url.setHost(baseUrl.host());
        url.setPort(baseUrl.port());
        url.setPath(lit("/static/inline.html"));
        url.setFragment(lit("/setup"));
        return url;
    }

}

QnSetupWizardDialog::QnSetupWizardDialog(QWidget *parent)
    : base_type(parent)
    , d_ptr(new QnSetupWizardDialogPrivate(this))
{
    Q_D(QnSetupWizardDialog);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(QMargins());
#ifdef _DEBUG
    QLineEdit* urlLineEdit = new QLineEdit(this);
    layout->addWidget(urlLineEdit);
    //urlLineEdit->setText(constructUrl(serverUrl).toString());
    connect(urlLineEdit, &QLineEdit::returnPressed, this, [this, urlLineEdit]()
    {
        Q_D(QnSetupWizardDialog);
        d->webView->load(urlLineEdit->text());
    });
#endif
    layout->addWidget(d->webView);
    setFixedSize(480, 374);
}

QnSetupWizardDialog::~QnSetupWizardDialog()
{
}

int QnSetupWizardDialog::exec()
{
    Q_D(QnSetupWizardDialog);

    QUrl url = constructUrl(d->url);

    NX_LOG(lit("QnSetupWizardDialog: Opening setup URL: %1")
           .arg(url.toString()), cl_logINFO);

    d->webView->load(url);

    return base_type::exec();
}

QUrl QnSetupWizardDialog::url() const
{
    Q_D(const QnSetupWizardDialog);
    return d->url;
}

void QnSetupWizardDialog::setUrl(const QUrl& url)
{
    Q_D(QnSetupWizardDialog);
    d->url = url;
}

QString QnSetupWizardDialog::localLogin() const
{
    Q_D(const QnSetupWizardDialog);
    return d->loginInfo.localLogin;

}

QString QnSetupWizardDialog::localPassword() const
{
    Q_D(const QnSetupWizardDialog);
    return d->loginInfo.localPassword;
}

QString QnSetupWizardDialog::cloudLogin() const
{
    Q_D(const QnSetupWizardDialog);
    return d->loginInfo.cloudEmail;
}

void QnSetupWizardDialog::setCloudLogin(const QString& login)
{
    Q_D(QnSetupWizardDialog);
    d->loginInfo.cloudEmail = login;
}

QString QnSetupWizardDialog::cloudPassword() const
{
    Q_D(const QnSetupWizardDialog);
    return d->loginInfo.cloudPassword;
}

void QnSetupWizardDialog::setCloudPassword(const QString& password)
{
    Q_D(QnSetupWizardDialog);
    d->loginInfo.cloudPassword = password;
}
