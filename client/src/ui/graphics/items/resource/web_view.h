#pragma once

#include <QtWebKitWidgets/QGraphicsWebView>

enum WebViewPageStatus
{
    kPageInitialLoadInProgress
    , kPageLoading
    , kPageLoaded
    , kPageLoadFailed
};

class QnWebView : public QGraphicsWebView
{
    Q_OBJECT

    Q_PROPERTY(WebViewPageStatus status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack WRITE setCanGoBack NOTIFY canGoBackChanged)
public:
    QnWebView(const QUrl &url = QUrl()
        , QGraphicsItem *parent = nullptr);

    virtual ~QnWebView();

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
