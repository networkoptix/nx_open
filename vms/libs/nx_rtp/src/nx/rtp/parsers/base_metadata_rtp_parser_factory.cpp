// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base_metadata_rtp_parser_factory.h"

#include <nx/rtp/parsers/base_metadata_rtp_parser.h>

namespace nx::rtp {

BaseMetadataRtpParserFactory::BaseMetadataRtpParserFactory(std::set<QString> codecs):
    m_codecs(std::move(codecs))
{
}

std::unique_ptr<StreamParser> BaseMetadataRtpParserFactory::createParser(
    const QString& codecName) const
{
    if (supportsCodec(codecName))
        return std::make_unique<BaseMetadataRtpParser>();

    return nullptr;
}

std::unique_ptr<StreamParser> BaseMetadataRtpParserFactory::createParser(int /*payloadType*/) const
{
    return nullptr;
}

bool BaseMetadataRtpParserFactory::supportsCodec(const QString& codecName) const
{
    return m_codecs.find(codecName.toLower()) != m_codecs.cend();
}

bool BaseMetadataRtpParserFactory::supportsPayloadType(int /*payloadType*/) const
{
    return false;
}

} // namespace nx::rtp
