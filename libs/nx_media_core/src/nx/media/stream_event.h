// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <nx/reflect/enum_instrument.h>

namespace nx::media {

enum class StreamEvent
{
    noEvent,

    tooManyOpenedConnections,
    forbiddenWithDefaultPassword,
    forbiddenWithNoLicense,
    oldFirmware,
    cannotDecryptMedia,
    incompatibleCodec,
    cameraNotReady,
    archiveRangeChanged,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(StreamEvent*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<StreamEvent>;
    return visitor(
        Item{StreamEvent::noEvent, "NoEvent"},
        Item{StreamEvent::tooManyOpenedConnections, "TooManyOpenedConnections"},
        Item{StreamEvent::forbiddenWithDefaultPassword, "ForbiddenWithDefaultPassword"},
        Item{StreamEvent::forbiddenWithNoLicense, "ForbiddenWithNoLicense"},
        Item{StreamEvent::oldFirmware, "oldFirmare"},
        Item{StreamEvent::cannotDecryptMedia, "cannotDecryptMedia"},
        Item{StreamEvent::incompatibleCodec, "incompatibleCodec"},
        Item{StreamEvent::cameraNotReady, "cameraNotInitialized"}
    );
}

struct NX_MEDIA_CORE_API StreamEventPacket
{
    bool operator== (const StreamEventPacket& right) const;

    StreamEvent code = StreamEvent::noEvent;

    /**
     * Extra data depend on StreamEvent. Currently it used for 'cannotDecryptMedia' only.
     * It contains 'encryptionData' from the media packet that can not be decrypted (key not found).
     * this data can be passed to the function:
     * AesKey makeKey(const QString& password, const std::array<uint8_t, 16>& ivVect) as a 'ivVect'
     * to check whether a user password matches encryption data or not.
    */
    QByteArray extraData;
};

NX_MEDIA_CORE_API QString toString(StreamEvent value);
NX_MEDIA_CORE_API QString toString(StreamEventPacket value);

NX_MEDIA_CORE_API QByteArray serialize(const StreamEventPacket& value);
NX_MEDIA_CORE_API bool deserialize(const QByteArray& data, StreamEventPacket* outValue);

} // namespace nx::media
