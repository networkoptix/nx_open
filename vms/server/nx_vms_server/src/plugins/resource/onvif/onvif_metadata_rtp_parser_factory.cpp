#include "onvif_metadata_rtp_parser_factory.h"

#include <plugins/resource/onvif/onvif_metadata_rtp_parser.h>

using namespace nx::streaming::rtp;

std::unique_ptr<StreamParser> OnvifMetadataRtpParserFactory::createParser(
    const QString& codecName)
{
    const auto& supportedCodecs = OnvifMetadataRtpParser::kSupportedCodecs;
    if (supportedCodecs.find(codecName) != supportedCodecs.cend())
        return std::make_unique<OnvifMetadataRtpParser>();

    return nullptr;
}

std::set<QString> OnvifMetadataRtpParserFactory::supportedCodecs() const
{
    return OnvifMetadataRtpParser::kSupportedCodecs;
}
