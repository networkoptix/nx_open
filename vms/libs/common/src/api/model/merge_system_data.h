#pragma once

#include <QtCore/QString>

#include <api/model/password_data.h>
#include <nx/fusion/model_functions_fwd.h>

struct MergeSystemData: public CurrentPasswordData
{
    MergeSystemData() = default;
    MergeSystemData(const QnRequestParams& params);

    /**
     * Remote system url.
     */
    QString url;
    QString getKey;
    QString postKey;
    bool takeRemoteSettings = false;
    bool mergeOneServer = false;
    bool ignoreIncompatible = false;
};

#define MergeSystemData_Fields CurrentPasswordData_Fields \
    (url)(getKey)(postKey)(takeRemoteSettings)(mergeOneServer)(ignoreIncompatible)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((MergeSystemData), (json))
