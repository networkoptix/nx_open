// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/dialog.h>

namespace Ui { class ApplauncherControlDialog; }

namespace nx::vms::client::desktop {

class ApplauncherControlDialog: public QnDialog
{
    using base_type = QnDialog;
public:
    ApplauncherControlDialog(QWidget* parent = nullptr);

    static void registerAction();
private:
    QScopedPointer<Ui::ApplauncherControlDialog> ui;
};

} // namespace nx::vms::client::desktop
