#include "web_engine_view.h"

#include <memory>

#include <QtCore/QTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QPointer>
#include <QtGui/QDesktopServices>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QWindow>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtNetwork/QAuthenticator>
#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWebEngineWidgets/QWebEngineContextMenuData>
#include <QtWebEngineWidgets/QWebEngineScript>
#include <QtWebEngineWidgets/QWebEngineScriptCollection>

using namespace nx::vms::client::desktop;

struct WebEngineView::Private
{
    bool m_useActionsForLinks = false;
    bool m_ignoreSslErrors = true;
    bool m_redirectLinksToDesktop = false;
    QWebEnginePage* m_masterPage = nullptr;
    std::shared_ptr<QWebEngineProfile> m_profile;
    std::vector<QWebEnginePage::WebAction> m_hiddenActions;
};

namespace {

class WebEnginePage: public QWebEnginePage
{
    using base_type = QWebEnginePage;

public:
    WebEnginePage(QWebEngineProfile* profile, QObject* parent, const WebEngineView& view):
        base_type(profile, parent),
        m_view(view)
    {
        init();
    }

    WebEnginePage(QObject* parent, const WebEngineView& view):
        base_type(parent),
        m_view(view)
    {
        init();
    }

protected:
    virtual bool certificateError(const QWebEngineCertificateError& certificateError) override
    {
        if (m_view.hasIgnoreSslErrors())
        {
            // Ignore the error and complete the request.
            return true;
        }
        return base_type::certificateError(certificateError);
    }

    virtual bool acceptNavigationRequest(
        const QUrl& url,
        NavigationType type,
        bool isMainFrame) override
    {
        if (m_view.isRedirectLinksToDesktop() && type == NavigationTypeLinkClicked)
        {
            QDesktopServices::openUrl(url);
            return false;
        }
        return base_type::acceptNavigationRequest(url, type, isMainFrame);
    }

private:
    void init()
    {
        action(QWebEnginePage::ViewSource)->setVisible(false);
        action(QWebEnginePage::SavePage)->setVisible(false);
    }

    const WebEngineView& m_view;
};

} // namespace

WebEngineView::WebEngineView(QWidget* parent, WebEngineView* deriveFrom):
    base_type(parent),
    d(new Private())
{
    if (deriveFrom)
    {
        // Copy settings.
        *d = *deriveFrom->d;
        addActions(deriveFrom->actions());
    }

    // Make the page use derived profile.
    if (d->m_masterPage)
        setPage(new WebEnginePage(d->m_masterPage->profile(), this, *this));
    else
        setPage(new WebEnginePage(this, *this));

    if (deriveFrom)
        setHiddenActions(deriveFrom->d->m_hiddenActions);
}

WebEngineView::~WebEngineView()
{
    // Enforce deleting the page before deleting the shared profile.
    if (page()->parent() == this)
        delete page();
}

void WebEngineView::setHiddenActions(const std::vector<QWebEnginePage::WebAction> actions)
{
    for (const auto& action: d->m_hiddenActions)
        page()->action(action)->setVisible(true);

    d->m_hiddenActions = actions;

    for (const auto& action: d->m_hiddenActions)
        page()->action(action)->setVisible(false);
}

void WebEngineView::setUseActionsForLinks(bool enabled)
{
    d->m_useActionsForLinks = enabled;
}

bool WebEngineView::useActionsForLinks() const
{
    return d->m_useActionsForLinks;
}

void WebEngineView::setIgnoreSslErrors(bool ignore)
{
    d->m_ignoreSslErrors = ignore;
}

bool WebEngineView::hasIgnoreSslErrors() const
{
    return d->m_ignoreSslErrors;
}

void WebEngineView::setRedirectLinksToDesktop(bool enabled)
{
    d->m_redirectLinksToDesktop = enabled;
}

bool WebEngineView::isRedirectLinksToDesktop() const
{
    return d->m_redirectLinksToDesktop;
}

void WebEngineView::createPageWithNewProfile()
{
    // Since we are changing user agent here, create the off-the-record profile
    // to avoid mess in persistent storage.
    auto profile = std::make_shared<QWebEngineProfile>();

    // QWebEngineView does not take ownership of the custom QWebEnginePage set via setPage()
    // despite stating this in docs. So we need to delete it manually.
    // Use QPointer in case setPage() gets a fix someday.
    QPointer<QWebEnginePage> oldPage = page();

    // Deleting the page will trigger closing all of child windows.

    setPage(new WebEnginePage(profile.get(), this, *this));
    if (oldPage && oldPage->parent() == this)
        delete oldPage.data();

    d->m_masterPage = page();

    // Release ownership of the off-the-record profile which was used by the old page.
    // This profile may still be owned by child windows during closing.
    d->m_profile = profile;
}

void WebEngineView::contextMenuEvent(QContextMenuEvent* event)
{
    if (d->m_useActionsForLinks)
    {
        const QWebEngineContextMenuData data = page()->contextMenuData();

        if (!data.linkUrl().isEmpty())
        {
            QMenu* menu = new QMenu(this);
            menu->addActions(actions());
            menu->popup(event->globalPos());
            return;
        }
    }

    base_type::contextMenuEvent(event);
}

void WebEngineView::waitLoadFinished(std::chrono::milliseconds timeout)
{
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    connect(this, &QWebEngineView::loadFinished, &loop, [&loop](){ loop.quit(); });
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeout);
    loop.exec();
}

void WebEngineView::insertStyleSheet(const QString& name, const QString& source, bool immediately)
{
    QWebEngineScript script;
    const QString s = QString::fromLatin1(R"JS(
        (function() {
            var css = document.createElement('style');
            css.type = 'text/css';
            css.id = '%1';
            document.head.appendChild(css);
            css.innerText = '%2';
        })()
        )JS").arg(name).arg(source.simplified().replace('\'', "\\'"));
    if (immediately)
        page()->runJavaScript(s, QWebEngineScript::ApplicationWorld);

    script.setName(name);
    script.setSourceCode(s);
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    script.setRunsOnSubFrames(true);
    script.setWorldId(QWebEngineScript::ApplicationWorld);
    page()->scripts().insert(script);
}

void WebEngineView::removeStyleSheet(const QString& name, bool immediately)
{
    const QString s = QString::fromLatin1(R"JS(
        (function() {
            var element = document.getElementById('%1');
            element.outerHTML = '';
            delete element;
        })()
        )JS").arg(name);
    if (immediately)
        page()->runJavaScript(s, QWebEngineScript::ApplicationWorld);

    QWebEngineScript script = page()->scripts().findScript(name);
    page()->scripts().remove(script);
}

QWebEngineView* WebEngineView::createWindow(QWebEnginePage::WebWindowType type)
{
    Qt::WindowFlags flags = type == QWebEnginePage::WebDialog ? Qt::Dialog : Qt::Window;
    flags |= Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint
        | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint;

    auto window = new QMainWindow(nullptr, flags);

    static constexpr QSize kMinumumWindowSize(214, 0); //< Leave some space for window controls.
    window->setMinimumSize(kMinumumWindowSize);

    auto webView = new WebEngineView(window, this);

    window->setAttribute(Qt::WA_DeleteOnClose);

    if (d->m_masterPage)
    {
        connect(d->m_masterPage, &QObject::destroyed, window, &QMainWindow::close);

        connect(webView->page(), &QWebEnginePage::authenticationRequired, d->m_masterPage,
            &QWebEnginePage::authenticationRequired);
        connect(webView->page(), &QWebEnginePage::proxyAuthenticationRequired, d->m_masterPage,
            &QWebEnginePage::proxyAuthenticationRequired);
    }

    // setGeometry() expects a size excluding the window decoration, while geom includes it.
    const auto changeGeometry =
        [window](const QRect& geom)
        {
            window->setGeometry(geom.marginsRemoved(window->windowHandle()->frameMargins()));
        };

    switch (type)
    {
        case QWebEnginePage::WebBrowserWindow:
        case QWebEnginePage::WebDialog:
            // Allow JavaScript to change geomerty only for windows and dialogs.
            connect(webView->page(), &QWebEnginePage::geometryChangeRequested, changeGeometry);
            break;
        case QWebEnginePage::WebBrowserTab:
        case QWebEnginePage::WebBrowserBackgroundTab:
            window->setGeometry(this->window()->geometry());
            break;
        default:
            break;
    }

    connect(webView->page(), &QWebEnginePage::windowCloseRequested,  window, &QMainWindow::close);

    window->setCentralWidget(webView);
    window->show();

    return webView;
}
