#include "setup_wizard_dialog_p.h"

#include <ui/dialogs/setup_wizard_dialog.h>

#include <QtGui/QDesktopServices>
#include <QtWebKitWidgets/QWebFrame>
#include <QQuickItem>

#include <ui/graphics/items/standard/graphics_web_view.h>
#include <ui/widgets/common/web_page.h>

#include <nx/utils/log/log.h>

#include <nx/fusion/model_functions.h>

#include <nx/vms/client/desktop/ini.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnSetupWizardDialogPrivate::LoginInfo, (json), LoginInfo_Fields);


QnSetupWizardDialogPrivate::QnSetupWizardDialogPrivate(
    QnSetupWizardDialog *parent)
    : QObject(parent)
    , q_ptr(parent)
    , m_webView(nullptr)
    , m_quickWidget(nullptr)
{
    Q_Q(QnSetupWizardDialog);

    static const QString exportedName("setupDialog");

    if (nx::vms::client::desktop::ini().useWebEngine)
    {
        m_webView = new QWebView(parent);
        QnWebPage* page = new QnWebPage(m_webView);
        m_webView->setPage(page);

        QWebFrame *frame = page->mainFrame();

        connect(frame, &QWebFrame::javaScriptWindowObjectCleared,
            this, [this, frame]()
            {
                frame->addToJavaScriptWindowObject(exportedName, this);
            });

        connect(page, &QWebPage::windowCloseRequested, q, &QnSetupWizardDialog::accept);
    }
    else
    {
        m_quickWidget = new QQuickWidget(QUrl(), parent);
        connect(m_quickWidget, &QQuickWidget::statusChanged,
            this, [this, q](QQuickWidget::Status status){
                if (status != QQuickWidget::Ready)
                    return;

                QQuickItem* webView = m_quickWidget->rootObject();
                if (!webView)
                    return;

                connect(webView, SIGNAL(windowCloseRequested()), q, SLOT(accept()));

                nx::vms::client::desktop::GraphicsWebEngineView::registerObject(webView, exportedName, this);
            });
        m_quickWidget->setSource(nx::vms::client::desktop::GraphicsWebEngineView::kQmlSourceUrl);
    }
}

QWidget* QnSetupWizardDialogPrivate::webWidget()
{
    if (m_webView)
        return m_webView;

    return m_quickWidget;
}

void QnSetupWizardDialogPrivate::load(const QUrl& url)
{
    if (m_webView)
    {
        m_webView->load(url);
        return;
    }

    auto setUrlFunc = [this, url](QQuickWidget::Status status){
        if (status != QQuickWidget::Ready)
            return;
        QQuickItem* webView = m_quickWidget->rootObject();
        if (!webView)
            return;
        webView->setProperty("url", url);
    };

    if (m_quickWidget->status() != QQuickWidget::Ready)
    {
        connect(m_quickWidget, &QQuickWidget::statusChanged, this, setUrlFunc);
        return;
    }

    setUrlFunc(m_quickWidget->status());
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
    if (m_webView)
        disconnect(m_webView->page(), nullptr, q, nullptr);
    else if (QQuickItem* webView = m_quickWidget->rootObject())
        disconnect(webView, nullptr, q, nullptr);

    /* Security fix to make sure we will never try to login further. */
    loginInfo = LoginInfo();

    q->reject();
}

void QnSetupWizardDialogPrivate::openUrlInBrowser(const QString &urlString)
{
    QUrl url(urlString);

    NX_INFO(this, lit("External URL requested: %1")
        .arg(urlString));

    if (!url.isValid())
    {
        NX_WARNING(this, lit("External URL is invalid: %1")
            .arg(urlString));
        return;
    }

    QDesktopServices::openUrl(url);
}
