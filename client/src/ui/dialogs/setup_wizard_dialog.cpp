#include "setup_wizard_dialog.h"
#include "private/setup_wizard_dialog_private.h"

#include <QtWebKitWidgets/QWebFrame>
#include <QtWidgets/QBoxLayout>
#include <QtGui/QDesktopServices>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>

#include <ui/widgets/web_page.h>

#include <nx/utils/log/log.h>

QnSetupWizardDialog::QnSetupWizardDialog(const QUrl& serverUrl, QWidget *parent)
    : base_type(parent)
    , d_ptr(new QnSetupWizardDialogPrivate(this))
{
    Q_D(QnSetupWizardDialog);
    d->url = serverUrl;

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(QMargins());
    layout->addWidget(d->webView);

    setFixedSize(480, 374);
}

QnSetupWizardDialog::~QnSetupWizardDialog()
{
}

int QnSetupWizardDialog::exec()
{
    Q_D(QnSetupWizardDialog);

    QUrl url(d->url);
    url.setScheme(d->url.scheme());
    url.setHost(d->url.host());
    url.setPort(d->url.port());
    url.setPath(lit("/static/inline.html"));
    url.setFragment(lit("/setup"));

    NX_LOG(lit("QnSetupWizardDialog: Opening setup URL: %1")
           .arg(url.toString()), cl_logINFO);

    d->webView->load(url);

    return base_type::exec();
}

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

    connect(page, &QWebPage::windowCloseRequested,
            q, &QnSetupWizardDialog::accept);
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
