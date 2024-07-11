// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/server_rest_connection_fwd.h>
#include <core/resource/user_resource.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/vms/api/data/user_model.h>

namespace nx::vms::common {

class AbstractSessionTokenHelper;
using SessionTokenHelperPtr = std::shared_ptr<AbstractSessionTokenHelper>;

} // namespace nx::vms::common

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API UserResource: public QnUserResource
{
    Q_OBJECT
    using base_type = QnUserResource;

public:
    using QnUserResource::QnUserResource;

    /** Special constructor for compatibility mode. */
    explicit UserResource(nx::vms::api::UserModelV1 data);

    /** Constructor for cross-system contexts as of 6.0 and newer. */
    explicit UserResource(nx::vms::api::UserModelV3 data);

    /** Send request to the currently connected server to save user settings. */
    rest::Handle saveSettings(
        const UserSettings& value,
        common::SessionTokenHelperPtr sessionTokenHelper,
        std::function<void(bool /*success*/, rest::Handle /*handle*/)> callback = {},
        nx::utils::AsyncHandlerExecutor executor = {});

protected:
    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;

private:
    std::optional<nx::vms::api::UserModelV1> m_overwrittenData;
};

using UserResourcePtr = QnSharedResourcePointer<UserResource>;

} // namespace nx::vms::client::core
