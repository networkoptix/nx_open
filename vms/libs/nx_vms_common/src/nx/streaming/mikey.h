// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>

#include <QtCore/QByteArray>

#include <nx/rtp/sdp.h>
#include <nx/utils/url.h>
#include <rtsp/srtp_encryptor.h>

namespace nx::streaming::rtsp {

struct MikeyData
{
    nx::rtsp::EncryptionData encryptionData = {};
    QByteArray keyMgmtHeader;
};

NX_VMS_COMMON_API std::optional<std::string> getMikeyPayload(
    const nx::rtp::Sdp::Media& media, const std::vector<std::string>& sessionSdpAttributes);

NX_VMS_COMMON_API std::optional<MikeyData> makeStandardMikey(
    const std::string& mikeyPayload, const nx::Url& setupUrl);

NX_VMS_COMMON_API std::optional<MikeyData> makeClientManagedMikey(
    const nx::Url& setupUrl, nx::rtsp::SrtpCryptoPolicy cryptoPolicy = {});

} // namespace nx::streaming::rtsp
