#pragma once

#include <memory>
#include <set>

namespace nx::streaming::rtp {

class StreamParser;

class IRtpParserFactory
{
public:
    virtual std::unique_ptr<StreamParser> createParser(const QString& codecName) = 0;

    virtual std::set<QString> supportedCodecs() const = 0;
};

} // namespace nx::streaming::rtp
