#pragma once

#include <vector>

#include<utils/common/model_functions_fwd.h>

struct QnHttpConfigureRequest
{
    QString templateString;
    QString method;
    bool isAllowedToFail;
    QString body;
};

typedef std::vector<QnHttpConfigureRequest> QnHttpConfigureRequestList;

#define QnHttpConfigureRequest_Fields (templateString)(method)(isAllowedToFail)(body)
QN_FUSION_DECLARE_FUNCTIONS(QnHttpConfigureRequest, (json)(ubjson)(xml)(csv_record))
Q_DECLARE_METATYPE(QnHttpConfigureRequest)

typedef std::vector<quint64> QnBitrateList;

struct QnBounds
{
    //TODO: #dmishin move to signed integer type and refactor isNull method
    quint64 min;
    quint64 max;

    QnBounds() : min(0), max(0) {};
    bool isNull() const { return min == 0 && max == 0; };
};

#define QnBounds_Fields (min)(max)
QN_FUSION_DECLARE_FUNCTIONS(QnBounds, (json)(ubjson)(xml)(csv_record))
Q_DECLARE_METATYPE(QnBounds)

typedef std::vector<QnBounds> QnBoundsList;
