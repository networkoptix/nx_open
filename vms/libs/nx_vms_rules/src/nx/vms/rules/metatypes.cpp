// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metatypes.h"

#include <string>

#include <analytics/common/object_metadata.h>
#include <common/common_meta_types.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/event/level.h>
#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/camera_conflict_list.h>
#include <nx/vms/rules/client_action.h>
#include <nx/vms/rules/icon.h>
#include <nx/vms/rules/network_issue_info.h>
#include <utils/email/message.h>

#include "basic_event.h"
#include "field_types.h"

namespace nx::vms::rules {

// TODO: Consider moving this registration to the API library.
void Metatypes::initialize()
{
    static std::atomic_bool initialized = false;

    if (initialized.exchange(true))
        return;

    QnCommonMetaTypes::initialize();

    // Register types and serializers in alphabetical order.

    qRegisterMetaType<email::Message>();

    qRegisterMetaType<nx::network::http::AuthType>();
    QnJsonSerializer::registerSerializer<nx::network::http::AuthType>();

    QnJsonSerializer::registerSerializer<nx::common::metadata::Attributes>();

    qRegisterMetaType<nx::vms::api::EventLevel>();
    QnJsonSerializer::registerSerializer<nx::vms::api::EventLevel>();

    qRegisterMetaType<nx::vms::api::EventLevels>();
    QnJsonSerializer::registerSerializer<nx::vms::api::EventLevels>();

    QnJsonSerializer::registerSerializer<nx::vms::api::EventReason>();
    QnJsonSerializer::registerSerializer<nx::vms::api::StreamQuality>();

    qRegisterMetaType<nx::vms::api::rules::RuleList>("nx::vms::api::rules::RuleList");

    qRegisterMetaType<nx::vms::api::rules::State>();
    QnJsonSerializer::registerSerializer<State>();

    qRegisterMetaType<nx::vms::event::Level>();
    QnJsonSerializer::registerSerializer<nx::vms::event::Level>();

    qRegisterMetaType<nx::vms::rules::CameraConflictList>();
    QnJsonSerializer::registerSerializer<nx::vms::rules::CameraConflictList>();

    qRegisterMetaType<nx::vms::rules::ClientAction>();
    QnJsonSerializer::registerSerializer<nx::vms::rules::ClientAction>();

    qRegisterMetaType<nx::vms::rules::EventPtr>();
    qRegisterMetaType<std::vector<nx::vms::rules::EventPtr>>();

    qRegisterMetaType<nx::vms::rules::Icon>();
    QnJsonSerializer::registerSerializer<nx::vms::rules::Icon>();

    qRegisterMetaType<nx::vms::rules::NetworkIssueInfo>();
    QnJsonSerializer::registerSerializer<nx::vms::rules::NetworkIssueInfo>();

    qRegisterMetaType<nx::vms::rules::UuidSelection>();
    QnJsonSerializer::registerSerializer<nx::vms::rules::UuidSelection>();

    qRegisterMetaType<QnUuidList>("QnUuidList");
    QnJsonSerializer::registerSerializer<QnUuidList>();

    qRegisterMetaType<QnUuidSet>("QnUuidSet");
    QnJsonSerializer::registerSerializer<QnUuidSet>();

    QnJsonSerializer::registerSerializer<std::chrono::seconds>();
    QnJsonSerializer::registerSerializer<std::chrono::microseconds>();

    qRegisterMetaType<TextLookupCheckType>();
    QnJsonSerializer::registerSerializer<TextLookupCheckType>();

    qRegisterMetaType<ObjectLookupCheckType>();
    QnJsonSerializer::registerSerializer<ObjectLookupCheckType>();

    qRegisterMetaType<nx::vms::rules::AuthenticationInfo>();
    QnJsonSerializer::registerSerializer<nx::vms::rules::AuthenticationInfo>();

    QnJsonSerializer::registerSerializer<std::string>();
    QnJsonSerializer::registerSerializer<nx::network::http::SerializableCredentials>();
};

} // namespace nx::vms::rules
