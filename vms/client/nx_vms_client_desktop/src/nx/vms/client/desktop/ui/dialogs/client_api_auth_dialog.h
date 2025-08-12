// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrl>

#include <ui/dialogs/common/message_box.h>

namespace nx::vms::client::desktop {

class ClientApiAuthDialog: public QnMessageBox
{
    Q_OBJECT
public:
    ClientApiAuthDialog(const QString& origin, QWidget* parent = nullptr);
    static bool isApproved(const QUrl& url);

private:
    QPushButton* m_allowButton = nullptr;
};

} // nx::vms::client::desktop
