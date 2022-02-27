// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>

struct CloudCredentialsData
{
    // TODO: rename it to cloudSystemId
    QString cloudSystemID; //< keep same name as the constant QnGlobalSettings::kNameCloudSystemID
    QString cloudAuthKey; //< keep same name as the constant QnGlobalSettings::kNameCloudAuthKey
    QString cloudAccountName; //< keep same name as the constant QnGlobalSetttings::kNameCloudAccountName
};

#define CloudCredentialsData_Fields (cloudSystemID)(cloudAuthKey)(cloudAccountName)

QN_FUSION_DECLARE_FUNCTIONS(CloudCredentialsData, (json), NX_VMS_COMMON_API)
