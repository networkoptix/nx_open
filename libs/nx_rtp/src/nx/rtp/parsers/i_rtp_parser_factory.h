// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QString>

namespace nx::rtp {

class StreamParser;

class IRtpParserFactory
{
public:
    virtual ~IRtpParserFactory() = default;
    virtual std::unique_ptr<StreamParser> createParser(const QString& codecName) const = 0;
    virtual std::unique_ptr<StreamParser> createParser(int payloadType) const = 0;

    virtual bool supportsCodec(const QString& codecName) const = 0;
    virtual bool supportsPayloadType(int payloadType) const = 0;
};

} // namespace nx::rtp
