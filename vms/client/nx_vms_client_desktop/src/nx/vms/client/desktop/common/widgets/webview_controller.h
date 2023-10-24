// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <memory>
#include <optional>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>
#include <QtQml/QJSValue>
#include <QtQuickWidgets/QQuickWidget>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>

class QQuickWebEngineProfile;
class QWebEngineCertificateError;
class QWebEngineScript;

class QnWorkbenchContext;
class QnWorkbenchItem;

namespace nx::vms::client::desktop {

class GraphicsQmlView;
class WebPageIconCache;
class AbstractWebAuthenticator;

/**
 * C++ part of `WebEngineView` QML component which is used for controlling its behavior.
 */
class NX_VMS_CLIENT_DESKTOP_API WebViewController: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    // Should be mapped to non-public QQuickWebEngineView::LoadStatus.
    enum WebEngineViewLoadStatus
    {
        LoadStartedStatus,
        LoadStoppedStatus,
        LoadSucceededStatus,
        LoadFailedStatus
    };

    using CertificateValidationFunc =
        std::function<bool(const QString& certificateChain, const QUrl& url)>;

    using ClientApiAuthCondition = std::function<bool(const QUrl& url)>;

public:
    WebViewController(QObject* parent);
    virtual ~WebViewController() override;

    /** Load WebEngineView QML inside QQuickWidget. */
    void loadIn(QQuickWidget* quickWidget);

    /** Load WebEngineView QML inside GraphicsQmlView. */
    void loadIn(GraphicsQmlView* qmlView);

    /** Load page with specified url, without vms proxy. */
    void load(const QUrl& url);

    /**
     * Load resource by http url, using resource parent as a proxy. Alternative url can be
     * specified for opening via resource proxy, but if it points to different domain, the page
     * may not load due to security settings (depends on vms server config `allowThirdPartyProxy`).
     */
    void load(const QnResourcePtr& resource, const QUrl& url = {});

    /** Reload current web page. */
    void reload();

    /** Go to previous web page. */
    void goBack();

    /** Stop loading web page. */
    void stop();

    /** Get currently opened url. */
    QUrl url() const;

    /** Current load progress of the web content, represented as an integer between 0 and 100. */
    int loadProgress() const;

    /** Set HTML content for display. This function is asynchronous.*/
    void setHtml(const QString& html, const QUrl& baseUrl = QUrl());

    /** Forces all clicked links to be opened in current OS web browser. */
    void setRedirectLinksToDesktop(bool enabled);

    /** Show `Back`, `Forward` and `Stop/Reload` menu actions. Also allows opening new windows. */
    void setMenuNavigation(bool enabled);

    /** Show save menu actions such as `Save page` or `Save image`. */
    void setMenuSave(bool enabled);

    /** Enable web page to have its own context menu. */
    void setWebPageContextMenuEnabled(bool enabled);

    /** Returns true if web page is allowed to have its own context menu. */
    bool isWebPageContextMenuEnabled() const;

    /** Sets whether to load a web page icon. */
    void setLoadIcon(bool value);

    /**
     * Expose QObject properties and slots/signals to the page JavaScript.
     * Lifetime of the object is controlled by its creator. Sutable for separate web views
     * which are not stored between runs.
     * Supports complex domain-like naming, like "nx.area.management".
     */
    void registerObject(const QString& name, QObject* object);

    /**
     * Registers API object created by specified factory. Its properties and slots/signals are
     * exposed to the page JavaScript.
     * Should be used for web views which are placed on the layouts. We store web views between
     * layout changes and have to set ownership of the given object to the view to prevent
     * its re-creation.
     * Supports complex domain-like naming, like "nx.area.management".
     */
    using ObjectFactory = std::function<QObject* (QObject* parent)>;
    void registerApiObjectWithFactory(const QString& name, ObjectFactory factory);

    /** Enable/disable saving web engine profile data on disk. */
    void setOffTheRecord(bool offTheRecord);

    /** When the page requires authentication (like http auth), an authenticator is used. */
    void setAuthenticator(std::shared_ptr<AbstractWebAuthenticator> authenticator);

    /** Execute JavaScript source code. */
    void runJavaScript(const QString& source);

    /** Returns true if there are prior session history entries, false otherwise. */
    bool canGoBack() const;

    /**
     * Add a script to the web page current profile. If the script with the same name already
     * exists, it is replaced with the new one.
     */
    void addProfileScript(const QWebEngineScript& script);

    /** Get a list of actions for context menu. Optionally set a parent for each QAction. */
    QList<QAction*> getContextMenuActions(QObject* parent = nullptr) const;

    /** Suspend current page and return its state as QVariant. */
    QVariant suspend();

    /** Load and run the page from provided state. */
    void resume(QVariant state);

    /**
     * Set external certificate validator.
     * This validator is called only when Chromium engine failed to verify certificate by itself.
     */
    void setCertificateValidator(CertificateValidationFunc validator);

    /**
     * Initializes Client API.
     * @param context Workbench context.
     * @param item Workbench item.
     * @param authCondition Condition which is checked during the authentication using Client API.
     *     May be used with another controller.
     */
    void initClientApiSupport(
        QnWorkbenchContext* context, QnWorkbenchItem* item, ClientApiAuthCondition authCondition);

    /** Register the save state metatype for suspend()/resume() methods. */
    static void registerMetaType();

    // The functions below are intended to be called by WebEngineView QML component.

    /** Show context menu at local coordinates with provided list of web actions. */
    Q_INVOKABLE void showMenu(float x, float y, const QVariant& webActions);

    /**
     * Create new window with web view.
     */
    Q_INVOKABLE QObject* newWindow();

    /**
     * Requests showing an alert, a confirmation, or a prompt dialog from within JavaScript
     * to the user.
     */
    Q_INVOKABLE void requestJavaScriptDialog(QObject* request);

    /**
     * Requests a dialog for providing authentication credentials required by proxies or
     * HTTP servers.
     */
    Q_INVOKABLE void requestAuthenticationDialog(QJSValue request);

    /**
     * Requests a dialog for letting the user choose a (new or existing) file or directory.
     */
    Q_INVOKABLE void requestFileDialog(QObject* request);

    /**
     * Requests a dialog for selecting a color by the user.
     */
    Q_INVOKABLE void requestColorDialog(QObject* request);

    /**
     * Requests a dialog for selecting client certificate.
     */
    Q_INVOKABLE void requestCertificateDialog(QObject* selection);

    /**
     * Request external validator to check provided certificate chain. If no validator has been set,
     * returns false (i.e. only valid public certificates are accepted by the Chromium engine).
     */
    Q_INVOKABLE bool verifyCertificate(const QWebEngineCertificateError& error);

    Q_INVOKABLE QUrl resourceUrl() const;

    Q_INVOKABLE nx::vms::client::desktop::WebPageIconCache* iconCache() const;

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

    /**
     * Get the nearest QWidget which contains the WebEngineView controlled by this class instance.
     */
    QWidget* containerWidget() const;

public slots:
    void onStatusChanged(QQuickWidget::Status status);
    void onLoadingStatusChanged(int status);
    void closeWindows();

signals:
    /* Emitted when QML component is loaded and ready. */
    void rootReady(QQuickItem* root);

    void loadStarted();
    void loadFinished(bool ok);
    void profileChanged(QQuickWebEngineProfile* profile);
    void windowCloseRequested();
    void loadingStatusChanged(int);
    void loadProgressChanged();
    void urlChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
