#include "multiserver_request_data.h"

#include <nx/fusion/model_functions.h>

namespace {

static const QString kLocalParam(lit("local"));
static const QString kFormatParam(lit("format"));
static const QString kExtraFormattingParam(lit("extraFormatting"));

} // namespace

const Qn::SerializationFormat QnBaseMultiserverRequestData::kDefaultFormat = Qn::JsonFormat;

QnBaseMultiserverRequestData::QnBaseMultiserverRequestData(const RequestParams& params)
{
    loadFromParams(params);
}

void QnBaseMultiserverRequestData::loadFromParams(const RequestParams& params)
{
    isLocal = params.contains(kLocalParam);
    extraFormatting = params.contains(kExtraFormattingParam);
    format = QnLexical::deserialized(params.value(kFormatParam), kDefaultFormat);
}

QnMultiserverRequestData::~QnMultiserverRequestData()
{
}

void QnMultiserverRequestData::loadFromParams(QnResourcePool* resourcePool,
    const RequestParams& params)
{
    QnBaseMultiserverRequestData::loadFromParams(params);
}

RequestParams QnMultiserverRequestData::toParams() const
{
    RequestParams result;
    if (isLocal)
        result.insert(kLocalParam, QString());
    if (extraFormatting)
        result.insert(kExtraFormattingParam, QString());
    result.insert(kFormatParam, QnLexical::serialized(format));
    return result;
}

QUrlQuery QnMultiserverRequestData::toUrlQuery() const
{
    return toParams().toUrlQuery();
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
