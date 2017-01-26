#include "setup_wizard_dialog_p.h"

#include <ui/dialogs/setup_wizard_dialog.h>

#include <QtGui/QDesktopServices>
#include <QtWebKitWidgets/QWebFrame>

#include <ui/widgets/common/web_page.h>

#include <nx/utils/log/log.h>

#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnSetupWizardDialogPrivate::LoginInfo, (json), LoginInfo_Fields);


QnSetupWizardDialogPrivate::QnSetupWizardDialogPrivate(
    QnSetupWizardDialog *parent)
    : QObject(parent)
    , q_ptr(parent)
    , webView(new QWebView(parent))
{
    Q_Q(QnSetupWizardDialog);

    QnWebPage* page = new QnWebPage(webView);
    webView->setPage(page);

    QWebFrame *frame = page->mainFrame();

    connect(frame, &QWebFrame::javaScriptWindowObjectCleared,
        this, [this, frame]()
    {
        frame->addToJavaScriptWindowObject(lit("setupDialog"), this);
    }
    );

    connect(page, &QWebPage::windowCloseRequested, q, &QnSetupWizardDialog::accept);
}

QString QnSetupWizardDialogPrivate::getCredentials() const
{
    return QString::fromUtf8(QJson::serialized(loginInfo));
}

void QnSetupWizardDialogPrivate::updateCredentials(
    const QString& login,
    const QString& password,
    bool isCloud,
    bool savePassword)
{
    loginInfo.localLogin    = isCloud ? QString()   : login;
    loginInfo.localPassword = isCloud ? QString()   : password;
    loginInfo.cloudEmail    = isCloud ? login       : QString();
    loginInfo.cloudPassword = isCloud ? password    : QString();
    loginInfo.savePassword  = savePassword;
}

void QnSetupWizardDialogPrivate::cancel()
{
    Q_Q(QnSetupWizardDialog);

    /* Remove 'accept' connection. */
    disconnect(webView->page(), nullptr, q, nullptr);

    /* Security fix to make sure we will never try to login further. */
    loginInfo = LoginInfo();

    q->reject();
}

void QnSetupWizardDialogPrivate::openUrlInBrowser(const QString &urlString)
{
    QUrl url(urlString);

    NX_LOG(lit("QnSetupWizardDialog: External URL requested: %1")
        .arg(urlString), cl_logINFO);

    if (!url.isValid())
    {
        NX_LOG(lit("QnSetupWizardDialog: External URL is invalid: %1")
            .arg(urlString), cl_logWARNING);
        return;
    }

    QDesktopServices::openUrl(url);
}
