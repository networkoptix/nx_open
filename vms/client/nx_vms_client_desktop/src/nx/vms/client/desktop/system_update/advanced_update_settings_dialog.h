// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/server_rest_connection_fwd.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <ui/workbench/workbench_state_manager.h>

namespace nx::vms::client::desktop {

class AdvancedUpdateSettingsDialog: public QmlDialogWrapper, public QnSessionAwareDelegate
{
public:
    AdvancedUpdateSettingsDialog(QWidget* parent = nullptr);

    const QmlProperty<bool> advancedMode{rootObjectHolder(), "advancedMode"};
    const QmlProperty<bool> notifyAboutUpdates{rootObjectHolder(), "notifyAboutUpdates"};
    const QmlProperty<bool> offlineUpdatesEnabled{rootObjectHolder(), "offlineUpdatesEnabled"};

    virtual bool tryClose(bool force) override;

private:
    void loadSettings();
    void saveSettings();

private:
    rest::Handle m_currentRequest = 0;
};

} // namespace nx::vms::client::desktop
