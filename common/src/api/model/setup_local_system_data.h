#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>

#include "password_data.h"

struct SetupLocalSystemData: public PasswordData
{
    SetupLocalSystemData();
    SetupLocalSystemData(const QnRequestParams& params);

    QString systemName;
    QHash<QString, QString> systemSettings;
};

#define SetupLocalSystemData_Fields PasswordData_Fields (systemName)(systemSettings)

QN_FUSION_DECLARE_FUNCTIONS(SetupLocalSystemData, (json));
