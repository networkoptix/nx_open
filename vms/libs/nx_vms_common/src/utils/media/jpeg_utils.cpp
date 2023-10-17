// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "jpeg_utils.h"

#include <nx/utils/log/assert.h>
#include <utils/media/bitStream.h>

namespace nx::media::jpeg
{

static const int MARKER_HEADER_SIZE = 2;
static const int MARKER_LENGTH_SIZE = 2;

struct Component
{
    uint8_t id = 0;
    uint8_t vCount = 0;
    uint8_t hCount = 0;
    uint8_t quantIndex = 0;
};

AVPixelFormat convertGbr(uint32_t pixFmtMask, uint8_t bits)
{
    switch (pixFmtMask)
    {
        case 0x11111100:
            return bits <= 8 ? AV_PIX_FMT_GBRP : AV_PIX_FMT_GBRP16;

        case 0x12111100:
        case 0x14121200:
        case 0x14111100:
        case 0x22211100:
        case 0x22112100:
        case 0x21111100:
            return bits <= 8 ? AV_PIX_FMT_GBRP : AV_PIX_FMT_NONE;
    }
    return AV_PIX_FMT_NONE;
}

AVPixelFormat convertYuv(uint32_t pixFmtMask, uint8_t bits)
{
    switch (pixFmtMask)
    {
        case 0x11111100:
        {
            if (bits <= 8)
                return bits <= 8 ? AV_PIX_FMT_YUV444P : AV_PIX_FMT_YUV444P16;
        }
        [[fallthrough]];
        case 0x12121100:
        case 0x22122100:
        case 0x21211100:
        case 0x22211200:
        case 0x22221100:
        case 0x22112200:
        case 0x11222200:
        case 0x31111100:
            return bits <= 8 ? AV_PIX_FMT_YUV444P : AV_PIX_FMT_NONE;

        case 0x12111100:
        case 0x14121200:
        case 0x14111100:
        case 0x22211100:
        case 0x22112100:
            return bits <= 8 ? AV_PIX_FMT_YUV440P : AV_PIX_FMT_NONE;

        case 0x21111100:
            return bits <= 8 ? AV_PIX_FMT_YUV422P : AV_PIX_FMT_YUV422P16;

        case 0x22121100:
        case 0x22111200:
            return bits <= 8 ? AV_PIX_FMT_YUV422P : AV_PIX_FMT_NONE;

        case 0x22111100:
        case 0x42111100:
        case 0x24111100:
        {
            if (pixFmtMask == 0x42111100 || pixFmtMask == 0x24111100)
            {
                if (bits > 8)
                    return AV_PIX_FMT_NONE;
            }
            return bits <= 8 ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_YUV420P16;
        }
        case 0x41111100:
            return bits <= 8 ? AV_PIX_FMT_YUV411P : AV_PIX_FMT_NONE;

        default:
            return AV_PIX_FMT_NONE;
    }
}

// Parsing of SOF JPEG header to obtain pixel format
AVPixelFormat parsePixelFormatFromSof(const quint8* data, size_t size)
{
    if (size <= 4)
        return AV_PIX_FMT_NONE;

    uint8_t bits = 0;
    uint8_t numberComponents = 0;
    Component components[4];
    try
    {
        BitStreamReader reader(data + 4, size - 4);
        bits = reader.getBits(8);
        reader.skipBits(32); // height and width
        numberComponents = reader.getBits(8);
        if (numberComponents > 4)
            return AV_PIX_FMT_NONE;

        for (uint8_t i = 0; i < numberComponents; i++)
        {

            components[i].id = reader.getBits(8) - 1;
            components[i].hCount = reader.getBits(4);
            components[i].vCount = reader.getBits(4);
            components[i].quantIndex = reader.getBits(8);
            if (components[i].quantIndex >= 4)
                return AV_PIX_FMT_NONE;
            if (!components[i].hCount || !components[i].vCount)
                return AV_PIX_FMT_NONE;
        }
    }
    catch (const BitStreamException&)
    {
        return AV_PIX_FMT_NONE;
    }

    if (numberComponents == 1)
        return bits <= 8 ? AV_PIX_FMT_GRAY8 : AV_PIX_FMT_GRAY16;

    if (numberComponents != 3) // not impelemented
        return AV_PIX_FMT_NONE;

    // below obfuscated code from ffmpeg, need to be simplified
    uint32_t pixFmtMask =
        ((unsigned)components[0].hCount << 28) | (components[0].vCount << 24) |
        (components[1].hCount << 20) | (components[1].vCount << 16) |
        (components[2].hCount << 12) | (components[2].vCount <<  8) |
        (components[3].hCount <<  4) |  components[3].vCount;
    /* NOTE we do not allocate pictures large enough for the possible
     * padding of h/v_count being 4 */
    if (!(pixFmtMask & 0xD0D0D0D0))
        pixFmtMask -= (pixFmtMask & 0xF0F0F0F0) >> 1;
    if (!(pixFmtMask & 0x0D0D0D0D))
        pixFmtMask -= (pixFmtMask & 0x0F0F0F0F) >> 1;

    if (components[0].id == 'Q' && components[1].id == 'F' && components[2].id == 'A')
        return convertGbr(pixFmtMask, bits);

    return convertYuv(pixFmtMask, bits);
}

QString toString(ResultCode resCode)
{
    switch (resCode)
    {
        case succeeded:
            return "succeeded";
        case interrupted:
            return "interrupted";
        case badData:
            return "badData";
        case failed:
            return "failed";
        default:
            return "unknownError";
    }
}

static bool markerHasBody(int markerType)
{
    return !((markerType >= 0xD0 && markerType <= 0xD9) //RSTn, SOI, EOI
        || (markerType == 0x01)); //TEM
}

JpegParser::JpegParser():
    m_state(waitingStartOfMarker),
    m_currentMarkerCode(0)
{}

ResultCode JpegParser::parse(const quint8* data, size_t size)
{
    const quint8* dataEnd = data + size;
    for (const quint8* pos = data; pos < dataEnd;)
    {
        switch(m_state)
        {
            case waitingStartOfMarker:
                if (*pos == 0xff)
                    m_state = readingMarkerCode;
                ++pos;
                continue;

            case readingMarkerCode:
                m_currentMarkerCode = *pos;
                ++pos;
                if (markerHasBody(m_currentMarkerCode))
                {
                    m_state = readingMarkerLength;
                }
                else
                {
                    if (m_parseHandler)
                        if (!m_parseHandler(m_currentMarkerCode, 0, pos - data - MARKER_HEADER_SIZE))
                        {
                            if (m_errorHandler)
                                m_errorHandler(interrupted, pos - data - MARKER_HEADER_SIZE);
                            return interrupted;
                        }
                    m_state = waitingStartOfMarker;
                }
                continue;

            case readingMarkerLength:
            {
                if (pos + 1 >= dataEnd)
                {
                    if (m_errorHandler)
                        m_errorHandler(badData, pos - data - MARKER_HEADER_SIZE);
                    return badData;   //error
                }
                const size_t markerLength = (*pos << 8) | *(pos+1);
                if (m_parseHandler)
                    if (!m_parseHandler(m_currentMarkerCode, markerLength, pos - data - MARKER_HEADER_SIZE))
                    {
                        if (m_errorHandler)
                            m_errorHandler(interrupted, pos - data - MARKER_HEADER_SIZE);
                        return interrupted;
                    }
                //skipping marker body
                pos += markerLength;
                m_state = waitingStartOfMarker;
                break;
            }

            default:
                return failed;
        }
    }
    return succeeded;
}

bool readJpegImageInfo(const quint8* data, size_t size, ImageInfo* const imgInfo)
{
    bool parsed = false;
    auto parseHandler = [&parsed, data, size, imgInfo]( int markerType, size_t markerLength, size_t currentOffset ) -> bool
    {
        static const size_t MIN_SOF_SIZE = 5;
        if( !(markerType == 0xC0 || markerType == 0xC2) )
            return true;   //not SOF marker, continue parsing
        if( (markerLength < MARKER_LENGTH_SIZE + MIN_SOF_SIZE) ||       //marker size not enough to read data. marker not recognized?
            (currentOffset + MARKER_HEADER_SIZE + MARKER_LENGTH_SIZE + MIN_SOF_SIZE >= size) )  //not enough data in source buffer
        {
            return false;
        }
        const quint8* sofDataStart = data + currentOffset + MARKER_HEADER_SIZE + MARKER_LENGTH_SIZE;
        imgInfo->precision = sofDataStart[0];
        imgInfo->height = (sofDataStart[1] << 8) | sofDataStart[2];
        imgInfo->width = (sofDataStart[3] << 8) | sofDataStart[4];
        imgInfo->pixelFormat = parsePixelFormatFromSof(data + currentOffset, size);
        parsed = true;
        return false;
    };

    JpegParser parser;
    parser.setParseHandler(parseHandler);
    parser.parse(data, size);
    return parsed;
}

} // namespace nx::media::jpeg
