#include "multiserver_request_data.h"

#include <nx/fusion/model_functions.h>

namespace {

static const QString kLocalParam(lit("local"));
static const QString kFormatParam(lit("format"));
static const QString kExtraFormattingParam(lit("extraFormatting"));

} // namespace

const Qn::SerializationFormat QnBaseMultiserverRequestData::kDefaultFormat = Qn::JsonFormat;

QnBaseMultiserverRequestData::QnBaseMultiserverRequestData(const QnRequestParamList& params)
{
    loadFromParams(params);
}

QnBaseMultiserverRequestData::QnBaseMultiserverRequestData(const QnRequestParams& params)
{
    loadFromParams(params);
}

void QnBaseMultiserverRequestData::loadFromParams(const QnRequestParamList& params)
{
    isLocal = params.contains(kLocalParam);
    extraFormatting = params.contains(kExtraFormattingParam);
    format = QnLexical::deserialized(params.value(kFormatParam), kDefaultFormat);
}

void QnBaseMultiserverRequestData::loadFromParams(const QnRequestParams& params)
{
    isLocal = params.contains(kLocalParam);
    extraFormatting = params.contains(kExtraFormattingParam);
    format = QnLexical::deserialized(params.value(kFormatParam), kDefaultFormat);
}

QnMultiserverRequestData::~QnMultiserverRequestData()
{
}

void QnMultiserverRequestData::loadFromParams(QnResourcePool* resourcePool,
    const QnRequestParamList& params)
{
    QnBaseMultiserverRequestData::loadFromParams(params);
}

void QnMultiserverRequestData::loadFromParams(QnResourcePool* resourcePool,
    const QnRequestParams& params)
{
    QnBaseMultiserverRequestData::loadFromParams(params);
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
