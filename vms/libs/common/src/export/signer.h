#pragma once

#include <nx/streaming/media_data_packet.h>
#include <nx/utils/cryptographic_hash.h>

class QnLicensePool;

/*
 * Class helper that build new version of signature that process audio frames separatelly, due to
 * some media containers(#: ismv) can change audio/video packets order.
 */

class MediaSigner
{
public:
    MediaSigner();
    void processMedia(
        AVCodecContext* context, const uint8_t* data, int size, QnAbstractMediaData::DataType type);
    void processMedia(
        const QnConstMediaContextPtr& context, const uint8_t* data, int size, QnAbstractMediaData::DataType type);
    QByteArray buildSignature(QnLicensePool* licensePool);
    QByteArray buildSignature(const QByteArray& signPattern);
private:
    nx::utils::QnCryptographicHash m_audioHash;
    nx::utils::QnCryptographicHash m_signatureHash;
};
