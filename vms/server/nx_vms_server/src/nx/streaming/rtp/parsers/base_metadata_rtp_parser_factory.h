#pragma once

#include <nx/streaming/rtp/parsers/rtp_stream_parser.h>
#include <nx/streaming/rtp/parsers/i_rtp_parser_factory.h>

namespace nx::streaming::rtp {

class BaseMetadataRtpParserFactory: public nx::streaming::rtp::IRtpParserFactory
{
public:
    BaseMetadataRtpParserFactory(std::set<QString> codecs);

    virtual std::unique_ptr<nx::streaming::rtp::StreamParser>
        createParser(const QString& codecName) override;

    virtual bool supportsCodec(const QString& codecName) const override;

private:
    const std::set<QString> m_codecs;
};

} // namespace nx::streaming::rtp
