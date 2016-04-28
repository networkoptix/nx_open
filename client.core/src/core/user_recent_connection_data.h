
#pragma once

#include <utils/common/model_functions_fwd.h>

struct QnUserRecentConnectionData
{
    QString systemName;
    QString userName;
    QString password;
};

#define QnUserRecentConnectionData_Fields (systemName)(userName)(password)
QN_FUSION_DECLARE_FUNCTIONS(QnUserRecentConnectionData, (datastream)(metatype))

typedef QList<QnUserRecentConnectionData> QnUserRecentConnectionDataList;
Q_DECLARE_METATYPE(QnUserRecentConnectionDataList)
