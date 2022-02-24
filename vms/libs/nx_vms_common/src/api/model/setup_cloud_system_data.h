// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>

#include "cloud_credentials_data.h"

struct SetupCloudSystemData: public CloudCredentialsData
{
    QString systemName;
    QHash<QString, QString> systemSettings;
};

#define SetupCloudSystemData_Fields CloudCredentialsData_Fields (systemName)(systemSettings)

QN_FUSION_DECLARE_FUNCTIONS(SetupCloudSystemData, (json), NX_VMS_COMMON_API);
