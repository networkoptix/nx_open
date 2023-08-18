// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webview_controller.h"

#include <chrono>
#include <vector>

#include <QtCore/QVariant>
#include <QtGui/private/qinputcontrol_p.h>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtWebChannel/QWebChannel>
#include <QtWebEngineCore/QWebEngineCertificateError>
#include <QtWebEngineQuick/QQuickWebEngineProfile>
#include <QtWebEngineQuick/private/qquickwebengineaction_p.h>
#include <QtWebEngineQuick/private/qquickwebenginescriptcollection_p.h>
#include <QtWebEngineQuick/private/qquickwebengineview_p.h>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/network_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/nx_network_ini.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/abstract_web_authenticator.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/integrations/c2p/jsapi/external.h>
#include <nx/vms/client/desktop/jsapi/auth.h>
#include <nx/vms/client/desktop/jsapi/globals.h>
#include <nx/vms/client/desktop/jsapi/logger.h>
#include <nx/vms/client/desktop/jsapi/resources.h>
#include <nx/vms/client/desktop/jsapi/self.h>
#include <nx/vms/client/desktop/jsapi/tab.h>
#include <nx/vms/client/desktop/resource_properties/camera/utils/camera_web_page_workarounds.h>
#include <nx/vms/client/desktop/ui/image_providers/web_page_icon_cache.h>
#include <ui/dialogs/common/certificate_selection_dialog.h>
#include <ui/dialogs/common/password_dialog.h>
#include <ui/graphics/items/standard/graphics_qml_view.h>
#include <ui/workbench/workbench_context.h>
#include <utils/web_downloader.h>

#include "webview_window.h"

Q_DECLARE_METATYPE(std::shared_ptr<QQuickItem>)

namespace nx::vms::client::desktop {

static_assert(
    (int) WebViewController::LoadStartedStatus == (int)QQuickWebEngineView::LoadStartedStatus);
static_assert(
    (int) WebViewController::LoadStoppedStatus == (int) QQuickWebEngineView::LoadStoppedStatus);
static_assert(
    (int) WebViewController::LoadSucceededStatus == (int) QQuickWebEngineView::LoadSucceededStatus);
static_assert(
    (int) WebViewController::LoadFailedStatus == (int) QQuickWebEngineView::LoadFailedStatus);

using namespace std::chrono;

namespace {

// Should be mapped to non-public QQuickWebEngineAuthenticationDialogRequest::AuthenticationType.
enum AuthenticationDialogRequestAuthenticationType
{
    AuthenticationTypeHTTP,
    AuthenticationTypeProxy
};

QUrl urlFromResource(const QnResourcePtr& resource)
{
    if (!resource)
        return {};

    QUrl result = resource->getUrl();
    if (const auto networkResource = resource.dynamicCast<QnNetworkResource>())
        result.setPort(networkResource->httpPort());
    return result;
}

/**
 * Allows to monitor if certain number of attemps is exceeded withing a time perdiod.
 */
class AttemptCounter
{
public:
    AttemptCounter() { setLimit(1); }

    void reset()
    {
        m_next = 0;
        m_count = 0;
    }

    void setLimit(size_t limit)
    {
        reset();

        if (limit == 0)
            return;

        m_timeStamps.resize(limit, {});
    }

    size_t limit() const { return m_timeStamps.size(); }

    size_t count() const { return m_count; }

    void registerAttempt()
    {
        if (m_count < limit())
            ++m_count;

        m_timeStamps[m_next] = steady_clock::now();
        m_next = (m_next + 1) % limit();
    }

    milliseconds timeFrame() const
    {
        if (m_count == 0)
            return milliseconds::zero();

        size_t first = m_next % limit();
        size_t last = m_next == 0 ? m_count - 1 : m_next - 1;

        return duration_cast<milliseconds>(m_timeStamps[last] - m_timeStamps[first]);
    }

    bool exceeded(milliseconds time) const
    {
        return m_count == limit() && timeFrame() < time;
    }

private:
    // Ring buffer.
    std::vector<steady_clock::time_point> m_timeStamps;
    size_t m_next = 0;
    size_t m_count = 0;
};

void jsConfirm(QWidget* parent, QObject* request, const QString& title, const QString& message)
{
    const auto button = QMessageBox::information(
        parent, title, message.toHtmlEscaped(), QMessageBox::Ok | QMessageBox::Cancel);

    if (button == QMessageBox::Ok)
        QMetaObject::invokeMethod(request, "dialogAccept");
    else
        QMetaObject::invokeMethod(request, "dialogReject");
}

static QList<QAction*> createActions(const QVariant& webActions, QObject* parent)
{
    QList<QAction*> result;

    const auto webActionsList = webActions.toList();

    // Make a list of QActions from the list of QQuickWebEngineActions.
    for (const QVariant& v: webActionsList)
    {
        QAction* action = new QAction(parent);

        if (const auto webAction = v.value<QQuickWebEngineAction*>())
        {
            action->setText(webAction->text());
            action->setEnabled(webAction->isEnabled());

            QObject::connect(action, &QAction::triggered,
                webAction, &QQuickWebEngineAction::trigger);
            QObject::connect(webAction, &QQuickWebEngineAction::enabledChanged,
                action, [webAction, action] { action->setEnabled(webAction->isEnabled()); });
        }
        else
        {
            // Create separator when webAction object is null.
            action->setSeparator(true);
        }

        result << action;
    }

    return result;
}

constexpr milliseconds kDialogRequestSmallInterval = 10s;
constexpr int kDialogRequestLimit = 3;

static constexpr auto kResourceIdProperty = "resourceId";
static const QUrl kQmlComponentUrl("qrc:/qml/Nx/Controls/WebEngineView.qml");

static constexpr auto kWebViewId = "webView";
static constexpr auto kControllerProperty = "controller";

} // namespace

struct WebViewController::Private: public QObject
{
    using Callback = nx::utils::MoveOnlyFunc<void(QQuickItem*)>;

    WebViewController* const q;
    QPointer<QQuickWidget> quickWidget;
    QPointer<GraphicsQmlView> qmlView;
    QnResourcePtr resource;

    std::vector<Callback> delayedCalls;

    QJSValue authRequest;
    std::shared_ptr<AbstractWebAuthenticator> authenticator;

    AttemptCounter dialodCounter;

    bool enableClientApi = true;
    std::vector<std::tuple<QString, ObjectFactory>> apiObjectFactories;

    CertificateValidationFunc certificateValidator;

    Private(WebViewController* q):
        q(q)
    {
        dialodCounter.setLimit(kDialogRequestLimit);
    }

    bool acceptDialogRequest(QObject* request)
    {
        // Prevent showing the default dialog.
        request->setProperty("accepted", true);

        dialodCounter.registerAttempt();
        if (!widget()->isVisible() || dialodCounter.exceeded(kDialogRequestSmallInterval))
        {
            // Block too many attempts to show the dialog, otherwise certain websites would make
            // the UI unusable.
            // Dialogs should also be blocked for invisible pages:
            //  - pages on inactive layouts
            //  - when camera settings dialog is closed or switched to a tab other than webpage)
            QMetaObject::invokeMethod(request, "dialogReject");
            return false;
        }

        return true;
    }

    void tryEnableApiObject(const QString& name, const ObjectFactory& factory)
    {
        disableClientApiObject(name);
        if (enableClientApi)
            injectClientApiObject(name, factory);
    }

    void setClientApiEnabled(bool value)
    {
        if (enableClientApi == value)
            return;

        enableClientApi = value;

        for (const auto& [name, factory]: apiObjectFactories)
            tryEnableApiObject(name, factory);

        q->reload();
    }

    QQuickItem* rootObject() const
    {
        if (quickWidget)
            return quickWidget->rootObject();
        if (qmlView)
            return qmlView->rootObject();
        return nullptr;
    }

    QWidget* widget() const
    {
        if (quickWidget)
            return quickWidget;
        if (qmlView)
            return qmlView->scene()->views().first();
        return nullptr;
    }

    QQuickWebEngineView* webEngineView()
    {
        if (const auto root = rootObject())
            return root->findChild<QQuickWebEngineView*>(kWebViewId);
        return nullptr;
    }

    void injectClientApiObject(const QString& name, ObjectFactory factory)
    {
        callWhenReady(
            [name, objectFactory = std::move(factory)](QQuickItem* root)
            {
                const auto webChannel = root->findChild<QWebChannel*>("nxWebChannel");
                if (!webChannel)
                    return;

                // We should set "enableInjections" to true before registering any objects because
                // it assigns "injectScripts" property to "userScripts" (see WebEngineView.qml)
                // and "registerObjects" internally uses "userScripts".
                root->setProperty("enableInjections", true);

                if (NX_ASSERT(!webChannel->registeredObjects().contains(name)),
                    "Object with the same path is registered already")
                {
                    webChannel->registerObject(name, objectFactory(webChannel));
                }
            });
    }

    void disableClientApiObject(const QString& name)
    {
        callWhenReady(
            [name](QQuickItem* root)
            {
                const auto webChannel = root->findChild<QWebChannel*>("nxWebChannel");
                if (!webChannel)
                    return;

                const auto registeredObjects = webChannel->registeredObjects();
                const auto it = registeredObjects.find(name);
                if (it != registeredObjects.cend())
                    webChannel->deregisterObject(it.value());
            });
    }

    void callWhenReady(Callback callback)
    {
        if (QQuickItem* root = rootObject())
            callback(root);
        else
            delayedCalls.emplace_back(std::move(callback));
    }

    void updateProxyInfo()
    {
        QQuickItem* root = rootObject();
        if (!root)
            return;

        // Setup resourceId property if we need to proxy this page via a server.
        // This may create a new web engine profile in order to isolate proxies,
        // so do it before setting the url.
        const bool isProxied = resource && !resource->getParentId().isNull();

        root->setProperty(kResourceIdProperty, isProxied ? resource->getId().toString() : "");
    }

    void acceptAuth(const QString& username, const QString& password)
    {
        if (!NX_ASSERT(authRequest.toQObject()))
            return;

        QMetaObject::invokeMethod(
            authRequest.toQObject(),
            "dialogAccept",
            Q_ARG(QString, username),
            Q_ARG(QString, password));

        authRequest = {};
    }

    void rejectAuth()
    {
        if (!NX_ASSERT(authRequest.toQObject()))
            return;

        QMetaObject::invokeMethod(authRequest.toQObject(), "dialogReject");

        authRequest = {};
    }

    void auth(const QAuthenticator& credentials)
    {
        auto requestObject = authRequest.toQObject();
        if (!requestObject)
            return;

        const bool isPasswordRequired = credentials.password().isNull();

        if (!isPasswordRequired)
            return acceptAuth(credentials.user(), credentials.password());

        QString text;
        if (requestObject->property("type").toInt() == AuthenticationTypeProxy)
        {
            text = tr("The proxy %1 requires a username and password.")
                .arg(requestObject->property("proxyHost").toString());
        }
        else
        {
            const auto url = requestObject->property("url").toUrl();
            text = url.toString(QUrl::RemovePassword | QUrl::RemovePath | QUrl::RemoveQuery);
        }

        PasswordDialog dialog(widget());
        dialog.setText(text);
        if (!credentials.isNull())
            dialog.setUsername(credentials.user());

        if (dialog.exec() == QDialog::Accepted)
            acceptAuth(dialog.username(), dialog.password());
        else
            rejectAuth();
    }

    QList<QAction*> getAdditionalActions(QObject* parent) const
    {
        QQuickItem* root = rootObject();
        if (!root)
            return {};

        const auto webView = root->findChild<QQuickWebEngineView*>(kWebViewId);
        if (!webView)
            return {};

        QList<QAction*> result;

        if (ini().developerMode)
        {
            QAction* devTools = new QAction(parent);
            devTools->setText(tr("Developer Tools"));

            connect(devTools, &QAction::triggered, this,
                [webView = QPointer(webView)]()
                {
                    if (!NX_ASSERT(webView))
                        return;

                    QMetaObject::invokeMethod(webView, "showDevTools");
                });

            result.append(devTools);
        }

        return result;
    }
};

WebViewController::WebViewController(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

WebViewController::~WebViewController()
{}

void WebViewController::loadIn(QQuickWidget* quickWidget)
{
    if (d->quickWidget)
    {
        if (const auto window = d->quickWidget->window())
            window->removeEventFilter(this);
        d->quickWidget->removeEventFilter(this);
        d->quickWidget->disconnect(this);
    }

    d->quickWidget = quickWidget;
    connect(quickWidget, &QQuickWidget::statusChanged, this, &WebViewController::onStatusChanged);

    quickWidget->setClearColor(quickWidget->palette().window().color());
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quickWidget->setSource(kQmlComponentUrl);

    // Watch for position changes to move offscreen window accordingly.
    quickWidget->installEventFilter(this);
    if (const auto window = quickWidget->window())
        window->installEventFilter(this);
}

void WebViewController::loadIn(GraphicsQmlView* qmlView)
{
    d->qmlView = qmlView;
    connect(qmlView, &GraphicsQmlView::statusChanged, this, &WebViewController::onStatusChanged);
    qmlView->installEventFilter(this);
    qmlView->setSource(kQmlComponentUrl);
}

QWidget* WebViewController::containerWidget() const
{
    return d->widget();
}

bool WebViewController::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == d->quickWidget || (d->quickWidget && watched == d->quickWidget->window()))
    {
        switch (event->type())
        {
            case QEvent::Show:
                if (const auto window = d->quickWidget->window())
                {
                    // Workaround for Qt WebEngine - popups should have on-screen parent window.
                    d->quickWidget->quickWindow()->setProperty(
                        "_popup_parent",
                        QVariant::fromValue(window->windowHandle()));
                }
                break;
            case QEvent::Move:
            {
                // Workaround for broken popups in Qt WebEngine - offscreen QQuickWindow position
                // should be in sync with onscreen window so the popup can be positioned correctly.
                const auto topLeft = d->quickWidget->mapToGlobal(QPoint{0, 0});
                d->quickWidget->quickWindow()->setPosition(topLeft);
                break;
            }
            default:
                break;
        }
    }

    if (event->type() == QEvent::ShortcutOverride)
    {
        // Filter out certain ShortcutOverride events to avoid an assert in QtWebEngine code.
        // TODO: Check if this is still needed in Qt6.
        auto keyEvent = static_cast<QKeyEvent*>(event);
        if (QInputControl::isCommonTextEditShortcut(keyEvent))
        {
            // Hide the ShortcutOverride event for common shortcuts and deliver them as key press
            // events. The ShortcutOverride should be accepted here in order to notify Qt that
            // the shortcut is going to be handled by the QtWebEngine.
            keyEvent->accept();
            return true;
        }
    }

    return base_type::eventFilter(watched, event);
}

void WebViewController::onStatusChanged(QQuickWidget::Status status)
{
    if (status != QQuickWidget::Ready)
        return;

    QQuickItem* root = d->rootObject();
    if (!root)
        return;

    connect(root, SIGNAL(loadingStatusChanged(int)), this, SLOT(onLoadingStatusChanged(int)));

    const auto webView = root->findChild<QQuickWebEngineView*>(kWebViewId);
    if (!NX_ASSERT(webView))
        return;

    connect(webView, &QQuickWebEngineView::profileChanged, this,
        [this, webView]
        {
            emit profileChanged(webView->profile());
        });

    connect(webView, &QQuickWebEngineView::windowCloseRequested,
        this, &WebViewController::windowCloseRequested);
    connect(webView, &QQuickWebEngineView::loadProgressChanged,
        this, &WebViewController::loadProgressChanged);
    connect(webView, &QQuickWebEngineView::urlChanged,
        this, &WebViewController::urlChanged);

    root->setProperty(kControllerProperty, QVariant::fromValue<QObject*>(this));

    for (const auto& func: d->delayedCalls)
        func(root);
    d->delayedCalls.clear();

    emit rootReady(root);
}

void WebViewController::onLoadingStatusChanged(int status)
{
    switch (status)
    {
        case QQuickWebEngineView::LoadStartedStatus:
            emit loadStarted();
            break;
        case QQuickWebEngineView::LoadSucceededStatus:
            emit loadFinished(true);
            break;
        case QQuickWebEngineView::LoadFailedStatus:
        case QQuickWebEngineView::LoadStoppedStatus:
            emit loadFinished(false);
            break;
        default:
            break;
    }
    emit loadingStatusChanged(status);
}

void WebViewController::closeWindows()
{
    if (QQuickItem* root = d->rootObject())
        QMetaObject::invokeMethod(root, "closeWindows");
}

void WebViewController::runJavaScript(const QString& source)
{
    d->callWhenReady(
        [source](QQuickItem* root)
        {
            const auto webView = root->findChild<QQuickWebEngineView*>(kWebViewId);
            QMetaObject::invokeMethod(webView, "runJavaScript", Q_ARG(QString, source));
        });
}

void WebViewController::setHtml(const QString& html, const QUrl& baseUrl)
{
    d->callWhenReady(
        [html, baseUrl](QQuickItem* root)
        {
            const auto webView = root->findChild<QQuickWebEngineView*>(kWebViewId);
            QMetaObject::invokeMethod(
                webView, "loadHtml", Q_ARG(QString, html), Q_ARG(QUrl, baseUrl));
        });
}

void WebViewController::setOffTheRecord(bool offTheRecord)
{
    d->callWhenReady(
        [offTheRecord](QQuickItem* root)
        {
            root->setProperty("offTheRecord", offTheRecord);
        });
}

bool WebViewController::canGoBack() const
{
    const auto root = d->rootObject();
    if (!root)
        return false;

    const auto webView = root->findChild<QQuickWebEngineView*>(kWebViewId);

    return webView && webView->canGoBack();
}

void WebViewController::reload()
{
    const auto root = d->rootObject();
    if (!root)
        return;

    if (const auto webView = root->findChild<QQuickWebEngineView*>(kWebViewId))
        webView->reload();
}

void WebViewController::goBack()
{
    const auto root = d->rootObject();
    if (!root)
        return;

    if (const auto webView = root->findChild<QQuickWebEngineView*>(kWebViewId))
        webView->goBack();
}

void WebViewController::stop()
{
    if (const auto webView = d->webEngineView())
        webView->stop();
}

int WebViewController::loadProgress() const
{
    const auto root = d->rootObject();
    if (!root)
        return 0;

    if (const auto webView = root->findChild<QQuickWebEngineView*>(kWebViewId))
        return webView->loadProgress();

    return 0;
}

void WebViewController::load(const QUrl& url)
{
    load(QnResourcePtr(nullptr), url);
}

void WebViewController::load(const QnResourcePtr& resource, const QUrl& url)
{
    QUrl pageUrl = url;

    if (pageUrl.isEmpty() && resource)
        pageUrl = urlFromResource(resource);

    if (d->resource)
        d->resource->disconnect(this);

    d->resource = resource;

    d->callWhenReady(
        [this, pageUrl](QQuickItem* root)
        {
            d->updateProxyInfo();
            root->setProperty("url", pageUrl);
        });

    if (!d->resource)
        return;

    // Reset the page when proxy (parent server) changes.
    connect(d->resource.get(), &QnResource::parentIdChanged, this,
        [this]
        {
            d->updateProxyInfo();
            reload();
        });

    connect(resource.get(), &QnResource::urlChanged, this,
        [this, resource]()
        {
            d->callWhenReady(
                [resource](QQuickItem* root)
                {
                    root->setProperty("url", urlFromResource(resource));
                });
        });

    const auto webpageResource = resource.dynamicCast<QnWebPageResource>();
    if (!webpageResource)
        return;

    const auto handleApiEnabledChanged =
        [this](const QnWebPageResourcePtr& webpageResource)
        {
            d->setClientApiEnabled(
                webpageResource->subtype() == nx::vms::api::WebPageSubtype::clientApi);
        };

    handleApiEnabledChanged(webpageResource);
    connect(webpageResource.get(), &QnWebPageResource::subtypeChanged,
        this, handleApiEnabledChanged);
}

QUrl WebViewController::url() const
{
    QQuickItem* root = d->rootObject();
    if (!root)
        return QUrl();

    return root->property("url").toUrl();
}

QList<QAction*> WebViewController::getContextMenuActions(QObject* parent) const
{
    QQuickItem* root = d->rootObject();
    if (!root)
        return {};

    const auto webView = root->findChild<QQuickWebEngineView*>(kWebViewId);
    if (!webView)
        return {};

    QVariant emptyRequest;
    QVariant webActions;

    const bool ok = QMetaObject::invokeMethod(
        webView,
        "getContextMenuActions",
        Q_RETURN_ARG(QVariant, webActions),
        Q_ARG(QVariant, emptyRequest));

    if (!ok)
        return {};

    auto result = createActions(webActions, parent);

    result.append(d->getAdditionalActions(parent));

    return result;
}

void WebViewController::showMenu(float x, float y, const QVariant& webActions)
{
    QQuickItem* root = d->rootObject();
    if (!root)
        return;

    const auto webView = root->findChild<QQuickWebEngineView*>(kWebViewId);
    if (!webView)
        return;

    const auto menu = new QMenu(d->widget());

    menu->setAttribute(Qt::WA_DeleteOnClose);

    const auto menuActions = createActions(webActions, menu);
    menu->addActions(menuActions);
    menu->addActions(d->getAdditionalActions(menu));

    const QPointF globalPos = webView->mapToGlobal({x, y});
    menu->popup(globalPos.toPoint());
}

QObject* WebViewController::newWindow()
{
    // Create new WebViewWindow with the same parent as current window parent.
    // This allows all web windows to be on the same level.
    const auto currentWindow = d->widget()->window();
    auto newWindow = new WebViewWindow(
        currentWindow && currentWindow->parentWidget()
            ? currentWindow->parentWidget()
            : currentWindow);

    newWindow->setAttribute(Qt::WA_DeleteOnClose);

    // Make registered object factories available inside the new window.
    for (const auto& [name, factory]: d->apiObjectFactories)
         newWindow->controller()->registerApiObjectWithFactory(name, factory);

    return newWindow;
}

void WebViewController::setRedirectLinksToDesktop(bool enabled)
{
    d->callWhenReady(
        [enabled](QQuickItem* root)
        {
            root->setProperty("redirectLinksToDesktop", enabled);
        });
}

void WebViewController::setMenuNavigation(bool enabled)
{
    d->callWhenReady(
        [enabled](QQuickItem* root)
        {
            root->setProperty("menuNavigation", enabled);
        });
}

void WebViewController::setMenuSave(bool enabled)
{
    d->callWhenReady(
        [enabled](QQuickItem* root)
        {
            root->setProperty("menuSave", enabled);
        });
}

bool WebViewController::isWebPageContextMenuEnabled() const
{
    const QQuickItem* root = d->rootObject();
    return root && root->property("enableCustomMenu").toBool();
}

void WebViewController::setWebPageContextMenuEnabled(bool enabled)
{
    d->callWhenReady(
        [enabled](QQuickItem* root)
        {
            root->setProperty("enableCustomMenu", enabled);
        });
}

void WebViewController::addProfileScript(const QWebEngineScript& script)
{
    d->callWhenReady(
        [&script](QQuickItem* root) mutable
        {
            const auto profile = root->property("profile").value<QQuickWebEngineProfile*>();
            if (!profile)
                return;

            // Avoid script duplication with the same name.
            auto scripts = profile->userScripts();
            if (scripts->contains(script))
                scripts->remove(script);

            scripts->insert(script);
        });
}

void WebViewController::registerObject(const QString& name, QObject* object)
{
    // Dummy factory, always returning already created object.
    registerApiObjectWithFactory(name,
        [object](QObject* /*parent*/)
        {
            return object;
        });
}

void WebViewController::registerApiObjectWithFactory(
    const QString& name,
    ObjectFactory factory)
{
    // Remember registered factories so they can be passed to a new window.
    d->apiObjectFactories.emplace_back(name, factory);
    d->tryEnableApiObject(name, factory);
}

void WebViewController::setAuthenticator(std::shared_ptr<AbstractWebAuthenticator> authenticator)
{
    if (d->authenticator)
        d->authenticator->disconnect(this);

    d->authenticator = authenticator;

    if (d->authenticator)
    {
        connect(authenticator.get(), &AbstractWebAuthenticator::authReady,
            this->d.get(), &Private::auth);
    }
}

QVariant WebViewController::suspend()
{
    if (!d->qmlView)
        return QVariant();

    QQuickItem* root = d->qmlView->rootObject();
    if (!root)
        return QVariant();

    // Make page invisible so it can transition to Frozen state.
    // Do it before taking the item because otherwise its visibility does not change.
    root->setVisible(false);

    std::shared_ptr<QQuickItem> rootItem = d->qmlView->takeRootObject();
    if (!rootItem)
        return QVariant();

    closeWindows();

    if (const auto webView = rootItem->findChild<QQuickWebEngineView*>(kWebViewId))
        webView->setLifecycleState(QQuickWebEngineView::LifecycleState::Frozen);

    rootItem->setProperty(kControllerProperty, QVariant::fromValue<QObject*>(nullptr));

    return QVariant::fromValue(rootItem);
}

void WebViewController::resume(QVariant state)
{
    auto rootItem = state.value<std::shared_ptr<QQuickItem>>();
    if (!rootItem)
        return;

    if (!d->qmlView)
        return;

    rootItem->setProperty(kControllerProperty, QVariant::fromValue<QObject*>(this));
    if (const auto webView = rootItem->findChild<QQuickWebEngineView*>(kWebViewId))
        webView->setLifecycleState(QQuickWebEngineView::LifecycleState::Active);

    rootItem->setVisible(true);
    d->qmlView->setRootObject(rootItem);

    emit loadingStatusChanged(QQuickWebEngineView::LoadSucceededStatus);
}

void WebViewController::setCertificateValidator(CertificateValidationFunc validator)
{
    d->certificateValidator = validator;
}

void WebViewController::initClientApiSupport(
    QnWorkbenchContext* context,
    QnWorkbenchItem* item,
    ClientApiAuthCondition authCondition)
{
    if (!NX_ASSERT(context))
        return;

    registerApiObjectWithFactory("external",
        [=](QObject* parent) -> QObject*
        {
            using External = integrations::c2p::jsapi::External;
            return new External(context, item, parent);
        });

    registerApiObjectWithFactory("vms.tab",
        [=](QObject* parent) -> QObject*
        {
            return new jsapi::Tab(context, parent);
        });

    registerApiObjectWithFactory("vms.resources",
        [=](QObject* parent) -> QObject*
        {
            return new jsapi::Resources(parent);
        });

    registerApiObjectWithFactory("vms.log",
        [](QObject* parent) -> QObject*
        {
            return new jsapi::Logger(parent);
        });

    registerApiObjectWithFactory("vms.auth",
        [=](QObject* parent) -> QObject*
        {
            // Objects may be used with another controller, so use webView to get current url.
            return new jsapi::Auth(
                context->systemContext(),
                [authCondition, webView = QPointer(d->rootObject())]
                {
                    if (!NX_ASSERT(webView))
                        return false;

                    return authCondition(webView->property("url").toUrl());
                },
                parent);
        });

    registerApiObjectWithFactory("vms.self",
        [=](QObject* parent) -> QObject*
        {
            return new jsapi::Self(item, parent);
        });

    registerApiObjectWithFactory("vms",
        [](QObject* parent) -> QObject*
        {
            return new jsapi::Globals(parent);
        });
}

void WebViewController::registerMetaType()
{
    qRegisterMetaType<std::shared_ptr<QQuickItem>>(); //< Used for saving web view root item.
}

void WebViewController::requestJavaScriptDialog(QObject* request)
{
    if (!d->acceptDialogRequest(request))
        return;

    // Should be mapped to non-public QQuickWebEngineJavaScriptDialogRequest::DialogType.
    enum JavaScriptDialogRequestDialogType
    {
        DialogTypeAlert,
        DialogTypeConfirm,
        DialogTypePrompt,
        DialogTypeBeforeUnload
    };

    // Show dialogs similar to QWebEnginePage default behavior.
    switch (request->property("type").toInt())
    {
        case DialogTypeAlert:
            QMessageBox::information(
                d->widget(),
                request->property("title").toString(),
                request->property("message").toString().toHtmlEscaped());
            QMetaObject::invokeMethod(request, "dialogAccept");
            return;
        case DialogTypeConfirm:
            jsConfirm(
                d->widget(),
                request,
                request->property("title").toString(),
                request->property("message").toString());
            return;
        case DialogTypeBeforeUnload:
        {
            // Match the string with translated string in QWebEnginePagePrivate.
            // The same string is passed to default QWebEnginePage::javaScriptConfirm().
            auto message = QCoreApplication::translate(
                "QWebEnginePage",
                "Are you sure you want to leave this page? Changes that you made may not be saved.");
            jsConfirm(d->widget(), request, request->property("title").toString(), message);
            return;
        }
        case DialogTypePrompt:
        {
            bool ok;
            const QString text = QInputDialog::getText(
                d->widget(),
                request->property("title").toString(),
                request->property("message").toString().toHtmlEscaped(),
                QLineEdit::Normal,
                request->property("defaultText").toString(),
                &ok);

            if (ok)
                QMetaObject::invokeMethod(request, "dialogAccept", Q_ARG(QString, text));
            else
                QMetaObject::invokeMethod(request, "dialogReject");
            return;
        }
    }
}

void WebViewController::requestAuthenticationDialog(QJSValue request)
{
    NX_ASSERT(request.toQObject());

    auto requestObject = request.toQObject();

    if (!d->acceptDialogRequest(requestObject))
        return;

    if (d->authRequest.toQObject() != nullptr)
    {
        QMetaObject::invokeMethod(requestObject, "dialogReject");
        return;
    }

    // Save the request object so it won't be deleted by GC.
    d->authRequest = request;

    if (requestObject->property("type").toInt() == AuthenticationTypeProxy || !d->authenticator)
        d->auth({});
    else
        d->authenticator->requestAuth(requestObject->property("url").toUrl());
}

void WebViewController::requestFileDialog(QObject* request)
{
    if (!d->acceptDialogRequest(request))
        return;

    // Should be mapped to non-public QQuickWebEngineFileDialogRequest::FileMode.
    enum FileDialogRequestFileMode
    {
        FileModeOpen,
        FileModeOpenMultiple,
        FileModeUploadFolder,
        FileModeSave
    };

    QStringList files;
    QString str;

    static constexpr auto kDefaultFileName = "defaultFileName";

    // Show dialogs similar to QWebEnginePage default behavior.
    switch (request->property("mode").toInt())
    {
        case FileModeOpenMultiple:
            files = QFileDialog::getOpenFileNames(d->widget(), QString());
            break;
        case FileModeUploadFolder:
            str = QFileDialog::getExistingDirectory(d->widget(), tr("Select folder to upload"));
            if (!str.isNull())
                files << str;
            break;
        case FileModeSave:
            str = utils::WebDownloader::selectFile(request->property(kDefaultFileName).toString(),
                d->widget());
            if (!str.isNull())
                files << str;
            break;
        case FileModeOpen:
            str = QFileDialog::getOpenFileName(d->widget(), QString(),
                request->property(kDefaultFileName).toString());
            if (!str.isNull())
                files << str;
            break;
    }

    if (!files.isEmpty())
        QMetaObject::invokeMethod(request, "dialogAccept", Q_ARG(QStringList, files));
    else
        QMetaObject::invokeMethod(request, "dialogReject");
}

void WebViewController::requestColorDialog(QObject* request)
{
    if (!d->acceptDialogRequest(request))
        return;

    const auto initialColor = request->property("color").value<QColor>();
    const auto dialog = new QColorDialog(initialColor, d->widget());

    // For unknown reason invoking dialogAccept(color) directly from here causes a crash
    // inside WebEngine code. Copy QWebEnginePage implementation of showColorDialog()
    // which shows the dialog after returning from the colorDialogRequested signal handler.

    connect(dialog, SIGNAL(colorSelected(QColor)), request, SLOT(dialogAccept(QColor)));
    connect(dialog, SIGNAL(rejected()), request, SLOT(dialogReject()));

    // Delete when done.
    connect(dialog, SIGNAL(colorSelected(QColor)), dialog, SLOT(deleteLater()));
    connect(dialog, SIGNAL(rejected()), dialog, SLOT(deleteLater()));

    dialog->open();
}

void WebViewController::requestCertificateDialog(QObject* selection)
{
    CertificateSelectionDialog dialog(
        d->widget(),
        selection->property("host").toUrl(),
        QQmlListReference(selection, "certificates"));

    if (dialog.exec() == QDialog::Accepted)
        QMetaObject::invokeMethod(selection, "select", Q_ARG(int, dialog.selectedIndex()));
    else
        QMetaObject::invokeMethod(selection, "selectNone");
}

bool WebViewController::verifyCertificate(const QWebEngineCertificateError& error)
{
    if (!nx::network::ini().verifySslCertificates)
        return true;

    QString pemString;
    for (const auto& cert: error.certificateChain())
        pemString += cert.toPem();

    return d->certificateValidator
        ? d->certificateValidator(pemString, error.url())
        : false;
}

QUrl WebViewController::resourceUrl() const
{
    return urlFromResource(d->resource);
}

WebPageIconCache* WebViewController::iconCache() const
{
    const auto iconCache = appContext()->webPageIconCache();
    QQmlEngine::setObjectOwnership(iconCache, QQmlEngine::CppOwnership);
    return iconCache;
}

} // namespace nx::vms::client::desktop
