#pragma once

#include <functional>

#include <ui/graphics/items/standard/graphics_qml_view.h>

enum WebViewPageStatus
{
    kPageInitialLoadInProgress
    , kPageLoading
    , kPageLoaded
    , kPageLoadFailed
};

namespace nx::vms::client::desktop {

class GraphicsWebEngineView: public GraphicsQmlView
{
    Q_OBJECT

    Q_PROPERTY(WebViewPageStatus status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack WRITE setCanGoBack NOTIFY canGoBackChanged)

public:
    static const QUrl kQmlSourceUrl;

    typedef std::function<void(void)> RootReadyCallback;

    GraphicsWebEngineView(const QUrl& url = QUrl(), QGraphicsItem* parent = nullptr);

    virtual ~GraphicsWebEngineView();

    WebViewPageStatus status() const;

    void setStatus(WebViewPageStatus value);

    bool canGoBack() const;

    void setCanGoBack(bool value);

    void setPageUrl(const QUrl &newUrl);

    void setUrl(const QUrl &url);

    QUrl url() const;

    void addToJavaScriptWindowObject(const QString& name, QObject* object);

    void whenRootReady(RootReadyCallback callback);

    static void registerObject(
        QQuickItem* webView, const QString& name, QObject* object, bool enablePromises = false);

    static void requestJavaScriptDialog(QObject* request, QWidget* parent);

    static void requestAuthenticationDialog(QObject* request, QWidget* parent);

    static void requestFileDialog(QObject* request, QWidget* parent);

    static void requestColorDialog(QObject* request, QWidget* parent);

public slots:
    void back();

signals:
    void loadStarted();

    void statusChanged();

    void canGoBackChanged();

    void loadProgress(int progress);

    void loadFinished(bool ok);

private slots:
    void viewUrlChanged();

    void setViewStatus(int status);

    void onLoadProgressChanged();

private:
    WebViewPageStatus m_status = kPageInitialLoadInProgress;
    bool m_canGoBack = false;
    RootReadyCallback m_rootReadyCallback;
};

} // namespace nx::vms::client::desktop
