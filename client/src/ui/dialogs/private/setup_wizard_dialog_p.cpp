#include "setup_wizard_dialog_p.h"

#include <ui/dialogs/setup_wizard_dialog.h>

#include <QtGui/QDesktopServices>
#include <QtWebKitWidgets/QWebFrame>

#include <ui/widgets/web_page.h>

#include <nx/utils/log/log.h>

#include <utils/common/model_functions.h>

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
