// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/dialog.h>

class QLineEdit;

namespace nx::vms::client::desktop {

class WebViewWidget;

class WebEngineDialog: public QnDialog
{
    using base_type = QnDialog;
    Q_OBJECT

public:
    WebEngineDialog(QWidget* parent = nullptr);

    static void registerAction();

    /** Test integration with web pages. */
    Q_INVOKABLE void handleJsonObject(const QString& json);

private:
    WebViewWidget* const m_webViewWidget;
    QLineEdit* const m_urlLineEdit;
};

} // namespace nx::vms::client::desktop
