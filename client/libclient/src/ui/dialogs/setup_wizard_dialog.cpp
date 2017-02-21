#include "setup_wizard_dialog.h"

#include <QtWidgets/QBoxLayout>

#include <common/common_module.h>

#include <client/client_translation_manager.h>

#include <ui/dialogs/private/setup_wizard_dialog_p.h>

#include <nx/utils/log/log.h>

namespace
{
    static const QSize kSetupWizardSize(496, 392);

    QUrl constructUrl(const QUrl &baseUrl)
    {
        QUrl url(baseUrl);
        url.setScheme(baseUrl.scheme());
        url.setHost(baseUrl.host());
        url.setPort(baseUrl.port());
        url.setPath(lit("/static/inline.html"));
        url.setFragment(lit("/setup"));

        QUrlQuery q;
        q.addQueryItem(lit("lang"), qnCommon->instance<QnClientTranslationManager>()->getCurrentLanguage());
        url.setQuery(q);
        return url;
    }

} // namespace

QnSetupWizardDialog::QnSetupWizardDialog(QWidget *parent)
    : base_type(parent, Qt::MSWindowsFixedSizeDialogHint)
    , d_ptr(new QnSetupWizardDialogPrivate(this))
{
    Q_D(QnSetupWizardDialog);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(QMargins());
#ifdef _DEBUG
    QLineEdit* urlLineEdit = new QLineEdit(this);
    layout->addWidget(urlLineEdit);
    connect(urlLineEdit, &QLineEdit::returnPressed, this,
        [this, urlLineEdit]()
        {
            Q_D(QnSetupWizardDialog);
            d->webView->load(urlLineEdit->text());
        });
#endif
    layout->addWidget(d->webView);
    setFixedSize(kSetupWizardSize);
}

QnSetupWizardDialog::~QnSetupWizardDialog()
{
}

int QnSetupWizardDialog::exec()
{
    Q_D(QnSetupWizardDialog);

    QUrl url = constructUrl(d->url);

#ifdef _DEBUG
    if (auto lineEdit = findChild<QLineEdit*>())
        lineEdit->setText(url.toString());
#endif

    NX_LOG(lit("QnSetupWizardDialog: Opening setup URL: %1")
           .arg(url.toString(QUrl::RemovePassword)), cl_logDEBUG1);

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

QnCredentials QnSetupWizardDialog::localCredentials() const
{
    Q_D(const QnSetupWizardDialog);
    return QnCredentials(d->loginInfo.localLogin, d->loginInfo.localPassword);
}

QnCredentials QnSetupWizardDialog::cloudCredentials() const
{
    Q_D(const QnSetupWizardDialog);
    return QnCredentials(d->loginInfo.cloudEmail, d->loginInfo.cloudPassword);
}

void QnSetupWizardDialog::setCloudCredentials(const QnCredentials& value)
{
    Q_D(QnSetupWizardDialog);
    d->loginInfo.cloudEmail = value.user;
    d->loginInfo.cloudPassword = value.password.value();
}

bool QnSetupWizardDialog::savePassword() const
{
    Q_D(const QnSetupWizardDialog);
    return d->loginInfo.savePassword;
}
