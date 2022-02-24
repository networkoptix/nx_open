// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>

#include <ui/workbench/workbench_context_aware.h>

class QScreen;

namespace nx::vms::client::desktop {

class AdvancedSearchDialog:
    public QmlDialogWrapper,
    public QnWorkbenchContextAware
{
public:
    AdvancedSearchDialog(QWidget* parent = nullptr);

    static void registerStateDelegate();
    static void unregisterStateDelegate();

    class StateDelegate;

private:
    QScreen* defaultScreen() const;
};

} // namespace nx::vms::client::desktop
