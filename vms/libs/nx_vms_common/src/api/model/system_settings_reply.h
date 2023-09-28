// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>

class QnSystemSettingsReply
{
public:
    //map<name, value>
    QHash<QString, QString> settings;
};
#define QnSystemSettingsReply_Fields (settings)

QN_FUSION_DECLARE_FUNCTIONS(QnSystemSettingsReply, (json), NX_VMS_COMMON_API)
