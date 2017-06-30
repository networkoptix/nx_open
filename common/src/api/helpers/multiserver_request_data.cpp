#include "multiserver_request_data.h"

#include <nx/fusion/model_functions.h>

namespace {

static const QString kLocalParam(lit("local"));
static const QString kFormatParam(lit("format"));
static const QString kExtraFormattingParam(lit("extraFormatting"));

static const Qn::SerializationFormat kDefaultFormat = Qn::SerializationFormat::JsonFormat;

} // namespace

QnMultiserverRequestData::QnMultiserverRequestData():
    isLocal(false),
    format(kDefaultFormat),
    extraFormatting(false)
{
}

QnMultiserverRequestData::QnMultiserverRequestData(
    QnResourcePool* resourcePool,
    const QnRequestParamList& params)
    :
    QnMultiserverRequestData()
{
    loadFromParams(resourcePool, params);
}

QnMultiserverRequestData::QnMultiserverRequestData(const QnMultiserverRequestData& src):
    isLocal(src.isLocal),
    format(src.format),
    extraFormatting(src.extraFormatting)
{
}

void QnMultiserverRequestData::loadFromParams(QnResourcePool* resourcePool,
    const QnRequestParamList& params)
{
    isLocal = params.contains(kLocalParam);
    extraFormatting = params.contains(kExtraFormattingParam);
    format = QnLexical::deserialized(params.value(kFormatParam), kDefaultFormat);
}

void QnMultiserverRequestData::loadFromParams(QnResourcePool* resourcePool,
    const QnRequestParams& params)
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
