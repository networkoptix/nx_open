// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <api/model/password_data.h>
#include <nx/fusion/model_functions_fwd.h>

struct NX_VMS_COMMON_API MergeSystemData: public CurrentPasswordData
{
    /**
     * Remote Site url.
     */
    QString url;
    QString getKey;
    QString postKey;
    bool takeRemoteSettings = false;
    bool mergeOneServer = false;
    bool ignoreIncompatible = false;
    bool dryRun = false;
};

#define MergeSystemData_Fields CurrentPasswordData_Fields \
    (url)(getKey)(postKey)(takeRemoteSettings)(mergeOneServer)(ignoreIncompatible)(dryRun)

QN_FUSION_DECLARE_FUNCTIONS(MergeSystemData, (json), NX_VMS_COMMON_API)
