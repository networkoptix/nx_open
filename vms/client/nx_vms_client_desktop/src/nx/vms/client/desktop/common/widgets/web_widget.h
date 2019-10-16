#pragma once

#include <QtWidgets/QWidget>

class QUrl;
class QWebView;
class QWebEngineView;
class QNetworkRequest;

namespace nx::vms::client::desktop {

class WebWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    WebWidget(QWidget* parent = nullptr, bool useActionsForLinks = false);

    QWebView* view() const;
    QWebEngineView* webEngineView() const;

    void load(const QUrl& url);
    void load(const QNetworkRequest& request);
    void reset();

private:
    QWebView* m_webView;
    QWebEngineView* m_webEngineView;
};

} // namespace nx::vms::client::desktop
