// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <nx/rtp/parsers/i_rtp_parser_factory.h>
#include <nx/rtp/parsers/rtp_stream_parser.h>

namespace nx::rtp {

class NX_RTP_API BaseMetadataRtpParserFactory: public nx::rtp::IRtpParserFactory
{
public:
    BaseMetadataRtpParserFactory(std::set<QString> codecs);

    virtual std::unique_ptr<nx::rtp::StreamParser>
        createParser(const QString& codecName) const override;
    virtual std::unique_ptr<nx::rtp::StreamParser>
        createParser(int payloadType) const override;

    virtual bool supportsCodec(const QString& codecName) const override;
    virtual bool supportsPayloadType(int payloadType) const override;

private:
    const std::set<QString> m_codecs;
};

} // namespace nx::rtp
