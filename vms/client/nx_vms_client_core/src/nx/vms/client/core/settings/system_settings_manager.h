// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <api/server_rest_connection_fwd.h>
#include <nx/utils/async_handler_executor.h>
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

    using RequestCallback = std::function<void(bool /*success*/, rest::Handle /*requestId*/)>;

    /** Request System Settings. Callback will be called in the manager's thread. */
    rest::Handle requestSystemSettings(RequestCallback callback);

    nx::vms::api::SystemSettings* systemSettings();

    rest::Handle saveSystemSettings(RequestCallback callback,
        nx::utils::AsyncHandlerExecutor executor,
        const nx::vms::common::SessionTokenHelperPtr& helper = nullptr);

signals:
    void systemSettingsChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
