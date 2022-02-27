// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "signer.h"

#include <export/sign_helper.h>

MediaSigner::MediaSigner():
    m_audioHash(EXPORT_SIGN_METHOD),
    m_signatureHash(EXPORT_SIGN_METHOD)
{
    m_signatureHash.reset();
    m_signatureHash.addData(EXPORT_SIGN_MAGIC, sizeof(EXPORT_SIGN_MAGIC));

    m_audioHash.reset();
}

void MediaSigner::processMedia(
    AVCodecParameters* avCodecParams, const uint8_t* data, int size, QnAbstractMediaData::DataType type)
{
    // Audio/video order can be broken after muxing/demuxing so use separate hash for audio
    if (type == QnAbstractMediaData::VIDEO)
        QnSignHelper::updateDigest(avCodecParams, m_signatureHash, data, size);
    else
        QnSignHelper::updateDigest(avCodecParams, m_audioHash, data, size);
}

QByteArray MediaSigner::buildSignature(QnLicensePool* licensePool)
{
    QByteArray signature = QnSignHelper::getSignPattern(licensePool);
    m_signatureHash.addData(m_audioHash.result());
    m_signatureHash.addData(signature);
    signature.replace(QnSignHelper::getSignMagic(),
        QnSignHelper::getSignFromDigest(m_signatureHash.result()));
    return signature;
}

QByteArray MediaSigner::buildSignature(const QByteArray& signPattern)
{
    m_signatureHash.addData(m_audioHash.result());
    QByteArray baPattern = signPattern.trimmed();
    QByteArray magic = QnSignHelper::getSignMagic();
    QList<QByteArray> patternParams = baPattern.split(QnSignHelper::getSignPatternDelim());
    baPattern.replace(0, magic.size(), magic);
    m_signatureHash.addData(baPattern);
    return m_signatureHash.result();
}
