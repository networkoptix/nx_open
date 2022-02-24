// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/dialog.h>

class QnResourcePool;

namespace nx::vms::client::desktop {

class ResourcePoolDialog: public QnDialog
{
    using base_type = QnDialog;
    Q_OBJECT

public:
    ResourcePoolDialog(QnResourcePool* resourcePool, QWidget* parent = nullptr);

    static void registerAction();
};

} // namespace nx::vms::client::desktop
