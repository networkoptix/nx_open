#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>

#include "cloud_credentials_data.h"

struct SetupCloudSystemData: public CloudCredentialsData
{
    SetupCloudSystemData();
    SetupCloudSystemData(const QnRequestParams& params);

    QString systemName;
    QHash<QString, QString> systemSettings;
};

#define SetupCloudSystemData_Fields CloudCredentialsData_Fields (systemName)(systemSettings)

QN_FUSION_DECLARE_FUNCTIONS(SetupCloudSystemData, (json));
