#pragma once

#include <vector>
#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

struct ApiServerHardwareIdsData
{
    QnUuid serverId;
    QStringList hardwareIds;
};
#define ApiServerHardwareIdsData_Fields (serverId)(hardwareIds)

using ApiServerHardwareIdsDataList = std::vector<ApiServerHardwareIdsData>;

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (ApiServerHardwareIdsData), (json)(ubjson)(xml)(csv_record)(metatype))
