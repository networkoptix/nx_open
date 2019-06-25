#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <QtCore/QByteArray>

#include <nx/streaming/rtp/parsers/rtp_stream_parser.h>

namespace nx::streaming::rtp {

class MjpegParser: public VideoStreamParser
{
public:
    MjpegParser();
    virtual void setSdpInfo(const Sdp::Media& sdp) override;

    virtual bool processData(
        quint8* rtpBufferBase, int bufferOffset, int bytesRead, bool& gotData) override;

private:
    int makeHeaders(
        quint8* p, int type, int w, int h, const quint8* lqt, const quint8* cqt, unsigned short dri);

    void updateHeaderTables(const quint8* lumaTable, const quint8* chromaTable);

    bool parseMjpegExtension(const quint8* data, int size);
    bool processRtpExtensions(const quint8* data, int size);
private:
    bool resolutionWorkaroundLogged = false;
    bool mjpeg16BitWarningLogged = false;

    // TODO: Consider adding pix_fmt to QnMediaContext. Currently m_context is not used.
    //QnMediaContextPtr m_context;

    //AVJpeg::Header m_jpegHeader;
    quint8 m_lumaTable[64 * 2];
    quint8 m_chromaTable[64 * 2];
    int m_frameWidth;
    int m_frameHeight;

    int m_lastJpegQ;

    int m_hdrQ;
    int m_hdrDri;
    int m_hdrWidth;
    int m_hdrHeight;
    quint8 m_hdrBuffer[1024];
    int m_headerLen;
    quint8* m_lumaTablePos;
    quint8* m_chromaTablePos;

    //QnByteArray m_frameData;
    int m_frameSize;
    std::vector<quint8> m_extendedJpegHeader;
};

} // namespace nx::streaming::rtp

#endif // defined(ENABLE_DATA_PROVIDERS)
