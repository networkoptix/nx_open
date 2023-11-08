// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_data.h"

#include <nx/reflect/compare.h>
#include <nx/utils/math/fuzzy.h>

bool QnCloudSystem::operator==(const QnCloudSystem &other) const
{
    return nx::reflect::equals(*this, other);
}

bool QnCloudSystem::visuallyEqual(const QnCloudSystem& other) const
{
    return cloudId == other.cloudId
        && localId == other.localId
        && name == other.name
        && authKey == other.authKey
        && ownerAccountEmail == other.ownerAccountEmail
        && ownerFullName == other.ownerFullName
        && organizationId == other.organizationId;
}
