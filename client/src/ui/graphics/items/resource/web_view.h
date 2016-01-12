
#pragma once

#include <QtWebKitWidgets/QGraphicsWebView>

enum WebViewPageStatus
{
    kPageLoadFailed
    , kPageInitialLoadInProgress
    , kPageLoading
    , kPageLoaded
};

class QnWebView : public QGraphicsWebView
{
    Q_OBJECT

    Q_PROPERTY(WebViewPageStatus status READ status WRITE setStatus NOTIFY statusChanged)

public:
    QnWebView(const QUrl &url = QUrl()
        , QGraphicsItem *parent = nullptr);

    virtual ~QnWebView();

    WebViewPageStatus status() const;

    void setStatus(WebViewPageStatus value);

public:
    void loadPage(const QUrl &url);

    void reloadPage();

signals:
    void statusChanged();

private:
    void updatePageLoadProgress(int progress);

private:
    WebViewPageStatus m_status;
};
