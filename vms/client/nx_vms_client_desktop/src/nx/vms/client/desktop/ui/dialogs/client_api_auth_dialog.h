// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrl>

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/message_box.h>

namespace nx::vms::client::desktop {

class ClientApiAuthDialog: public QnMessageBox
{
    Q_OBJECT
public:
    ClientApiAuthDialog(const QnResourcePtr& resource, QWidget* parent = nullptr);
    static bool isApproved(const QnResourcePtr& resource, const QUrl& url);

private:
    QPushButton* m_allowButton = nullptr;
};

} // nx::vms::client::desktop
