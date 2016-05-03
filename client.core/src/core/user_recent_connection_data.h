
#pragma once

#include <utils/common/model_functions_fwd.h>

struct QnUserRecentConnectionData
{
    QString systemName;
    QString userName;
    QString password;
    bool isStoredPassword;
};

#define QnUserRecentConnectionData_Fields (systemName)(userName)(password)(isStoredPassword)
QN_FUSION_DECLARE_FUNCTIONS(QnUserRecentConnectionData, (datastream)(metatype)(eq))

typedef QList<QnUserRecentConnectionData> QnUserRecentConnectionDataList;
Q_DECLARE_METATYPE(QnUserRecentConnectionDataList)
