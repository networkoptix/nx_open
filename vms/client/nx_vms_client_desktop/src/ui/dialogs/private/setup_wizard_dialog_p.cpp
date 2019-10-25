#include "setup_wizard_dialog_p.h"

#include <ui/dialogs/setup_wizard_dialog.h>

#include <QtGui/QDesktopServices>
#include <QtQuick/QQuickItem>

#include <ui/graphics/items/standard/graphics_web_view.h>

#include <nx/utils/log/log.h>

#include <nx/fusion/model_functions.h>

#include <client_core/client_core_module.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnSetupWizardDialogPrivate::LoginInfo, (json), LoginInfo_Fields);

using namespace nx::vms::client::desktop;

QnSetupWizardDialogPrivate::QnSetupWizardDialogPrivate(
    QnSetupWizardDialog *parent)
    :
    QObject(parent),
    q_ptr(parent),
    m_quickWidget(new QQuickWidget(qnClientCoreModule->mainQmlEngine(), parent))
{
    Q_Q(QnSetupWizardDialog);

    static const QString exportedName("setupDialog");

    connect(m_quickWidget, &QQuickWidget::statusChanged,
        this, [this, q](QQuickWidget::Status status){
            if (status != QQuickWidget::Ready)
                return;

            QQuickItem* webView = m_quickWidget->rootObject();
            if (!webView)
                return;

            connect(webView, SIGNAL(windowCloseRequested()), q, SLOT(accept()));

            // Workaround for webadmin JS code which expects synchronous slot calls.
            static constexpr bool enablePromises = true;
            GraphicsWebEngineView::registerObject(webView, exportedName, this, enablePromises);
        });

    m_quickWidget->setClearColor(parent->palette().window().color());
    m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    m_quickWidget->setSource(GraphicsWebEngineView::kQmlSourceUrl);
}

void QnSetupWizardDialogPrivate::load(const QUrl& url)
{
    const auto setUrlFunc =
        [this, url](QQuickWidget::Status status)
        {
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
    if (QQuickItem* webView = m_quickWidget->rootObject())
        webView->disconnect(q);

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
