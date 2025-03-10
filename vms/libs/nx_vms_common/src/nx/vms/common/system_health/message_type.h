// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <set>
#include <vector>

#include <nx/vms/api/data/site_health_message.h>

namespace nx::vms::common { class SystemContext; }

// TODO: #sivanov refactor settings storage - move to User Settings tab on server.
namespace nx::vms::common::system_health {

using MessageType = nx::vms::api::SiteHealthMessageType;

using MessageTypePredicate = std::function<bool(MessageType)>;
using MessageTypePredicateList = std::vector<MessageTypePredicate>;

/** Some messages are not to be displayed in any case. */
NX_VMS_COMMON_API bool isMessageVisible(MessageType message);

/** Some messages must not be auto-hidden by timeout. */
NX_VMS_COMMON_API bool isMessageLocked(MessageType message);

/** Some messages must not be displayed in settings dialog, so user cannot disable them. */
NX_VMS_COMMON_API bool isMessageVisibleInSettings(MessageType message);

/** Intercom related message, may be show to any user with camera access rights. */
NX_VMS_COMMON_API bool isMessageIntercom(MessageType message);

/**
 * @return Predicate that returns true for system health message types that can be displayed
 *     under the terms of currently applied licensing model. The system is described by the given
 *     system context.
 */
NX_VMS_COMMON_API MessageTypePredicate isMessageApplicableForLicensingMode(
    nx::vms::common::SystemContext* systemContext);

/**
 *  @return Set of system health message types for which each of the passed predicates returns
 *      true or set that contains all message types if no predicate passed as a parameter.
 */
NX_VMS_COMMON_API std::set<MessageType> allMessageTypes(
    const MessageTypePredicateList& predicates);

} // namespace nx::vms::common::system_health
