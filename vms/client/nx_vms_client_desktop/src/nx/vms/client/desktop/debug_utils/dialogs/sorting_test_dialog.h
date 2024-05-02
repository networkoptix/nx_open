// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>

namespace nx::vms::client::desktop {

class SortingTestDialog: public QmlDialogWrapper
{
    Q_OBJECT
    using base_type = QmlDialogWrapper;

public:
    SortingTestDialog(QWidget* parent = nullptr);

    static void registerAction();
};

} // namespace nx::vms::client::desktop
