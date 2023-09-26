// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/media_data_packet.h>
#include <nx/utils/cryptographic_hash.h>

class QnLicensePool;
class QnUuid;

/*
 * Class helper that build new version of signature that process audio frames separately, due to
 * some media containers(#: ismv) can change audio/video packets order.
 */

class NX_VMS_COMMON_API MediaSigner
{
public:
    MediaSigner();
    void processMedia(AVCodecParameters* context, const uint8_t* data, int size);

    QByteArray buildSignature(QnLicensePool* licensePool, const QnUuid& serverId);
    QByteArray buildSignature(const QByteArray& signPattern);
    QByteArray currentResult();
    void reset();

private:
    nx::utils::QnCryptographicHash m_audioHash;
    nx::utils::QnCryptographicHash m_signatureHash;
};
