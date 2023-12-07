// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/serialization/json_fwd.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/uuid.h>

namespace nx::vms::rules {

// TODO: #amalov Select a better place for field types. Consider API library.

/** Common type for UUID selection field. Stores the result of camera/user selection dialog.*/
struct UuidSelection
{
    /** Manually selected ids.*/
    QSet<QnUuid> ids;

    /** Accept/target all flag. */
    bool all = false;

    bool operator==(const UuidSelection&) const = default;
};

#define UuidSelection_Fields (ids)(all)
QN_FUSION_DECLARE_FUNCTIONS(UuidSelection, (json), NX_VMS_RULES_API);

NX_REFLECTION_ENUM_CLASS(TextLookupCheckType,
    containsKeywords,
    doesNotContainKeywords,
    inList,
    notInList);

NX_REFLECTION_ENUM_CLASS(ObjectLookupCheckType,
    hasAttributes,
    inList,
    notInList);

/** Type for storing authentication info. */
struct AuthenticationInfo
{
    nx::network::http::SerializableCredentials credentials;
    nx::network::http::AuthType authType;
    bool operator==(const AuthenticationInfo&) const = default;
};

#define AuthenticationInfo_Fields (credentials)(authType)
QN_FUSION_DECLARE_FUNCTIONS(AuthenticationInfo, (json), NX_VMS_RULES_API);
} // namespace nx::vms::rules


// Adding declaration of fusion functions to avoid adding dependency on nx_vms_rules library to
// nx_network.
namespace nx::network::http {
QN_FUSION_DECLARE_FUNCTIONS(nx::network::http::SerializableCredentials, (json), NX_VMS_RULES_API);
} // namespace nx::network::http
