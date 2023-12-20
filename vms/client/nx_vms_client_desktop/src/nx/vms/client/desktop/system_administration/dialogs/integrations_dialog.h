// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/vms/client/desktop/common/dialogs/qml_dialog_with_state.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <nx/vms/client/desktop/current_system_context_aware.h>
#include <nx/vms/client/desktop/system_administration/models/analytics_settings_store.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API IntegrationsDialog:
    public QmlDialogWrapper,
    public CurrentSystemContextAware
{
    Q_OBJECT

public:
    IntegrationsDialog(QWidget* parent = nullptr);

private:
    std::unique_ptr<AnalyticsSettingsStore> store;
};

} // namespace nx::vms::client::desktop
