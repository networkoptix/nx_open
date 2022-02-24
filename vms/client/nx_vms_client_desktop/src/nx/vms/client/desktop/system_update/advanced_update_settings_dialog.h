// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>

#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class AdvancedUpdateSettingsDialog: public QmlDialogWrapper, public QnWorkbenchContextAware
{
public:
    AdvancedUpdateSettingsDialog(QWidget* parent = nullptr);

    const QmlProperty<bool> advancedMode{rootObjectHolder(), "advancedMode"};
};

} // namespace nx::vms::client::desktop
