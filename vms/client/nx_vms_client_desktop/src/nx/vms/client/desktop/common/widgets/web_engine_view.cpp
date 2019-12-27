#include "web_engine_view.h"

#include <QtCore/QTimer>
#include <QtCore/QEventLoop>
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
    QSharedPointer<QWebEngineProfile> m_webEngineProfile;
    std::vector<QWebEnginePage::WebAction> m_hiddenActions;
};

namespace {

class WebEnginePage: public QWebEnginePage
{
    using base_type = QWebEnginePage;

public:
    WebEnginePage(QWebEngineProfile* profile, QObject* parent, const WebEngineView& view):
        QWebEnginePage(profile, parent),
        m_view(view)
    {
        init();
    }

    WebEnginePage(QObject* parent, const WebEngineView& view):
        QWebEnginePage(parent),
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
    if (d->m_webEngineProfile)
        setPage(new WebEnginePage(d->m_webEngineProfile.data(), d->m_webEngineProfile.data(), *this));
    else
        setPage(new WebEnginePage(this, *this));

    if (deriveFrom)
        setHiddenActions(deriveFrom->d->m_hiddenActions);
}

WebEngineView::~WebEngineView()
{
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

void WebEngineView::createPageWithUserAgent(const QString& userAgent)
{
    // Since we are changine user agent here, create the off-the-record profile
    // to avoid mess in persistent storage.

    // Make new profile a parent of the web page because it should outlive the page.
    auto profile = new QWebEngineProfile(this);
    if (!userAgent.isEmpty())
        profile->setHttpUserAgent(userAgent);
    setPage(new WebEnginePage(profile, profile, *this));

    d->m_webEngineProfile.reset(profile);
    emit profileChanged();
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

    static constexpr QSize kMinumumWindowSize(120, 0); //< Leave some space for window controls.
    window->setMinimumSize(kMinumumWindowSize);

    auto webView = new WebEngineView(window, this);

    // Profile change means that our parent page is no longer used.
    // But it is going to stay in memory because its parent is the shared profile which is used
    // by all derived pages.
    // Closing the window dereferences the profile pointer so that all pages will be freed.
    connect(this, &WebEngineView::profileChanged, window, &QMainWindow::close);
    window->setAttribute(Qt::WA_DeleteOnClose);
    connect(page(), &QObject::destroyed, window, &QMainWindow::close);

    connect(webView->page(), &QWebEnginePage::authenticationRequired, page(),
        &QWebEnginePage::authenticationRequired);
    connect(webView->page(), &QWebEnginePage::proxyAuthenticationRequired, page(),
        &QWebEnginePage::proxyAuthenticationRequired);

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
