// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>

#include "password_data.h"

struct SetupLocalSystemData: public PasswordData
{
    QString systemName;
    QHash<QString, QString> systemSettings;
};

#define SetupLocalSystemData_Fields PasswordData_Fields (systemName)(systemSettings)

QN_FUSION_DECLARE_FUNCTIONS(SetupLocalSystemData, (json), NX_VMS_COMMON_API);
