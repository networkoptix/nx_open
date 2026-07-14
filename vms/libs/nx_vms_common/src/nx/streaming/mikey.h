// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QByteArray>

#include <nx/rtp/sdp.h>
#include <nx/utils/url.h>
#include <rtsp/srtp_encryptor.h>

namespace nx::streaming::rtsp {

struct ClientManagedMikeyData
{
    nx::rtsp::EncryptionData encryptionData = {};
    QByteArray keyMgmtHeader;
};

enum class MikeyPolicyMode
{
    rfc,
    gstreamerCompatibility,
};

bool isClientManagedMikeyMedia(const nx::rtp::Sdp::Media& media);

std::optional<ClientManagedMikeyData> makeClientManagedMikey(const nx::rtp::Sdp::Media& media,
    const nx::Url& setupUrl, MikeyPolicyMode policyMode = MikeyPolicyMode::rfc);

} // namespace nx::streaming::rtsp
