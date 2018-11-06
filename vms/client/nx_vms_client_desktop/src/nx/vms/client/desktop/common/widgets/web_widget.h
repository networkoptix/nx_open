#pragma once

#include <QtWidgets/QWidget>

class QUrl;
class QWebView;
class QNetworkRequest;

namespace nx::vms::client::desktop {

class WebWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    WebWidget(QWidget* parent = nullptr);

    QWebView* view() const;

    void load(const QUrl& url);
    void load(const QNetworkRequest& request);
    void reset();

private:
    QWebView* const m_webView;
};

} // namespace nx::vms::client::desktop
