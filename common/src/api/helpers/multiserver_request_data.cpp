#include "multiserver_request_data.h"


namespace {
    const QString localKey(lit("local"));
}

QnMultiserverRequestData::QnMultiserverRequestData()
    : isLocal(false)
{}

void QnMultiserverRequestData::loadFromParams(const QnRequestParamList& params) {
    isLocal = params.contains(localKey);
}

QnRequestParamList QnMultiserverRequestData::toParams() const {
    QnRequestParamList result;
    if (isLocal)
        result.insert(localKey, QString());
    return result;
}

QUrlQuery QnMultiserverRequestData::toUrlQuery() const {
    QUrlQuery urlQuery;
    for(const auto& param: toParams())
        urlQuery.addQueryItem(param.first, param.second);
    return urlQuery;
}

bool QnMultiserverRequestData::isValid() const {
    return true;
}
