// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::common {

enum class MediaStreamEvent
{
    noEvent,

    tooManyOpenedConnections,
    forbiddenWithDefaultPassword,
    forbiddenWithNoLicense,
    oldFirmware,
    cannotDecryptMedia,
    incompatibleCodec,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(MediaStreamEvent*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<MediaStreamEvent>;
    return visitor(
        Item{MediaStreamEvent::noEvent, "NoEvent"},
        Item{MediaStreamEvent::tooManyOpenedConnections, "TooManyOpenedConnections"},
        Item{MediaStreamEvent::forbiddenWithDefaultPassword, "ForbiddenWithDefaultPassword"},
        Item{MediaStreamEvent::forbiddenWithNoLicense, "ForbiddenWithNoLicense"},
        Item{MediaStreamEvent::oldFirmware, "oldFirmare"},
        Item{MediaStreamEvent::cannotDecryptMedia, "cannotDecryptMedia"},
        Item{ MediaStreamEvent::incompatibleCodec, "incompatibleCodec" }
    );
}

struct NX_VMS_COMMON_API MediaStreamEventPacket
{
    bool operator== (const MediaStreamEventPacket& right) const;

    MediaStreamEvent code = MediaStreamEvent::noEvent;

    /**
     * Extra data depend on MediaStreamEvent. Currently it used for 'cannotDecryptMedia' only.
     * It contains 'encryptionData' from the media packet that can not be decrypted (key not found).
     * this data can be passed to the function:
     * AesKey makeKey(const QString& password, const std::array<uint8_t, 16>& ivVect) as a 'ivVect'
     * to check whether a user password matches encryption data or not.
    */
    QByteArray extraData;
};

NX_VMS_COMMON_API QString toString(MediaStreamEvent value);
NX_VMS_COMMON_API QString toString(MediaStreamEventPacket value);

NX_VMS_COMMON_API QByteArray serialize(const MediaStreamEventPacket& value);
NX_VMS_COMMON_API bool deserialize(const QByteArray& data, MediaStreamEventPacket* outValue);

} // namespace nx::vms::common
