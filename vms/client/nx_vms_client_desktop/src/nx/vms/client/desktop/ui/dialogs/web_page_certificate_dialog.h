// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrl>

#include <ui/dialogs/common/session_aware_dialog.h>

namespace nx::vms::client::desktop {

class WebPageCertificateDialog: public QnSessionAwareMessageBox
{
    Q_OBJECT

public:
    WebPageCertificateDialog(QWidget* parent, const QUrl& url, bool isIntegration = false);
};

} // namespace nx::vms::client::desktop
