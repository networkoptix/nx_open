#include "setup_wizard_dialog.h"

#include <QtCore/QUrlQuery>

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLineEdit>

#include <client/client_runtime_settings.h>

#include <ui/dialogs/private/setup_wizard_dialog_p.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <nx/utils/log/log.h>

namespace
{
    static const QSize kSetupWizardSize(496, 392);

    QUrl constructUrl(const QUrl &baseUrl, QnCommonModule* commonModule)
    {
        QUrl url(baseUrl);
        url.setScheme(baseUrl.scheme());
        url.setHost(baseUrl.host());
        url.setPort(baseUrl.port());
        url.setPath(lit("/static/inline.html"));
        url.setFragment(lit("/setup"));

        QUrlQuery q;
        q.addQueryItem(lit("lang"), qnRuntime->locale());
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

    setHelpTopic(this, Qn::Setup_Wizard_Help);
}

QnSetupWizardDialog::~QnSetupWizardDialog()
{
}

int QnSetupWizardDialog::exec()
{
    Q_D(QnSetupWizardDialog);

    QUrl url = constructUrl(d->url, commonModule());

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

QnEncodedCredentials QnSetupWizardDialog::localCredentials() const
{
    Q_D(const QnSetupWizardDialog);
    return QnEncodedCredentials(d->loginInfo.localLogin, d->loginInfo.localPassword);
}

QnEncodedCredentials QnSetupWizardDialog::cloudCredentials() const
{
    Q_D(const QnSetupWizardDialog);
    return QnEncodedCredentials(d->loginInfo.cloudEmail, d->loginInfo.cloudPassword);
}

void QnSetupWizardDialog::setCloudCredentials(const QnEncodedCredentials& value)
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
