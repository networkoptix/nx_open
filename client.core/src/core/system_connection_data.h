
#pragma once

#include <utils/common/model_functions_fwd.h>

struct QnSystemConnectionData
{
    QString systemName;
    QString userName;
    QString password;
};

#define QnSystemConnectionData_Fields (systemName)(userName)(password)
QN_FUSION_DECLARE_FUNCTIONS(QnSystemConnectionData, (datastream)(metatype))

typedef QList<QnSystemConnectionData> QnSystemConnectionDataList;
Q_DECLARE_METATYPE(QnSystemConnectionDataList)
