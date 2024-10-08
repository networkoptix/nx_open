// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/server_rest_connection_fwd.h>
#include <core/resource/user_resource.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>

namespace nx::vms::common {

class AbstractSessionTokenHelper;
using SessionTokenHelperPtr = std::shared_ptr<AbstractSessionTokenHelper>;

} // namespace nx::vms::common

namespace nx::vms::client::core {

struct UserSettings: public common::UserSettingsEx
{
    EventSearch::CameraSelection cameraSelection = EventSearch::CameraSelection::layout;
    EventSearch::TimeSelection timeSelection = EventSearch::TimeSelection::anytime;
    QStringList objectTypeIds;
};
#define CoreUserSettings_Fields UserSettings_Fields(cameraSelection)(timeSelection)(objectTypeIds)
NX_REFLECTION_INSTRUMENT(UserSettings, CoreUserSettings_Fields)

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

        void setSettings(const UserSettings& settings);
        UserSettings settings() const;

protected:
    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;

private:
    std::optional<nx::vms::api::UserModelV1> m_overwrittenData;
};

using UserResourcePtr = QnSharedResourcePointer<UserResource>;

} // namespace nx::vms::client::core
