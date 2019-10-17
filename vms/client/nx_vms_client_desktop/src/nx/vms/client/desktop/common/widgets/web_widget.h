#pragma once

#include <QtWidgets/QWidget>

class QUrl;
class QWebView;
class QNetworkRequest;

namespace nx::vms::client::desktop {

class WebEngineView;

class WebWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    WebWidget(QWidget* parent = nullptr, bool useActionsForLinks = false);

    QWebView* view() const;
    WebEngineView* webEngineView() const;

    void load(const QUrl& url);
    void load(const QNetworkRequest& request);
    void reset();

private:
    QWebView* m_webView;
    WebEngineView* m_webEngineView;
};

} // namespace nx::vms::client::desktop
