// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_data.h"

#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCloudSystem, (json), QnCloudSystem_Fields, (brief, true))

bool QnCloudSystem::operator==(const QnCloudSystem &other) const
{
    return ((cloudId == other.cloudId)
        && (localId == other.localId)
        && (name == other.name)
        && (authKey == other.authKey)
        && (lastLoginTimeUtcMs == other.lastLoginTimeUtcMs)
        && qFuzzyEquals(weight, other.weight)
        && online == other.online
        && system2faEnabled == other.system2faEnabled);
}

bool QnCloudSystem::visuallyEqual(const QnCloudSystem& other) const
{
    return (cloudId == other.cloudId
        && (localId == other.localId)
        && (name == other.name)
        && (authKey == other.authKey)
        && (ownerAccountEmail == other.ownerAccountEmail)
        && (ownerFullName == other.ownerFullName));
}
