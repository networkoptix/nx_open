// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/dialog.h>

namespace nx::vms::client::desktop {

class AnimationsControlDialog: public QnDialog
{
    using base_type = QnDialog;
public:
    AnimationsControlDialog(QWidget* parent);

    static void registerAction();
};

} // namespace nx::vms::client::desktop
