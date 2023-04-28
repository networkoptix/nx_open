// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/system_context_aware.h>
#include <nx/vms/utils/abstract_session_token_helper.h>

namespace nx::vms::api { struct SystemSettings; }

namespace nx::vms::client::core {

/** Class for storing, saving and synchronizing system settings */
class NX_VMS_CLIENT_CORE_API SystemSettingsManager:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    SystemSettingsManager(SystemContext* context, QObject* parent = nullptr);
    ~SystemSettingsManager();

    void requestSystemSettings(std::function<void()> callback = {});
    nx::vms::api::SystemSettings* systemSettings();

    void saveSystemSettings(std::function<void(bool)> callback = {},
        const nx::vms::common::SessionTokenHelperPtr& helper = nullptr);

signals:
    void systemSettingsChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
