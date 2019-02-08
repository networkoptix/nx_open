
#include "cloud_system_data.h"

#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnCloudSystem), (json), _Fields, (brief, true))

bool QnCloudSystem::operator==(const QnCloudSystem &other) const
{
    return ((cloudId == other.cloudId)
        && (localId == other.localId)
        && (name == other.name)
        && (authKey == other.authKey)
        && (lastLoginTimeUtcMs == other.lastLoginTimeUtcMs)
        && qFuzzyEquals(weight, other.weight)
        && online == other.online);
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

