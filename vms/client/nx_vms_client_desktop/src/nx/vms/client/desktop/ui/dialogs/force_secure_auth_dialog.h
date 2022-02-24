// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>

namespace nx::vms::client::desktop {

class ForceSecureAuthDialog: public QnSessionAwareMessageBox
{
    Q_OBJECT
    using base_type = QnSessionAwareMessageBox;

public:
    ForceSecureAuthDialog(QWidget* parent);
    static bool isConfirmed(QWidget* parent);
};

} // nx::vms::client::desktop
