// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mjpeg_rtp_parser.h"

#include <nx/codec/jpeg/jpeg_utils.h>
#include <nx/media/config.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/media/video_data_packet.h>
#include <nx/rtp/rtp.h>
#include <nx/utils/log/log.h>

#ifdef USE_RFC_JPEG_TABLES

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

#else

// Tables from ffmpeg.

namespace nx::rtp {

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

void appendDQT(std::vector<uint8_t>& buffer, const uint8_t* qt, int tableNo)
{
    buffer.push_back(0xff);
    buffer.push_back(0xdb); //< DQT
    buffer.push_back(0); //< length msb
    buffer.push_back(67); //< length lsb
    buffer.push_back(tableNo);
    buffer.insert(buffer.end(), qt, qt + 64);
}

void appendDHT(std::vector<uint8_t>& buffer, uint8_t* codelens, int ncodes,
    uint8_t* symbols, int nsymbols, int tableNo, int tableClass)
{
    buffer.push_back(0xff);
    buffer.push_back(0xc4); //< DHT
    buffer.push_back(0); //< length msb
    buffer.push_back(3 + ncodes + nsymbols); //< length lsb
    buffer.push_back((tableClass << 4) | tableNo);
    buffer.insert(buffer.end(), codelens, codelens + ncodes);
    buffer.insert(buffer.end(), symbols, symbols + nsymbols);
}

void appendDRI(std::vector<uint8_t>& buffer, u_short dri)
{
    buffer.push_back(0xff);
    buffer.push_back(0xdd); //< DRI
    buffer.push_back(0x0); //< length msb
    buffer.push_back(4); //< length lsb
    buffer.push_back(dri >> 8); //< dri msb
    buffer.push_back(dri & 0xff); //< dri lsb
}

void appendSOI(std::vector<uint8_t>& buffer)
{
    buffer.push_back(0xff);
    buffer.push_back(0xd8);
}

void appendSOF(std::vector<uint8_t>& buffer, int type, int width, int height)
{
    buffer.push_back(0xff);
    buffer.push_back(0xc0); //< SOF
    buffer.push_back(0); //< length msb
    buffer.push_back(17); //< length lsb
    buffer.push_back(8); //< 8-bit precision
    buffer.push_back(height >> 8); //< height msb
    buffer.push_back(height); //< height lsb
    buffer.push_back(width >> 8); //< width msb
    buffer.push_back(width); //< width lsb
    buffer.push_back(3); //< number of components
    buffer.push_back(1); //< comp 1
    if (type == 0)
        buffer.push_back(0x21); //< hsamp = 2, vsamp = 1
    else
        buffer.push_back(0x22); //< hsamp = 2, vsamp = 2
    buffer.push_back(0); //< quant table 0
    buffer.push_back(2); //< comp 2
    buffer.push_back(0x11); //< hsamp = 1, vsamp = 1
    buffer.push_back(1); //< quant table 1
    buffer.push_back(3); //< comp 3
    buffer.push_back(0x11); //< hsamp = 1, vsamp = 1
    buffer.push_back(1); //< quant table 1
}

void appendSOS(std::vector<uint8_t>& buffer)
{
    buffer.push_back(0xff);
    buffer.push_back(0xda); //< SOS
    buffer.push_back(0); //< length msb
    buffer.push_back(12); //< length lsb
    buffer.push_back(3); //< 3 components
    buffer.push_back(1); //< comp 1
    buffer.push_back(0); //< huffman table 0
    buffer.push_back(2); //< comp 2
    buffer.push_back(0x11); //< huffman table 1
    buffer.push_back(3); //< comp 3
    buffer.push_back(0x11); //< huffman table 1
    buffer.push_back(0); //< first DCT coeff
    buffer.push_back(63); //< last DCT coeff
    buffer.push_back(0); //< successive approx.
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
void MjpegParser::makeHeaders(
    int type, int width, int height, const quint8* lqt, const quint8* cqt, unsigned short dri)
{
    m_hdrBuffer.clear();
    m_hdrBuffer.reserve(1024);

    // Convert from blocks to pixels.
    width <<= 3;
    height <<= 3;

    appendSOI(m_hdrBuffer);
    m_lumaTablePos = m_hdrBuffer.size() + 5;
    appendDQT(m_hdrBuffer, lqt, 0);
    m_chromaTablePos = m_hdrBuffer.size() + 5;
    appendDQT(m_hdrBuffer, cqt, 1);

    if (dri != 0)
        appendDRI(m_hdrBuffer, dri);

    if (!m_onvifJpegHeader.empty())
    {
        m_hdrBuffer.insert(m_hdrBuffer.end(), m_onvifJpegHeader.begin(), m_onvifJpegHeader.end());
        if (m_sofFound) // SOF already in onvif jpeg extension.
            return;
    }
    appendSOF(m_hdrBuffer, type, width, height);

    appendDHT(m_hdrBuffer, lum_dc_codelens,
        sizeof(lum_dc_codelens),
        lum_dc_symbols,
        sizeof(lum_dc_symbols), 0, 0);
    appendDHT(m_hdrBuffer, lum_ac_codelens,
        sizeof(lum_ac_codelens),
        lum_ac_symbols,
        sizeof(lum_ac_symbols), 0, 1);
    appendDHT(m_hdrBuffer, chm_dc_codelens,
        sizeof(chm_dc_codelens),
        chm_dc_symbols,
        sizeof(chm_dc_symbols), 1, 0);
    appendDHT(m_hdrBuffer, chm_ac_codelens,
        sizeof(chm_ac_codelens),
        chm_ac_symbols,
        sizeof(chm_ac_symbols), 1, 1);

    appendSOS(m_hdrBuffer);
}

//-------------------------------------------------------------------------------------------------

MjpegParser::MjpegParser()
{
    StreamParser::setFrequency(90'000);
    memset(m_lumaTable, 0, sizeof(m_lumaTable));
    memset(m_chromaTable, 0, sizeof(m_chromaTable));
    m_frameWidth = 0;
    m_frameHeight = 0;
    m_lastJpegQ = -1;

    m_hdrQ = -1;
    m_hdrDri = 0;
    m_hdrWidth = -1;
    m_hdrHeight = -1;
    m_lumaTablePos = 0;
    m_chromaTablePos = 0;
    m_frameSize = 0;
}

void MjpegParser::setConfiguredResolution(int width, int height)
{
    m_configuredWidth = width;
    m_configuredHeight = height;
}

void MjpegParser::setSdpInfo(const Sdp::Media& sdp)
{
    for (int i = 0; i < sdp.sdpAttributes.size(); ++i)
    {
        QString line = sdp.sdpAttributes[i].trimmed().toLower();
        if (line.startsWith("a=framesize:"))
        {
            const QStringList& values = line.split(' ');
            if (values.size() > 1)
            {
                const QStringList& dimension = values[1].split('-');
                if (dimension.size() == 2)
                {
                    m_frameWidth = dimension[0].trimmed().toInt();
                    m_frameHeight = dimension[1].trimmed().toInt();
                }
            }
        }
        else if (line.startsWith("a=x-dimensions:"))
        {
            const QStringList& values = line.split(':');
            if (values.size() > 1)
            {
                const QStringList& dimension = values[1].split(',');
                if (dimension.size() == 2)
                {
                    m_frameWidth = dimension[0].trimmed().toInt();
                    m_frameHeight = dimension[1].trimmed().toInt();
                }
            }
        }
    }
}

Result MjpegParser::processRtpExtension(
    const RtpHeaderExtensionHeader& extensionHeader, quint8* data, int size)
{
    constexpr uint16_t kOnvifJpegExtensionCode = 0xffd8;
    if (extensionHeader.id() == kOnvifJpegExtensionCode)
    {
        // Remove padding
        while (size > 0 && data[size - 1] == 0xff)
            --size;

        m_onvifJpegHeader.resize(size);
        if (size > 0)
            memcpy(m_onvifJpegHeader.data(), data, size);

        m_sofFound = false;
        nx::media::jpeg::ImageInfo imgInfo;
        if (nx::media::jpeg::readJpegImageInfo(data, size, &imgInfo))
        {
            m_frameHeight = imgInfo.height;
            m_frameWidth = imgInfo.width;
            m_sofFound = true;
        }
    }
    return { true };
}

void MjpegParser::fixResolution(int* width, int* height)
{
    if (*width == 0)
        *width += 256;

    if (*height == 0)
        *height += 256;

    if (m_configuredWidth == *width * 8 + 2048)
        *width = m_configuredWidth / 8;

    if (m_configuredHeight == *height * 8 + 2048)
        *height = m_configuredHeight / 8;

    if (m_frameWidth > 0)
        *width = m_frameWidth / 8;
    if (m_frameHeight > 0)
        *height = m_frameHeight / 8;

    // Workaround: certain 4K cameras do not provide "a=framesize", and drop MSB from resolution.
    if (m_frameWidth <= 0 && m_frameHeight <= 0
        && *width == (3840 - 2048) / 8 && *height == (2160 - 2048) / 8)
    {
        if (!resolutionWorkaroundLogged)
        {
            resolutionWorkaroundLogged = true;
            NX_DEBUG(this,
                "[mjpeg_rtp_parser] Camera reports resolution 1792 x 112, assuming 3840 x 2160 (~4K)");
        }
        *width = 3840 / 8;
        *height = 2160 / 8;
    }
}

Result MjpegParser::processData(
        const RtpHeader& rtpHeader,
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData)
{
    gotData = false;
    const quint8* curPtr = rtpBufferBase + bufferOffset;

    static quint8 jpeg_end[2] = {0xff, 0xd9};

    int bytesLeft = bytesRead;

    if (bytesLeft < 8)
        return {false, NX_FMT("Not enough bytes left in buffer. %1 bytes", bytesLeft)};

    // 1. jpeg main header

    [[maybe_unused]] int typeSpecific = *curPtr++;
    int fragmentOffset = (curPtr[0] << 16) + (curPtr[1] << 8) + curPtr[2];
    curPtr += 3;
    int jpegType = *curPtr++;
    int jpegQ = *curPtr++;
    int width = *curPtr++;
    int height = *curPtr++;

    fixResolution(&width, &height);

    bytesLeft -= 8;

    quint16 dri = 0;
    // 2. restart marker header (optional)
    if (jpegType & 0x40)
    {
        if (bytesLeft < 4)
            return {false, NX_FMT("Not enough bytes left in buffer. %1 bytes", bytesLeft)};
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
                return {false, NX_FMT("Not enough bytes left in buffer. %1 bytes", bytesLeft)};

            [[maybe_unused]] quint8 MBZ = *curPtr++;
            quint8 Precision = *curPtr++;
            quint16 length = (curPtr[0] << 8) + curPtr[1];
            curPtr += 2;
            bytesLeft -= 4;

            if (bytesLeft < length)
            {
                return {false, NX_FMT("Not enough bytes left in buffer. "
                    "%1 bytes, expected at least %2 bytes", bytesLeft, length)};
            }

            int lumaSize = (Precision & 1) ? 2 : 1;
            int chromaSize = (Precision & 2) ? 2 : 1;

            if (lumaSize != 1 || chromaSize != 1)
            {
                if (!mjpeg16BitWarningLogged)
                {
                    mjpeg16BitWarningLogged = true;
                    NX_DEBUG(this, lit("16-bit MJPEG is not supported"));
                }
                return {false, "Only 8-bit MJPEG is supported"};
            }

            if (length > 0)
            {
                static const int kTableSize = 64;
                if (length < kTableSize)
                {
                    return {false, NX_FMT("Invalid color table length %1. "
                        "Expected at least %2", length, kTableSize)};
                }
                // RFC2435. Same table for each frame, make deep copy of tables.
                lumaTable = curPtr;
                chromaTable = curPtr + (length >= kTableSize*2 ? kTableSize : 0);
                if (jpegQ != 255)
                {
                    // Make deep copy because the same table will be reused.
                    memcpy(m_lumaTable, lumaTable, kTableSize);
                    memcpy(m_chromaTable, chromaTable, kTableSize);
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
           !m_onvifJpegHeader.empty())
        {
            if (jpegQ != 255)
            {
                makeHeaders(jpegType, width, height, m_lumaTable, m_chromaTable, dri); //< use deep copy
            }
            else
            {
                if (!lumaTable || !chromaTable)
                    return {false, NX_FMT("Missing luma or chroma table")};

                makeHeaders(jpegType, width, height, lumaTable, chromaTable, dri); //< use tables direct pointer
            }
            m_hdrQ = jpegQ;
            m_hdrDri = dri;
            m_hdrWidth = width;
            m_hdrHeight = height;
        }
        else if (lumaTable && chromaTable)
        {
            // Update header tables.
            if ((int)m_hdrBuffer.size() > m_lumaTablePos + 64)
                memcpy(m_hdrBuffer.data() + m_lumaTablePos, lumaTable, 64);

            if ((int)m_hdrBuffer.size() > m_chromaTablePos + 64)
                memcpy(m_hdrBuffer.data() + m_chromaTablePos, chromaTable, 64);
        }

        m_chunks.clear();
        m_frameSize = 0;
    }

    // Check buffer overflow
    if (m_frameSize + bytesLeft > MAX_ALLOWED_FRAME_SIZE)
    {
        m_chunks.clear();
        m_frameSize = 0;
        return {false, "RTP MJPEG parser buffer overflow"};
    }
    m_chunks.emplace_back(Chunk{int(curPtr - rtpBufferBase), bytesLeft});
    m_frameSize += bytesLeft;

    bool lastPacketReceived = rtpHeader.marker;
    if (lastPacketReceived)
    {
        const quint8* EOI_marker = curPtr + bytesLeft - 2;
        //if (m_frameSize < 2 || EOI_marker[0] != jpeg_end[0] || EOI_marker[1] != jpeg_end[1])
        //    m_frameData.write((const char*) jpeg_end, sizeof(jpeg_end));
        bool needAddMarker =
            m_frameSize < 2 || EOI_marker[0] != jpeg_end[0] || EOI_marker[1] != jpeg_end[1];

        QnWritableCompressedVideoDataPtr videoData(new QnWritableCompressedVideoData(
            m_hdrBuffer.size() + m_frameSize + (needAddMarker ? 2 : 0)));
        m_mediaData = videoData;
        videoData->m_data.uncheckedWrite((const char*)m_hdrBuffer.data(), m_hdrBuffer.size());
        //m_videoData->data.write(m_frameData);
        for (uint i = 0; i < m_chunks.size(); ++i)
        {
            videoData->m_data.uncheckedWrite(
                (const char*)(rtpBufferBase + m_chunks[i].offset), m_chunks[i].size);
        }
        if (needAddMarker)
            videoData->m_data.uncheckedWrite((const char*)jpeg_end, sizeof(jpeg_end));

        m_chunks.clear();
        m_frameSize = 0;

        videoData->channelNumber = 0;
        videoData->flags |= QnAbstractMediaData::MediaFlags_AVKey;
        videoData->compressionType = AV_CODEC_ID_MJPEG;
        videoData->width = width * 8;
        videoData->height = height * 8;

        videoData->timestamp = rtpHeader.getTimestamp();

        auto codecParameters = QnFfmpegHelper::createVideoCodecParametersAnnexB(videoData.get());
        if (codecParameters && (!m_codecParameters || !m_codecParameters->isEqual(*codecParameters)))
            m_codecParameters = codecParameters;
        videoData->context = m_codecParameters;

        gotData = true;
    }
    return {true};
}

} // namespace nx::rtp
