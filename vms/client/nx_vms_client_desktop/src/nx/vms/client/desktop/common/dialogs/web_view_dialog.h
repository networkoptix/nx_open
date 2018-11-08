#pragma once

#include <QtCore/QUrl>

#include <ui/dialogs/common/button_box_dialog.h>

namespace nx::vms::client::desktop {

class WebWidget;

class WebViewDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit WebViewDialog(const QUrl& url, QWidget* parent = nullptr);
    virtual ~WebViewDialog() override = default;

    static void showUrl(const QUrl& url, QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop
