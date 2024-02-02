// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_event.h"

#include <QtCore/QList>

#include <nx/reflect/string_conversion.h>

namespace nx::media {

QString toString(StreamEvent value)
{
    switch (value)
    {
        case StreamEvent::noEvent:
            return QString();
        case StreamEvent::tooManyOpenedConnections:
            return "Too many opened connections";
        case StreamEvent::forbiddenWithDefaultPassword:
            return "Please set up camera password";
        case StreamEvent::forbiddenWithNoLicense:
            return "No license";
        case StreamEvent::oldFirmware:
            return "Cameras has too old firmware";
        case StreamEvent::cannotDecryptMedia:
            return "Cannot decrypt media";
        case StreamEvent::incompatibleCodec:
            return "Incompatible codec";
        default:
            return "Unknown error";
    }
}

bool StreamEventPacket::operator== (const StreamEventPacket& right) const
{
    return code == right.code && extraData == right.extraData;
}

QByteArray serialize(const StreamEventPacket& value)
{
    auto result = QByteArray::fromStdString(nx::reflect::toString(value.code));
    if (!value.extraData.isEmpty())
    {
        result.append(';');
        result.append(value.extraData);
    }
    return result;
}

bool deserialize(const QByteArray& data, StreamEventPacket* outValue)
{
    // Previous version contains a single field, newer version contains two fields via ';'.
    // The format should stay compatible with previous versions because of mobile client
    // could connect to the old media servers.

    const int delimiterPos = data.indexOf(';');
    QByteArray eventType = delimiterPos == -1 ? data : data.left(delimiterPos);
    outValue->code = nx::reflect::fromString<nx::media::StreamEvent>(
        eventType.toStdString());
    outValue->extraData = delimiterPos == -1 ? QByteArray() : data.mid(delimiterPos + 1);

    return true;
}

} // namespace nx::media
