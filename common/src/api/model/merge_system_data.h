#pragma once

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>

#include <utils/common/request_param.h>

struct MergeSystemData
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

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((MergeSystemData), (json))
