#include "base_metadata_rtp_parser_factory.h"

#include <nx/streaming/rtp/parsers/base_metadata_rtp_parser.h>

namespace nx::streaming::rtp {

BaseMetadataRtpParserFactory::BaseMetadataRtpParserFactory(std::set<QString> codecs):
    m_codecs(std::move(codecs))
{
}

std::unique_ptr<StreamParser> BaseMetadataRtpParserFactory::createParser(
    const QString& codecName)
{
    if (supportsCodec(codecName))
        return std::make_unique<BaseMetadataRtpParser>();

    return nullptr;
}

bool BaseMetadataRtpParserFactory::supportsCodec(const QString& codecName) const
{
    return m_codecs.find(codecName.toLower()) != m_codecs.cend();
}

} // namespace nx::streaming::rtp
