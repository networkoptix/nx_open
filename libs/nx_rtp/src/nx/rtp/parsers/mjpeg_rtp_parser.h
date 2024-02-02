// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>

#include <nx/rtp/parsers/rtp_stream_parser.h>

namespace nx::rtp {

class NX_RTP_API MjpegParser: public VideoStreamParser
{
public:
    MjpegParser();
    virtual void setSdpInfo(const Sdp::Media& sdp) override;

    virtual Result processData(
        const RtpHeader& rtpHeader,
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) override;

    virtual Result processRtpExtension(
        const RtpHeaderExtensionHeader& extensionHeader, quint8* data, int size) override;
    virtual void clear() override {}

    // Resolution that was configured using the camera API.
    void setConfiguredResolution(int width, int height);

private:
    void makeHeaders(
        int type, int width, int height, const quint8* lqt, const quint8* cqt, unsigned short dri);

    void updateHeaderTables(const quint8* lumaTable, const quint8* chromaTable);
    void fixResolution(int* width, int* height);

private:
    bool resolutionWorkaroundLogged = false;
    bool mjpeg16BitWarningLogged = false;

    //AVJpeg::Header m_jpegHeader;
    quint8 m_lumaTable[64 * 2];
    quint8 m_chromaTable[64 * 2];
    int m_frameWidth;
    int m_frameHeight;

    int m_configuredWidth = 0;
    int m_configuredHeight = 0;

    int m_lastJpegQ;

    int m_hdrQ;
    int m_hdrDri;
    int m_hdrWidth;
    int m_hdrHeight;
    std::vector<uint8_t> m_hdrBuffer;
    int m_lumaTablePos;
    int m_chromaTablePos;

    //nx::utils::ByteArray m_frameData;
    int m_frameSize;
    std::vector<uint8_t> m_onvifJpegHeader;
    bool m_sofFound = false;
    CodecParametersConstPtr m_codecParameters;
    struct Chunk
    {
        int offset;
        int size;
    };
    std::vector<Chunk> m_chunks;
};

} // namespace nx::rtp
