// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_stream_event.h"

#include <QtCore/QList>

#include <nx/reflect/string_conversion.h>

namespace nx::vms::common {

QString toString(MediaStreamEvent value)
{
    switch (value)
    {
        case MediaStreamEvent::noEvent:
            return QString();
        case MediaStreamEvent::tooManyOpenedConnections:
            return "Too many opened connections";
        case MediaStreamEvent::forbiddenWithDefaultPassword:
            return "Please set up camera password";
        case MediaStreamEvent::forbiddenWithNoLicense:
            return "No license";
        case MediaStreamEvent::oldFirmware:
            return "Cameras has too old firmware";
        case MediaStreamEvent::cannotDecryptMedia:
            return "Cannot decrypt media";
        case MediaStreamEvent::incompatibleCodec:
            return "Incompatible codec";
        default:
            return "Unknown error";
    }
}

bool MediaStreamEventPacket::operator== (const MediaStreamEventPacket& right) const
{
    return code == right.code && extraData == right.extraData;
}

QByteArray serialize(const MediaStreamEventPacket& value)
{
    auto result = QByteArray::fromStdString(nx::reflect::toString(value.code));
    if (!value.extraData.isEmpty())
    {
        result.append(';');
        result.append(value.extraData);
    }
    return result;
}

bool deserialize(const QByteArray& data, MediaStreamEventPacket* outValue)
{
    // Previous version contains a single field, newer version contains two fields via ';'.
    // The format should stay compatible with previous versions because of mobile client
    // could connect to the old media servers.

    const int delimiterPos = data.indexOf(';');
    QByteArray eventType = delimiterPos == -1 ? data : data.left(delimiterPos);
    outValue->code = nx::reflect::fromString<nx::vms::common::MediaStreamEvent>(
        eventType.toStdString());
    outValue->extraData = delimiterPos == -1 ? QByteArray() : data.mid(delimiterPos + 1);

    return true;
}

} // namespace nx::vms::common
