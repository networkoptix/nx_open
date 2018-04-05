#pragma once

#include <QtCore/QUrl>

#include <ui/dialogs/common/button_box_dialog.h>

class QWebView;
class QLabel;

namespace nx {
namespace client {
namespace desktop {

class BusyIndicatorWidget;

class WebViewDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit WebViewDialog(QWidget* parent = nullptr);
    explicit WebViewDialog(const QString& url, QWidget* parent = nullptr);
    explicit WebViewDialog(const QUrl& url, QWidget* parent = nullptr);
    virtual ~WebViewDialog() override = default;

    static void showUrl(const QString& url, QWidget* parent = nullptr);
    static void showUrl(const QUrl& url, QWidget* parent = nullptr);

private:
    QWebView* const m_webView = nullptr;
    BusyIndicatorWidget* const m_indicator = nullptr;
    QLabel* const m_errorLabel = nullptr;
};

} // namespace desktop
} // namespace client
} // namespace nx