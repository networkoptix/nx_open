#pragma once

#include <functional>

#include <QtWebKitWidgets/QGraphicsWebView>

#include <ui/graphics/items/standard/graphics_qml_view.h>

enum WebViewPageStatus
{
    kPageInitialLoadInProgress
    , kPageLoading
    , kPageLoaded
    , kPageLoadFailed
};

namespace nx::vms::client::desktop {

class GraphicsWebEngineView : public GraphicsQmlView
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

    static void registerObject(QQuickItem* webView, const QString& name, QObject* object);

public slots:
    void back();

signals:
    void loadStarted();

    void statusChanged();

    void canGoBackChanged();

private slots:
    void viewUrlChanged();

    void setViewStatus(int status);

private:
    WebViewPageStatus m_status;
    bool m_canGoBack;
    RootReadyCallback m_rootReadyCallback;
};

} // namespace nx::vms::client::desktop

class QnGraphicsWebView : public QGraphicsWebView
{
    Q_OBJECT

    Q_PROPERTY(WebViewPageStatus status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack WRITE setCanGoBack NOTIFY canGoBackChanged)
public:
    QnGraphicsWebView(const QUrl &url = QUrl(), QGraphicsItem *parent = nullptr);

    virtual ~QnGraphicsWebView();

    WebViewPageStatus status() const;

    void setStatus(WebViewPageStatus value);

    bool canGoBack() const;

    void setCanGoBack(bool value);

    void setPageUrl(const QUrl &newUrl);

signals:
    void statusChanged();

    void canGoBackChanged();

private:
    WebViewPageStatus m_status;
    bool m_canGoBack;
};
