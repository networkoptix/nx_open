// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/dialog.h>

namespace nx::vms::client::desktop {

class WebViewWidget;

class HelpDialog: public QnDialog
{
    using base_type = QnDialog;

public:
    HelpDialog(QWidget* parent = nullptr);

    void setUrl(const QString& url);

private:
    WebViewWidget* const m_webViewWidget;
};

} // namespace nx::vms::client::desktop
