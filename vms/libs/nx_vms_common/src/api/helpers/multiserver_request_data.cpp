// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multiserver_request_data.h"

#include <nx/network/rest/params.h>
#include <nx/reflect/string_conversion.h>

namespace {

static const QString kLocalParam(lit("local"));
static const QString kFormatParam(lit("format"));
static const QString kExtraFormattingParam(lit("extraFormatting"));

} // namespace

const Qn::SerializationFormat QnBaseMultiserverRequestData::kDefaultFormat =
    Qn::SerializationFormat::json;

QnBaseMultiserverRequestData::QnBaseMultiserverRequestData(const nx::network::rest::Params& params)
{
    loadFromParams(params);
}

void QnBaseMultiserverRequestData::loadFromParams(const nx::network::rest::Params& params)
{
    isLocal = params.contains(kLocalParam);
    extraFormatting = params.contains(kExtraFormattingParam);
    format = nx::reflect::fromString(params.value(kFormatParam).toStdString(), kDefaultFormat);
}

QnMultiserverRequestData::~QnMultiserverRequestData()
{
}

void QnMultiserverRequestData::loadFromParams(QnResourcePool* /*resourcePool*/,
    const nx::network::rest::Params& params)
{
    QnBaseMultiserverRequestData::loadFromParams(params);
}

nx::network::rest::Params QnBaseMultiserverRequestData::toParams() const
{
    nx::network::rest::Params result;
    if (isLocal)
        result.insert(kLocalParam, QString());
    if (extraFormatting)
        result.insert(kExtraFormattingParam, QString());
    result.insert(kFormatParam, format);
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
