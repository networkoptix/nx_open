
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

public:
    QnWebView(const QUrl &url = QUrl()
        , QGraphicsItem *parent = nullptr);

    virtual ~QnWebView();

    WebViewPageStatus status() const;

    void setStatus(WebViewPageStatus value);

signals:
    void statusChanged();

private:
    void updatePageLoadProgress(int progress);

private:
    WebViewPageStatus m_status;
};
