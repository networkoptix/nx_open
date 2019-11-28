#pragma once

#include <nx/streaming/rtp/parsers/rtp_stream_parser.h>
#include <nx/streaming/rtp/parsers/i_rtp_parser_factory.h>

class OnvifMetadataRtpParserFactory: public nx::streaming::rtp::IRtpParserFactory
{
public:
    virtual std::unique_ptr<nx::streaming::rtp::StreamParser>
        createParser(const QString& codecName) override;

    virtual std::set<QString> supportedCodecs() const override;
};
