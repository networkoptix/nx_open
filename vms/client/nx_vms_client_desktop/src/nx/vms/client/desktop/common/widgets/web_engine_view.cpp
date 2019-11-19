#include "web_engine_view.h"

#include <QtCore/QTimer>
#include <QtCore/QEventLoop>
#include <QtGui/QDesktopServices>
#include <QtGui/QContextMenuEvent>
#include <QtWidgets/QMenu>
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
    QScopedPointer<QWebEngineProfile> m_webEngineProfile;
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

WebEngineView::WebEngineView(QWidget* parent):
    base_type(parent),
    d(new Private())
{
    setPage(new WebEnginePage(this, *this));
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
    auto profile = new QWebEngineProfile(QString(), this);
    profile->setHttpUserAgent(userAgent);
    setPage(new WebEnginePage(profile, profile, *this));

    d->m_webEngineProfile.reset(profile);
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
