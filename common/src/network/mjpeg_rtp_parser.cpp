#include "mjpeg_rtp_parser.h"

#if defined(ENABLE_DATA_PROVIDERS)

#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/rtp_stream_parser.h>
#include <nx/streaming/rtsp_client.h>
#include <nx/streaming/config.h>
#include <utils/common/synctime.h>
#include <nx/utils/log/log.h>

#if 0
static const char H264_NAL_PREFIX[4] = {0x00, 0x00, 0x00, 0x01};
static const char H264_NAL_SHORT_PREFIX[3] = {0x00, 0x00, 0x01};
static const int DEFAULT_SLICE_SIZE = 1024 * 1024;
static const int DEFAULT_MJPEG_HEADER_SIZE = 1024;
#endif // 0

#if 0

/**
 * Table K.1 from JPEG spec.
 */
static const int jpeg_luma_quantizer[64] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

/**
 * Table K.2 from JPEG spec.
 */
static const int jpeg_chroma_quantizer[64] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

#else // 0

// Tables from ffmpeg.

static const int jpeg_luma_quantizer[64] = {
    16,  11,  12,  14,  12,  10,  16,  14,
    13,  14,  18,  17,  16,  19,  24,  40,
    26,  24,  22,  22,  24,  49,  35,  37,
    29,  40,  58,  51,  61,  60,  57,  51,
    56,  55,  64,  72,  92,  78,  64,  68,
    87,  69,  55,  56,  80,  109, 81,  87,
    95,  98,  103, 104, 103, 62,  77,  113,
    121, 112, 100, 120, 92,  101, 103, 99
};

static const int jpeg_chroma_quantizer[64] = {
    17,  18,  18,  24,  21,  24,  47,  26,
    26,  47,  99,  66,  56,  66,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99
};

#endif // 0

/**
 * Call MakeTables with the Q factor and two quint8[64] return arrays.
 */
void MakeTables(int q, quint8* lqt, quint8* cqt)
{
    int i;
    int factor = q;

    if (q < 1) factor = 1;
    if (q > 99) factor = 99;
    if (q < 50)
        q = 5000 / factor;
    else
        q = 200 - factor * 2;
    for (i = 0; i < 64; i++)
    {
        int lq = (jpeg_luma_quantizer[i] * q + 50) / 100;
        int cq = (jpeg_chroma_quantizer[i] * q + 50) / 100;

        // Limit the quantizers to 1 <= q <= 255.
        if (lq < 1)
            lq = 1;
        else if (lq > 255)
            lq = 255;
        lqt[i] = lq;

        if (cq < 1)
            cq = 1;
        else if (cq > 255)
            cq = 255;
        cqt[i] = cq;
    }
}

quint8 lum_dc_codelens[] = {
    0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
};

quint8 lum_dc_symbols[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

quint8 lum_ac_codelens[] = {
    0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d,
};

quint8 lum_ac_symbols[] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
    0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
    0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
    0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa,
};

quint8 chm_dc_codelens[] = {
    0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
};

quint8 chm_dc_symbols[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

quint8 chm_ac_codelens[] = {
        0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77,
};

quint8 chm_ac_symbols[] = {
        0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
        0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
        0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
        0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
        0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
        0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
        0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
        0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
        0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
        0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
        0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
        0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
        0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
        0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
        0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
        0xf9, 0xfa,
};

quint8* MakeQuantHeader(quint8* p, const quint8* qt, int tableNo)
{
    *p++ = 0xff;
    *p++ = 0xdb; //< DQT
    *p++ = 0; //< length msb
    *p++ = 67; //< length lsb
    *p++ = tableNo;
    memcpy(p, qt, 64);
    return p + 64;
}

quint8* MakeHuffmanHeader(quint8* p, quint8* codelens, int ncodes,
    quint8* symbols, int nsymbols, int tableNo, int tableClass)
{
    *p++ = 0xff;
    *p++ = 0xc4; //< DHT
    *p++ = 0; //< length msb
    *p++ = 3 + ncodes + nsymbols; //< length lsb
    *p++ = (tableClass << 4) | tableNo;
    memcpy(p, codelens, ncodes);
    p += ncodes;
    memcpy(p, symbols, nsymbols);
    p += nsymbols;
    return p;
}

quint8* MakeDRIHeader(quint8* p, u_short dri)
{
    *p++ = 0xff;
    *p++ = 0xdd; //< DRI
    *p++ = 0x0; //< length msb
    *p++ = 4; //< length lsb
    *p++ = dri >> 8; //< dri msb
    *p++ = dri & 0xff; //< dri lsb
    return p;
}

/**
 * Generate a frame and scan headers that can be prepended to the RTP/JPEG data payload to produce
 * a JPEG compressed image in the interchange format (except for possible trailing garbage and
 * absence of an EOI marker to terminate the scan).
 *
 * @param type, width, height As supplied in RTP/JPEG header.
 * @param lqt, cqt Quantization tables as either derived from the Q field using MakeTables() or as
 *     specified in section 4.2.
 * @param dri Restart interval in MCUs, or 0 if no restart.
 * @param p Pointer to the return array.
 * @return Length of the generated headers.
 */
int QnMjpegRtpParser::makeHeaders(
    quint8* p, int type, int w, int h, const quint8* lqt, const quint8* cqt, u_short dri)
{
    quint8* start = p;

    // Convert from blocks to pixels.
    w <<= 3;
    h <<= 3;
    *p++ = 0xff;
    *p++ = 0xd8; //< SOI

    m_lumaTablePos = p + 5;
    p = MakeQuantHeader(p, lqt, 0);
    m_chromaTablePos = p + 5;
    p = MakeQuantHeader(p, cqt, 1);

    if (dri != 0)
        p = MakeDRIHeader(p, dri);

    const int bytesLeft = sizeof(m_hdrBuffer) - (p - start);
    if (m_extendedJpegHeader.size() > 0 && m_extendedJpegHeader.size() < bytesLeft)
    {
        memcpy(p, m_extendedJpegHeader.data(), m_extendedJpegHeader.size());
        p += m_extendedJpegHeader.size();
        return p - start;
    }

    *p++ = 0xff;
    *p++ = 0xc0; //< SOF
    *p++ = 0; //< length msb
    *p++ = 17; //< length lsb
    *p++ = 8; //< 8-bit precision
    *p++ = h >> 8; //< height msb
    *p++ = h; //< height lsb
    *p++ = w >> 8; //< width msb
    *p++ = w; //< width lsb
    *p++ = 3; //< number of components
    *p++ = 0; //< comp 0
    if (type == 0)
        *p++ = 0x21; //< hsamp = 2, vsamp = 1
    else
        *p++ = 0x22; //< hsamp = 2, vsamp = 2
    *p++ = 0; //< quant table 0
    *p++ = 1; //< comp 1
    *p++ = 0x11; //< hsamp = 1, vsamp = 1
    *p++ = 1; //< quant table 1
    *p++ = 2; //< comp 2
    *p++ = 0x11; //< hsamp = 1, vsamp = 1
    *p++ = 1; //< quant table 1
    p = MakeHuffmanHeader(p, lum_dc_codelens,
        sizeof(lum_dc_codelens),
        lum_dc_symbols,
        sizeof(lum_dc_symbols), 0, 0);
    p = MakeHuffmanHeader(p, lum_ac_codelens,
        sizeof(lum_ac_codelens),
        lum_ac_symbols,
        sizeof(lum_ac_symbols), 0, 1);
    p = MakeHuffmanHeader(p, chm_dc_codelens,
        sizeof(chm_dc_codelens),
        chm_dc_symbols,
        sizeof(chm_dc_symbols), 1, 0);
    p = MakeHuffmanHeader(p, chm_ac_codelens,
        sizeof(chm_ac_codelens),
        chm_ac_symbols,
        sizeof(chm_ac_symbols), 1, 1);
    *p++ = 0xff;
    *p++ = 0xda; //< SOS
    *p++ = 0; //< length msb
    *p++ = 12; //< length lsb
    *p++ = 3; //< 3 components
    *p++ = 0; //< comp 0
    *p++ = 0; //< huffman table 0
    *p++ = 1; //< comp 1
    *p++ = 0x11; //< huffman table 1
    *p++ = 2; //< comp 2
    *p++ = 0x11; //< huffman table 1
    *p++ = 0; //< first DCT coeff
    *p++ = 63; //< last DCT coeff
    *p++ = 0; //< sucessive approx.

    return p - start;
}

void QnMjpegRtpParser::updateHeaderTables(const quint8* lumaTable, const quint8* chromaTable)
{
    memcpy(m_lumaTablePos, lumaTable, 64 * 1);
    memcpy(m_chromaTablePos, chromaTable, 64 * 1);
}

//-------------------------------------------------------------------------------------------------

QnMjpegRtpParser::QnMjpegRtpParser():
    QnRtpVideoStreamParser(),
    m_frequency(90000)
    //m_frameData(CL_MEDIA_ALIGNMENT, 1024 * 64)
{
    memset(m_lumaTable, 0, sizeof(m_lumaTable));
    memset(m_chromaTable, 0, sizeof(m_chromaTable));
    m_frameWidth = 0;
    m_frameHeight = 0;
    m_lastJpegQ = -1;

    m_hdrQ = -1;
    m_hdrDri = 0;
    m_hdrWidth = -1;
    m_hdrHeight = -1;
    m_headerLen = 0;
    m_lumaTablePos = 0;
    m_chromaTablePos = 0;
    m_frameSize = 0;
}

QnMjpegRtpParser::~QnMjpegRtpParser()
{
}

void QnMjpegRtpParser::setSDPInfo(QList<QByteArray> lines)
{
    for (int i = 0; i < lines.size(); ++i)
    {
        lines[i] = lines[i].trimmed();
        if (lines[i].startsWith("a=framesize:"))
        {
            QList<QByteArray> values = lines[i].split(' ');
            if (values.size() > 1)
            {
                QList<QByteArray> dimension = values[1].split('-');
                if (dimension.size() == 2)
                {
                    m_frameWidth = dimension[0].trimmed().toInt();
                    m_frameHeight = dimension[1].trimmed().toInt();
                }
            }
        }
        else if (lines[i].startsWith("a=x-dimensions:"))
        {
            QList<QByteArray> values = lines[i].split(':');
            if (values.size() > 1)
            {
                QList<QByteArray> dimension = values[1].split(',');
                if (dimension.size() == 2)
                {
                    m_frameWidth = dimension[0].trimmed().toInt();
                    m_frameHeight = dimension[1].trimmed().toInt();
                }
            }
        }
    }
}

bool QnMjpegRtpParser::parseMjpegExtension(const quint8* data, int size)
{
    // Remove padding
    while (size > 0 && data[size - 1] == 0xff)
        --size;

    m_extendedJpegHeader.resize(size);
    if (size > 0)
        memcpy(m_extendedJpegHeader.data(), data, size);
    if (size >= 9)
    {
        quint16 markerType = (data[0] << 8) + data[1];
        if (markerType == 0xffc0)
        {
            // Ffmpeg SOF header. Extract size from offset [5..8]
            m_frameHeight = (data[5] << 8) + data[6];
            m_frameWidth = (data[7] << 8) + data[8];
        }
    }
    return true;
}

bool getUint16(const quint8** data, int* size, quint16* result)
{
    if (*size < 2)
        return false;
    *result = ((*data)[0] << 8) + (*data)[1];
    *data += 2;
    *size -= 2;
    return true;
};

bool QnMjpegRtpParser::processRtpExtensions(const quint8* data, int size)
{
    static const quint16 kOnvifTimeExtensionCode = 0xabac;
    static const quint16 kOnvifJpegExtensionCode = 0xffd8;
    while (size > 0)
    {
        quint16 extensionCode;
        if (!getUint16(&data, &size, &extensionCode))
            return false;
        quint16 extensionSize;
        if (!getUint16(&data, &size, &extensionSize))
            return false;
        extensionSize *= 4;
        if (extensionSize > size)
            return false;

        switch (extensionCode)
        {
        case kOnvifTimeExtensionCode:
            extensionSize = 3 * 4;
            break;
        case kOnvifJpegExtensionCode:
            parseMjpegExtension(data, extensionSize);
            break;
        default:
            break;
        }

        data += extensionSize;
        size -= extensionSize;
    }
    return size == 0;
}

bool QnMjpegRtpParser::processData(quint8* rtpBufferBase, int bufferOffset, int bytesRead, const QnRtspStatistic& statistics, bool& gotData)
{
    gotData = false;
    const quint8* rtpBuffer = rtpBufferBase + bufferOffset;

    static quint8 jpeg_end[2] = {0xff, 0xd9};

    if (bytesRead < RtpHeader::RTP_HEADER_SIZE + 1)
        return false;

    RtpHeader* rtpHeader = (RtpHeader*)rtpBuffer;
    const quint8* curPtr = rtpBuffer + RtpHeader::RTP_HEADER_SIZE;
    int bytesLeft = bytesRead - RtpHeader::RTP_HEADER_SIZE;

    if (rtpHeader->extension)
    {
        if (bytesRead < RtpHeader::RTP_HEADER_SIZE + 4)
            return false;

        int extWords = ((int)curPtr[2] << 8) + curPtr[3];
        const auto extensionsSize = (extWords + 1) * 4;
        bytesLeft -= extensionsSize;
        processRtpExtensions(curPtr, std::min(extensionsSize, bytesLeft));
        curPtr += extensionsSize;
    }

    if (rtpHeader->padding != 0)
        bytesLeft -= ntohl(rtpHeader->padding);

    if (bytesLeft < 8)
        return false;

    // 1. jpeg main header

    int typeSpecific = *curPtr++;
    Q_UNUSED(typeSpecific);
    int fragmentOffset = (curPtr[0] << 16) + (curPtr[1] << 8) + curPtr[2];
    curPtr += 3;
    int jpegType = *curPtr++;
    int jpegQ = *curPtr++;
    int width = *curPtr++;
    int height = *curPtr++;

    if (m_frameWidth > 0)
        width = m_frameWidth / 8;
    if (m_frameHeight > 0)
        height = m_frameHeight / 8;

    // Workaround: certain 4K cameras do not provide "a=framesize", and drop MSB from resolution.
    if (m_frameWidth <= 0 && m_frameHeight <= 0
        && width == (3840 - 2048) / 8 && height == (2160 - 2048) / 8)
    {
        if (!resolutionWorkaroundLogged)
        {
            resolutionWorkaroundLogged = true;
            NX_LOG(lit(
                "[mjpeg_rtp_parser] Camera reports resolution 1792 x 112, assuming 3840 x 2160 (~4K)"),
                cl_logDEBUG1);
        }
        width = 3840 / 8;
        height = 2160 / 8;
    }

    bytesLeft -= 8;

    quint16 dri = 0;
    // 2. restart marker header (optional)
    if (jpegType & 0x40)
    {
        if (bytesLeft < 4)
            return false;
        dri = (curPtr[0] << 8) + curPtr[1];

        curPtr += 4;
        bytesLeft -= 4;
        jpegType &= ~0x40;
    }

    if (fragmentOffset == 0)
    {
        // 3. Quantization Table header
        const quint8* lumaTable = 0;
        const quint8* chromaTable = 0;
        if (jpegQ >= 128)
        {
            if (bytesLeft < 4)
                return false;
            quint8 MBZ = *curPtr++;
            Q_UNUSED(MBZ);
            quint8 Precision = *curPtr++;
            quint16 length = (curPtr[0] << 8) + curPtr[1];
            curPtr += 2;
            bytesLeft -= 4;

            if (bytesLeft < length)
                return false;

            int lumaSize = (Precision & 1) ? 2 : 1;
            int chromaSize = (Precision & 2) ? 2 : 1;

            if (lumaSize != 1 || chromaSize != 1)
            {
                if (!mjpeg16BitWarningLogged)
                {
                    mjpeg16BitWarningLogged = true;
                    NX_LOG(lit("16-bit MJPEG is not supported"), cl_logDEBUG1);
                }
                return false; //< Only 8-bit MJPEG is supported.
            }

            if (length > 0)
            {
                if (length < 64 * (lumaSize + chromaSize))
                    return false;
                // RFC2435. Same table for each frame, make deep copy of tables.
                lumaTable = curPtr;
                chromaTable = curPtr + 64 * lumaSize;
                if (jpegQ != 255)
                {
                    // Make deep copy because the same table will be reused.
                    memcpy(m_lumaTable, curPtr, 64 * lumaSize);
                    memcpy(m_chromaTable, curPtr + 64 * lumaSize, 64 * chromaSize);
                }
            }

            curPtr += length;
            bytesLeft -= length;
        }
        else if (jpegQ != m_lastJpegQ)
        {
            MakeTables(jpegQ, m_lumaTable, m_chromaTable);
            m_lastJpegQ = jpegQ;
        }

        if (m_hdrQ != jpegQ || dri != m_hdrDri || m_hdrWidth != width || m_hdrHeight != height ||
           !m_extendedJpegHeader.empty())
        {
            if (jpegQ != 255)
            {
                m_headerLen = makeHeaders(m_hdrBuffer, jpegType, width, height,
                    m_lumaTable, m_chromaTable, dri); //< use deep copy
            }
            else
            {
                if (!lumaTable || !chromaTable)
                    return false;
                m_headerLen = makeHeaders(m_hdrBuffer, jpegType, width, height,
                    lumaTable, chromaTable, dri); //< use tables direct pointer
            }
            m_hdrQ = jpegQ;
            m_hdrDri = dri;
            m_hdrWidth = width;
            m_hdrHeight = height;
        }
        else if (lumaTable && chromaTable)
        {
            updateHeaderTables(lumaTable, chromaTable);
        }

        m_chunks.clear();
        m_frameSize = 0;
    }

    // See ToDo in the header.
#if 0
    if (!m_context)
    {
        m_context = QnMediaContextPtr(new QnAvCodecMediaContext(AV_CODEC_ID_MJPEG));
        m_context->ctx()->pix_fmt = (jpegType & 1) == 1 ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_YUV422P;
        //m_jpegHeader.Initialize(qApp->organizationName().toLatin1().constData(), qApp->applicationName().toLatin1().constData(), "");
    }
#endif // 0

    //m_frameData.write((const char*)curPtr, bytesLeft);
    m_chunks.push_back(Chunk(curPtr - rtpBufferBase, bytesLeft));
    m_frameSize += bytesLeft;

    bool lastPacketReceived = rtpHeader->marker;
    if (lastPacketReceived)
    {
        const quint8* EOI_marker = curPtr + bytesLeft - 2;
        //if (m_frameSize < 2 || EOI_marker[0] != jpeg_end[0] || EOI_marker[1] != jpeg_end[1])
        //    m_frameData.write((const char*) jpeg_end, sizeof(jpeg_end));
        bool needAddMarker =
            m_frameSize < 2 || EOI_marker[0] != jpeg_end[0] || EOI_marker[1] != jpeg_end[1];

        QnWritableCompressedVideoDataPtr videoData(new QnWritableCompressedVideoData(
            CL_MEDIA_ALIGNMENT, m_headerLen + m_frameSize + (needAddMarker ? 2 : 0)));
        m_mediaData = videoData;
        videoData->m_data.uncheckedWrite((const char*)m_hdrBuffer, m_headerLen);
        //m_videoData->data.write(m_frameData);
        for (uint i = 0; i < m_chunks.size(); ++i)
        {
            videoData->m_data.uncheckedWrite(
                (const char*)rtpBufferBase + m_chunks[i].bufferOffset, m_chunks[i].len);
        }
        if (needAddMarker)
            videoData->m_data.uncheckedWrite((const char*)jpeg_end, sizeof(jpeg_end));

        m_chunks.clear();
        m_frameSize = 0;

        videoData->channelNumber = 0;
        videoData->flags |= QnAbstractMediaData::MediaFlags_AVKey;
        videoData->compressionType = AV_CODEC_ID_MJPEG;
        //m_videoData->context = m_context;
        videoData->width = width * 8;
        videoData->height = height * 8;

        if (m_timeHelper)
        {
            videoData->timestamp = m_timeHelper->getUsecTime(
                ntohl(rtpHeader->timestamp), statistics, m_frequency);
        }
        else
        {
            videoData->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;
        }
        gotData = true;
    }
    return true;
}

#endif // defined(ENABLE_DATA_PROVIDERS)
