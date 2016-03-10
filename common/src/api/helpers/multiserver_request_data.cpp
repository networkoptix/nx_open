#include "multiserver_request_data.h"

#include <utils/common/model_functions.h>

namespace
{
    const QString localKey                  (lit("local"));
    const QString formatKey                 (lit("format"));
    const QString extraFormattingKey        (lit("extraFormatting"));

    Qn::SerializationFormat defaultFormat()
    {
#ifdef _DEBUG
        return Qn::JsonFormat;
#else
        return Qn::UbjsonFormat;
#endif
    }
}

QnMultiserverRequestData::QnMultiserverRequestData()
    : isLocal(false)
    , format(defaultFormat())
    , extraFormatting(false)
{}

QnMultiserverRequestData::QnMultiserverRequestData( const QnMultiserverRequestData &src )
    : isLocal(src.isLocal)
    , format(src.format)
    , extraFormatting(src.extraFormatting)
{}

void QnMultiserverRequestData::loadFromParams(const QnRequestParamList& params) {
    isLocal = params.contains(localKey);
    extraFormatting = params.contains(extraFormattingKey);
    QnLexical::deserialize(params.value(formatKey), &format);
}

QnRequestParamList QnMultiserverRequestData::toParams() const {
    QnRequestParamList result;
    if (isLocal)
        result.insert(localKey, QString());
    if (extraFormatting)
        result.insert(extraFormattingKey, QString());
    result.insert(formatKey,    QnLexical::serialized(format));
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

void QnMultiserverRequestData::makeLocal(Qn::SerializationFormat localFormat) {
    isLocal = true;
    format = localFormat;
}
