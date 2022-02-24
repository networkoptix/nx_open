// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/dialog.h>

namespace nx::vms::client::desktop {

class PaletteDialog: public QnDialog
{
    using base_type = QnDialog;
    Q_OBJECT

public:
    PaletteDialog(QWidget* parent = nullptr);

    static void registerAction();
};

} // namespace nx::vms::client::desktop
