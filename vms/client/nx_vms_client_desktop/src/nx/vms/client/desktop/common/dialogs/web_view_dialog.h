// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrl>

#include <ui/dialogs/common/button_box_dialog.h>

class QnWorkbenchContext;

namespace nx::vms::client::desktop {

class WebViewDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit WebViewDialog(
        const QUrl& url,
        bool enableClientApi = false,
        QnWorkbenchContext* context = nullptr,
        QWidget* parent = nullptr);

    virtual ~WebViewDialog() override = default;

    static void showUrl(const QUrl& url,
        bool enableClientApi = false,
        QnWorkbenchContext* context = nullptr,
        QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop
