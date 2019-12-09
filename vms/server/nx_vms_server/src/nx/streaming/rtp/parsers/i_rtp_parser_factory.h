#pragma once

#include <memory>
#include <set>

namespace nx::streaming::rtp {

class StreamParser;

class IRtpParserFactory
{
public:
    virtual ~IRtpParserFactory() = default;
    virtual std::unique_ptr<StreamParser> createParser(const QString& codecName) = 0;

    virtual bool supportsCodec(const QString& codecName) const = 0;
};

} // namespace nx::streaming::rtp
