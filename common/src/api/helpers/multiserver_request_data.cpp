#include "multiserver_request_data.h"

#include <nx/fusion/model_functions.h>

namespace {

static const QString kLocalParam(lit("local"));
static const QString kFormatParam(lit("format"));
static const QString kExtraFormattingParam(lit("extraFormatting"));

static Qn::SerializationFormat defaultFormat()
{
    #if defined(_DEBUG_PROTOCOL)
        return Qn::JsonFormat;
    #else
        return Qn::UbjsonFormat;
    #endif
}

} // namespace

QnMultiserverRequestData::QnMultiserverRequestData():
    isLocal(false),
    format(defaultFormat()),
    extraFormatting(false)
{
}

QnMultiserverRequestData::QnMultiserverRequestData(const QnRequestParamList& params):
    QnMultiserverRequestData()
{
    loadFromParams(params);
}

QnMultiserverRequestData::QnMultiserverRequestData(const QnMultiserverRequestData& src):
    isLocal(src.isLocal),
    format(src.format),
    extraFormatting(src.extraFormatting)
{
}

void QnMultiserverRequestData::loadFromParams(const QnRequestParamList& params)
{
    isLocal = params.contains(kLocalParam);
    extraFormatting = params.contains(kExtraFormattingParam);
    QnLexical::deserialize(params.value(kFormatParam), &format);
}

void QnMultiserverRequestData::loadFromParams(const QnRequestParams& params)
{
    isLocal = params.contains(kLocalParam);
    extraFormatting = params.contains(kExtraFormattingParam);
    QnLexical::deserialize(params.value(kFormatParam), &format);
}

QnRequestParamList QnMultiserverRequestData::toParams() const
{
    QnRequestParamList result;
    if (isLocal)
        result.insert(kLocalParam, QString());
    if (extraFormatting)
        result.insert(kExtraFormattingParam, QString());
    result.insert(kFormatParam, QnLexical::serialized(format));
    return result;
}

QUrlQuery QnMultiserverRequestData::toUrlQuery() const
{
    QUrlQuery urlQuery;
    for (const auto& param: toParams())
        urlQuery.addQueryItem(param.first, param.second);
    return urlQuery;
}

bool QnMultiserverRequestData::isValid() const
{
    return true;
}

void QnMultiserverRequestData::makeLocal(Qn::SerializationFormat localFormat)
{
    isLocal = true;
    format = localFormat;
}
