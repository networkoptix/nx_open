#include "motion_estimation.h"

#include <cmath>

#include <QtGui/QImage>
#include <QtGui/QColor>
#include <QtCore/QDebug>

#include <nx/network/socket_common.h>

#include <utils/media/sse_helper.h>
#include <utils/common/synctime.h>
#include <utils/math/math.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/utils/log/log.h>
#include <nx_vms_server_ini.h>

// TODO: #Elric move to config?
// see https://code.google.com/p/arxlib/source/browse/include/arx/Utility.h
#ifndef _MSC_VER
#   define __forceinline inline
#endif

/* Do not warn about uninitialized variables in this module due to low-level optimization. */
#pragma warning( disable : 4700 )

//#define DEBUG_CPU_MODE
static const unsigned char BitReverseTable256[] =
{
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
    0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
    0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
    0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
    0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
    0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
    0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
    0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
    0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

static int sensitivityToMask[10] =
{
    255, //  0
    26,
    22,
    18,
    16,
    14,
    12,
    11,
    10,
    9, // 9

};

static const int MIN_SQUARE_BY_SENS[10] =
{
    INT_MAX, // motion mask: 0
    sensitivityToMask[1]*68,
    sensitivityToMask[2]*48,
    sensitivityToMask[3]*34,
    sensitivityToMask[4]*22,
    sensitivityToMask[5]*14,
    sensitivityToMask[6]*8,
    sensitivityToMask[7]*4,
    sensitivityToMask[8]*2,
    sensitivityToMask[9]*1, // max sens: 9
};

static quint16 sadTransformMatrix[256*256];
static const int SAD_SCALED_BITS = 2;
//static const double SCALED_MAX_VALUE = 4000;
//static const int ZERR_THRESHOLD = 8;
//static double sad_yPow = log(SCALED_MAX_VALUE-1)/log(255.0);
class SadTransformInit
{

    quint8 transformValue(quint8 value)
    {
        if (value < 9)
            return 0;

        double newValue;
        if (value < 32)
            newValue = std::pow((double)value, 1.40);
        else if (value < 80)
            newValue = std::pow((double)value, 1.41);
        else
            newValue = std::pow((double)value, 1.45);
        /*
        if (value < 7)
            return 0;
        else if (value < 32)
            return value;
        else
            return qMin(255.0, std::pow((double) value, 1.1)+0.5);
        */
        /*
        else if (value < 64)
            return value*1.2+0.5;
        else if (value < 128)
            return value*1.3+0.5;
        else
            return 255;
        */
        //double newValue = std::pow((double)value, sad_yPow);

        quint8 rez = qMin(int(newValue + 0.5) >> SAD_SCALED_BITS, 255);
        return rez;
    }

public:
    SadTransformInit()
    {
#ifdef DEBUG_TRANSFORM
        // debug
        for (int i = 0; i < 256; ++i)
        {
            qDebug() << "transform" << i << "to" << transformValue(i);
        }
#endif
        for (int i = 0; i < 65536; ++i)
        {
            quint8 low = transformValue(i & 0xff);
            quint8 hi = transformValue(i >> 8);
            sadTransformMatrix[i] = low + (hi << 8);
        }
    }
} initSad;



quint32 reverseBits(quint32 v)
{
    return (BitReverseTable256[v & 0xff]) |
        (BitReverseTable256[(v >> 8) & 0xff] << 8) |
        (BitReverseTable256[(v >> 16) & 0xff] << 16) |
        (BitReverseTable256[(v >> 24) & 0xff] << 24);
}

inline bool isMotionAt(const quint8* data, int x, int y)
{
    int shift = x*Qn::kMotionGridHeight + y;
    return data[shift/8] & (128 >> (shift&7));
}

inline void setMotionAt(quint8* data, int x, int y)
{
    int shift = x*Qn::kMotionGridHeight + y;
    data[shift/8] |= (128 >> (shift&7));
}


// for test
void fillFrameRect(CLVideoDecoderOutput* frame, const QRect& rect, quint8 value)
{
    for (int y = rect.top(); y <= rect.bottom(); ++y)
    {
        for (int x = rect.left(); x <= rect.right(); ++x)
        {
            frame->data[0][y*frame->linesize[0] + x] = value;
        }
    }
}

/*
* returns matrix with avarage Y value
*/

void saveFrame(const quint8* data, int width, int height, int linesize, const QString& fileName)
{
    QImage src(width, height, QImage::Format_Indexed8);
    QVector<QRgb> colors;
    for (int i = 0; i < 256; ++i)
        colors << QColor(i,i,i).rgba();
    src.setColorTable(colors);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
            src.setPixel(x,y, (uint) data[linesize*y + x]);
    }
    src.save(fileName);
}

#if defined(__i386) || defined(__amd64) || defined(_WIN32)
static const __m128i sse_0x80_const = _mm_setr_epi32(0x80808080, 0x80808080, 0x80808080, 0x80808080);
static const __m128i zerroConst = _mm_setr_epi32(0, 0, 0, 0);

inline __m128i advanced_sad(const __m128i src1, const __m128i src2)
{

    // get abs difference between frames
    __m128i pixelsAbs = _mm_sub_epi8(_mm_max_epu8(src1, src2), _mm_min_epu8(src1, src2));

    pixelsAbs =_mm_insert_epi16( pixelsAbs, sadTransformMatrix[_mm_extract_epi16(pixelsAbs, 0)], 0);
    pixelsAbs =_mm_insert_epi16( pixelsAbs, sadTransformMatrix[_mm_extract_epi16(pixelsAbs, 1)], 1);
    pixelsAbs =_mm_insert_epi16( pixelsAbs, sadTransformMatrix[_mm_extract_epi16(pixelsAbs, 2)], 2);
    pixelsAbs =_mm_insert_epi16( pixelsAbs, sadTransformMatrix[_mm_extract_epi16(pixelsAbs, 3)], 3);
    pixelsAbs =_mm_insert_epi16( pixelsAbs, sadTransformMatrix[_mm_extract_epi16(pixelsAbs, 4)], 4);
    pixelsAbs =_mm_insert_epi16( pixelsAbs, sadTransformMatrix[_mm_extract_epi16(pixelsAbs, 5)], 5);
    pixelsAbs =_mm_insert_epi16( pixelsAbs, sadTransformMatrix[_mm_extract_epi16(pixelsAbs, 6)], 6);
    pixelsAbs =_mm_insert_epi16( pixelsAbs, sadTransformMatrix[_mm_extract_epi16(pixelsAbs, 7)], 7);

    // return sum
    return _mm_sad_epu8(pixelsAbs, zerroConst);
}
#endif

void getFrame_avgY_array_8_x(const CLVideoDecoderOutput* frame, const CLVideoDecoderOutput* prevFrame, quint8* dst)
{
    //saveFrame(frame->data[0], frame->width, frame->height, frame->linesize[0], "c:/src_orig.bmp");

    //NX_ASSERT(frame->width % 8 == 0);
    const int effectiveWidth = frame->width & (~0x07);

    NX_ASSERT(frame->linesize[0] % 16 == 0);

    const simd128i* curLinePtr = (const simd128i*) frame->data[0];
    const simd128i* curLinePtrPrev = (const simd128i*) prevFrame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = qPower2Ceil((unsigned)effectiveWidth,16)/16;
    int linesInStep = (frame->height*65536)/ Qn::kMotionGridHeight;
    int ySteps = Qn::kMotionGridHeight;

    int curLineNum = 0;
    for (int y = 0; y < ySteps; ++y)
    {
        quint8* dstCurLine = dst;
        int nextLineNum = curLineNum + linesInStep;
        int rowCnt = (nextLineNum>>16) - (curLineNum>>16);
        const simd128i* linePtr = curLinePtr;
        const simd128i* linePtrPrev = curLinePtrPrev;
        for (int x = 0; x < xSteps; ++x)
        {
#if defined(__i386) || defined(__amd64) || defined(_WIN32)
            __m128i blockSum = _mm_setzero_si128();
            const __m128i* src = linePtr;
            const __m128i* srcPrev = linePtrPrev;
            for (int i = 0; i < rowCnt;)
            {
                //__m128i partSum = _mm_sad_epu8(*src, zerroConst); // src 16 bytes sum
                __m128i partSum = advanced_sad(*src, *srcPrev); // src 16 bytes sum // SSE2
                src += lineSize;
                srcPrev += lineSize;
                ++i;
                blockSum = _mm_add_epi32(blockSum, partSum); // SSE2
            }
            linePtr++;
            linePtrPrev++;

            // get avg value
            int pixels = rowCnt * 8;
            *dstCurLine = _mm_cvtsi128_si32(blockSum) / pixels; // SSE2
            dstCurLine[Qn::kMotionGridHeight] = _mm_cvtsi128_si32(_mm_srli_si128(blockSum, 8)) / pixels; // SSE2
            dstCurLine += Qn::kMotionGridHeight*2;
#endif
        }
        curLineNum = nextLineNum;
        curLinePtr += lineSize*rowCnt;
        curLinePtrPrev += lineSize*rowCnt;
        dst++;
    }

    //saveFrame(frame->data[0], effectiveWidth, frame->height, frame->linesize[0], "c:/src_orig.bmp");
}

void getFrame_avgY_array_8_x_mc(const CLVideoDecoderOutput* frame, quint8* dst)
{
    //saveFrame(frame->data[0], frame->width, frame->height, frame->linesize[0], "c:/src_orig.bmp");

    //NX_ASSERT(frame->width % 8 == 0);
    NX_ASSERT(frame->linesize[0] % 16 == 0);

    const simd128i* curLinePtr = (const simd128i*) frame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = qPower2Ceil((unsigned)frame->width,16)/16;
    int linesInStep = (frame->height*65536)/ Qn::kMotionGridHeight;
    int ySteps = Qn::kMotionGridHeight;

    int curLineNum = 0;
    for (int y = 0; y < ySteps; ++y)
    {
        quint8* dstCurLine = dst;
        int nextLineNum = curLineNum + linesInStep;
        int rowCnt = (nextLineNum>>16) - (curLineNum>>16);
        const simd128i* linePtr = curLinePtr;
        for (int x = 0; x < xSteps; ++x)
        {
#if defined(__i386) || defined(__amd64) || defined(_WIN32)
            __m128i blockSum = _mm_setzero_si128();
            const __m128i* src = linePtr;
            for (int i = 0; i < rowCnt;)
            {
                __m128i partSum = _mm_sad_epu8(*src, zerroConst); // src 16 bytes sum // SSE2
                src += lineSize;
                ++i;
                blockSum = _mm_add_epi32(blockSum, partSum); // SSE2
            }
            linePtr++;

            // get avg value
            int pixels = rowCnt * 8;
            *dstCurLine = _mm_cvtsi128_si32(blockSum) / pixels; // SSE2
            dstCurLine[Qn::kMotionGridHeight] = _mm_cvtsi128_si32(_mm_srli_si128(blockSum, 8)) / pixels; // SSE2
            dstCurLine += Qn::kMotionGridHeight*2;
#endif
        }
        curLineNum = nextLineNum;
        curLinePtr += lineSize*rowCnt;
        dst++;
    }

    //saveFrame(frame->data[0], frame->width, frame->height, frame->linesize[0], "c:/src_orig.bmp");
}

void fillRightEdge8(const CLVideoDecoderOutput* frame)
{
    NX_ASSERT(frame->linesize[0] >= frame->width + 8);
    quint64* dataPtr = (quint64*) (frame->data[0]+frame->width);
    for (int i = 0; i < frame->height; ++i) {
        dataPtr[0] = dataPtr[-1]; // replicate most right 8 pixels
        dataPtr += frame->linesize[0]/sizeof(quint64);
    }
}

void getFrame_avgY_array_16_x(const CLVideoDecoderOutput* frame, const CLVideoDecoderOutput* prevFrame, quint8* dst)
{
    //NX_ASSERT(frame->width % 8 == 0);
    NX_ASSERT(frame->linesize[0] % 16 == 0);

    const simd128i* curLinePtr = (const simd128i*) frame->data[0];
    const simd128i* prevLinePtr = (const simd128i*) prevFrame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = qPower2Ceil((unsigned)frame->width,16)/16;
    if (frame->width % 16)
        fillRightEdge8(frame);

    int linesInStep = (frame->height*65536)/ Qn::kMotionGridHeight;
    int ySteps = Qn::kMotionGridHeight;

    int curLineNum = 0;
    for (int y = 0; y < ySteps; ++y)
    {
        quint8* dstCurLine = dst;
        int nextLineNum = curLineNum + linesInStep;
        int rowCnt = (nextLineNum>>16) - (curLineNum>>16);
        const simd128i* linePtr = curLinePtr;
        const simd128i* linePtr2 = prevLinePtr;
        for (int x = 0; x < xSteps; ++x)
        {
#if defined(__i386) || defined(__amd64) || defined(_WIN32)
            __m128i blockSum = _mm_setzero_si128();
            const __m128i* src = linePtr;
            const __m128i* src2 = linePtr2;
            for (int i = 0; i < rowCnt;)
            {
                __m128i partSum = advanced_sad(*src, *src2); // src 16 bytes sum // SSE2
                src += lineSize;
                src2 += lineSize;
                ++i;
                blockSum = _mm_add_epi32(blockSum, partSum); // SSE2
            }
            linePtr++;
            linePtr2++;

            // get avg value
            int pixels = rowCnt * 16;
            *dstCurLine = (_mm_cvtsi128_si32(_mm_srli_si128(blockSum, 8)) + _mm_cvtsi128_si32(blockSum)) / pixels; // SSE2
            dstCurLine += Qn::kMotionGridHeight;
#endif
        }
        curLineNum = nextLineNum;
        curLinePtr += lineSize*rowCnt;
        prevLinePtr += lineSize*rowCnt;
        dst++;
    }
}

void getFrame_avgY_array_16_x_mc(const CLVideoDecoderOutput* frame, quint8* dst)
{
    //NX_ASSERT(frame->width % 8 == 0);
    NX_ASSERT(frame->linesize[0] % 16 == 0);

    const simd128i* curLinePtr = (const simd128i*) frame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = qPower2Ceil((unsigned)frame->width,16)/16;
    int linesInStep = (frame->height*65536)/ Qn::kMotionGridHeight;
    int ySteps = Qn::kMotionGridHeight;

    int curLineNum = 0;
    for (int y = 0; y < ySteps; ++y)
    {
        quint8* dstCurLine = dst;
        int nextLineNum = curLineNum + linesInStep;
        int rowCnt = (nextLineNum>>16) - (curLineNum>>16);
        const simd128i* linePtr = curLinePtr;
        for (int x = 0; x < xSteps; ++x)
        {
#if defined(__i386) || defined(__amd64) || defined(_WIN32)
            __m128i blockSum = _mm_setzero_si128();
            const __m128i* src = linePtr;
            for (int i = 0; i < rowCnt;)
            {
                __m128i partSum = _mm_sad_epu8(*src, zerroConst); // src 16 bytes sum // SSE2
                src += lineSize;
                ++i;
                blockSum = _mm_add_epi32(blockSum, partSum); // SSE2
            }
            linePtr++;

            // get avg value
            int pixels = rowCnt * 16;
            *dstCurLine = (_mm_cvtsi128_si32(_mm_srli_si128(blockSum, 8)) + _mm_cvtsi128_si32(blockSum)) / pixels; // SSE2
            dstCurLine += Qn::kMotionGridHeight;
#endif
        }
        curLineNum = nextLineNum;
        curLinePtr += lineSize*rowCnt;
        dst++;
    }
}

/*
__m128i QnMotionEstimation::advanced_sad_x_x(const __m128i* src1, const __m128i* src2, __m128i* dst, int index)
{
#define TRANSFORM_WORD(ptrBase, wordNum) \
    _mm_insert_epi16( pixelsAbs, \
         m_sadTransformMatrix[m_motionSensMask[ptrBase + wordNum*2]]   [_mm_extract_epi16(pixelsAbs, wordNum) & 0xff] + \
        (m_sadTransformMatrix[m_motionSensMask[ptrBase + wordNum*2+1]] [_mm_extract_epi16(pixelsAbs, wordNum) >> 8] << 8), wordNum);

    // get abs difference between frames
    __m128i pixelsAbs = _mm_sub_epi8(_mm_max_epi8(*src1, *src2), _mm_min_epi8(*src1, *src2));

    // non linear transformation using precomputing table
    TRANSFORM_WORD(index, 0);
    TRANSFORM_WORD(index, 1);
    TRANSFORM_WORD(index, 2);
    TRANSFORM_WORD(index, 3);
    TRANSFORM_WORD(index, 4);
    TRANSFORM_WORD(index, 5);
    TRANSFORM_WORD(index, 6);
    TRANSFORM_WORD(index, 7);

    // return sum
    return _mm_sad_epu8(pixelsAbs, zerroConst);
}
*/

void getFrame_avgY_array_x_x(const CLVideoDecoderOutput* frame, const CLVideoDecoderOutput* prevFrame, quint8* dst, int sqWidth)
{
#define flushData(pixels) \
{\
    *dstCurLine = squareSum / (pixels);\
    squareStep = 0;\
    squareSum = 0;\
    dstCurLine += Qn::kMotionGridHeight;\
}

    NX_ASSERT(frame->linesize[0] % 16 == 0);
    NX_ASSERT(sqWidth % 8 == 0);

    int sqWidthSteps = sqWidth / 8;

    const simd128i* curLinePtr = (const simd128i*) frame->data[0];
    const simd128i* prevLinePtr = (const simd128i*) prevFrame->data[0];
    int lineSize = frame->linesize[0] / 16;
    if (frame->width % 16)
        fillRightEdge8(frame);

    int xSteps = qPower2Ceil((unsigned)frame->width,16)/16;
    int linesInStep = (frame->height*65536)/ Qn::kMotionGridHeight;
    int ySteps = Qn::kMotionGridHeight;

    int squareSum = 0;
    int squareStep = 0;
    int curLineNum = 0;
    for (int y = 0; y < ySteps; ++y)
    {
        quint8* dstCurLine = dst;
        int nextLineNum = curLineNum + linesInStep;
        int rowCnt = (nextLineNum>>16) - (curLineNum>>16);
        const simd128i* linePtr = curLinePtr;
        const simd128i* linePtr2 = prevLinePtr;
        for (int x = 0; x < xSteps; ++x)
        {
#if defined(__i386) || defined(__amd64) || defined(_WIN32)
            __m128i blockSum = _mm_setzero_si128();
            const __m128i* src = linePtr;
            const __m128i* src2 = linePtr2;
            for (int i = 0; i < rowCnt; ++i)
            {
                //__m128i partSum = _mm_sad_epu8(*src, *src2); // src 16 bytes sum // SSE2
                __m128i partSum = advanced_sad(*src, *src2);
                src += lineSize;
                src2 += lineSize;
                blockSum = _mm_add_epi32(blockSum, partSum); // SSE2
            }
            linePtr++;
            linePtr2++;


            // get avg value
            squareSum += _mm_cvtsi128_si32(blockSum); // SSE2
            squareStep++;
            if (squareStep == sqWidthSteps)
                flushData(rowCnt * sqWidth);
            squareSum += _mm_cvtsi128_si32(_mm_srli_si128(blockSum, 8)); // SSE2
#endif

            squareStep++;
            if (squareStep == sqWidthSteps)
                flushData(rowCnt * sqWidth);
        }
        if (squareStep)
            flushData(rowCnt * squareStep*8);
        curLineNum = nextLineNum;
        curLinePtr += lineSize*rowCnt;
        prevLinePtr += lineSize*rowCnt;
        dst++;
    }
#undef flushData
}

void getFrame_avgY_array_x_x_mc(const CLVideoDecoderOutput* frame, quint8* dst, int sqWidth)
{
#define flushData() \
    {\
    int pixels = rowCnt * sqWidth;\
    *dstCurLine = squareSum / pixels;\
    squareStep = 0;\
    squareSum = 0;\
    dstCurLine += Qn::kMotionGridHeight;\
}
    NX_ASSERT(frame->width % 8 == 0);
    NX_ASSERT(frame->linesize[0] % 16 == 0);
    NX_ASSERT(sqWidth % 8 == 0);
    sqWidth /= 8;

    const simd128i* curLinePtr = (const simd128i*) frame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = qPower2Ceil((unsigned)frame->width,16)/16;
    int linesInStep = (frame->height*65536)/ Qn::kMotionGridHeight;
    int ySteps = Qn::kMotionGridHeight;

    int squareSum = 0;
    int squareStep = 0;
    int curLineNum = 0;
    for (int y = 0; y < ySteps; ++y)
    {
        quint8* dstCurLine = dst;
        int nextLineNum = curLineNum + linesInStep;
        int rowCnt = (nextLineNum>>16) - (curLineNum>>16);
        const simd128i* linePtr = curLinePtr;
        for (int x = 0; x < xSteps; ++x)
        {
#if defined(__i386) || defined(__amd64) || defined(_WIN32)
            __m128i blockSum = _mm_setzero_si128();
            const __m128i* src = linePtr;
            for (int i = 0; i < rowCnt;)
            {
                __m128i partSum = _mm_sad_epu8(*src, zerroConst); // src 16 bytes sum // SSE2
                src += lineSize;
                ++i;
                blockSum = _mm_add_epi32(blockSum, partSum); // SSE2
            }
            linePtr++;

            // get avg value
            squareSum += _mm_cvtsi128_si32(blockSum); // SSE2
            squareStep++;
            if (squareStep == sqWidth)
                flushData();
            squareSum += _mm_cvtsi128_si32(_mm_srli_si128(blockSum, 8)); // SSE2
            if (squareStep == sqWidth)
                flushData();
#endif
        }
        if (squareStep)
            flushData();
        curLineNum = nextLineNum;
        curLinePtr += lineSize*rowCnt;
        dst++;
    }
#undef flushData
}


QnMotionEstimation::QnMotionEstimation(const Config& config, nx::metrics::Storage* metrics):
    m_config(config),
    m_channelNum(0),
    m_metrics(metrics)
{
    //m_outFrame.setUseExternalData(false);
    //m_prevFrame.setUseExternalData(false);
    m_frames[0] = CLVideoDecoderOutputPtr(new CLVideoDecoderOutput());
    m_frames[1] = CLVideoDecoderOutputPtr(new CLVideoDecoderOutput());
    m_frames[0]->setUseExternalData(false);
    m_frames[1]->setUseExternalData(false);

    m_scaledMask = 0; // mask scaled to x * Qn::kMotionGridHeight. for internal use
    m_motionSensScaledMask = 0;
    m_frameDeltaBuffer = 0;
    for (int i = 0; i < FRAMES_BUFFER_SIZE; ++i)
        m_frameBuffer[i] = 0;
    m_filteredFrame = 0;
    m_scaledWidth = 0;
    m_xStep = 0; // 8, 16, 24 e.t.c value
    m_lastImgWidth = 0;
    m_lastImgHeight = 0;
    m_motionMask = 0;
    m_motionSensMask = 0;
    m_totalFrames = 0;
    m_resultMotion = 0;
    m_firstFrameTime = AV_NOPTS_VALUE;
    m_lastFrameTime = AV_NOPTS_VALUE;
    m_isNewMask = false;
    m_linkedNums = 0;

    /*m_numFrame = 0;
    m_sumLogTime = 0;*/

#ifdef DEBUG_TRANSFORM
    static int gg = 0;
    if (gg == 0)
    {
        gg = 1;
        SadTransformInit gg;
        int gg2 = 1;
    }
#endif
}

QnMotionEstimation::~QnMotionEstimation()
{
    m_decoder.reset();
    qFreeAligned(m_scaledMask);
    qFreeAligned(m_motionSensScaledMask);
    qFreeAligned(m_frameDeltaBuffer);
    for (int i = 0; i < FRAMES_BUFFER_SIZE; ++i)
        qFreeAligned(m_frameBuffer[i]);
    qFreeAligned(m_filteredFrame);
    qFreeAligned(m_motionMask);
    qFreeAligned(m_motionSensMask);
    qFreeAligned(m_linkedNums);
    delete [] m_resultMotion;
}

// rescale motion mask width (mask rotated, so actually remove or duplicate some lines)
void QnMotionEstimation::scaleMask(quint8* mask, quint8* scaledMask)
{

    float lineStep;
    float scaledLineNum;
    int prevILineNum;

    //std::vector<quint8> test0,test1;
    //test0.resize(m_scaledWidth*Qn::kMotionGridHeight);
    //test1.resize(m_scaledWidth*Qn::kMotionGridHeight);

#if !defined(DEBUG_CPU_MODE) && (defined(__i386) || defined(__amd64) || defined(_WIN32))
    lineStep = (m_scaledWidth-1) / float(Qn::kMotionGridWidth-1);
    scaledLineNum = 0;
    prevILineNum = -1;

    for (int y = 0; y < Qn::kMotionGridWidth; ++y)
    {
        int iLineNum = (int) (scaledLineNum+0.5);
        NX_ASSERT(iLineNum < m_scaledWidth);

        simd128i* dst = (simd128i*) (scaledMask/*&test0[0]*/ + Qn::kMotionGridHeight*iLineNum);
        simd128i* src = (simd128i*) (mask + Qn::kMotionGridHeight*y);
        if (iLineNum > prevILineNum)
        {
            for (int i = 0; i < iLineNum - prevILineNum; ++i)
                memcpy(dst - i*Qn::kMotionGridHeight/sizeof(simd128i) , src, Qn::kMotionGridHeight);
        }
        else
        {
            dst[0] = _mm_min_epu8(dst[0], src[0]); // SSE2
            dst[1] = _mm_min_epu8(dst[1], src[1]); // SSE2
        }

        prevILineNum = iLineNum;
        scaledLineNum += lineStep;
    }
#else
    lineStep = (m_scaledWidth-1) / float(Qn::kMotionGridWidth-1);
    scaledLineNum = 0;
    prevILineNum = -1;

    for (int y = 0; y < Qn::kMotionGridWidth; ++y)
    {
        int iLineNum = (int) (scaledLineNum+0.5);
        NX_ASSERT(iLineNum < m_scaledWidth);

        quint8* dst = scaledMask/*&test1[0]*/ + Qn::kMotionGridHeight*iLineNum;
        quint8* src = mask + Qn::kMotionGridHeight*y;
        if (iLineNum > prevILineNum)
        {
            for (int i = 0; i < iLineNum - prevILineNum; ++i)
                memcpy(dst - i*Qn::kMotionGridHeight , src , Qn::kMotionGridHeight);
        }
        else
        {
            for (int i = 0; i < Qn::kMotionGridHeight; ++i)
                dst[i] = qMin(dst[i], src[i]);
        }

        prevILineNum = iLineNum;
        scaledLineNum += lineStep;
    }
#endif
    /*
    Test to equality algoritms
    int it = 0;
    for (int x = 0; x < m_scaledWidth; ++x)
    {
        for (int y = 0; y < Qn::kMotionGridHeight; ++y)
        {
            if ( test0[it] != test1[it] )
            {
                test0[it] = test1[it];
            };
            it++;
        };
    };*/



    //__m128i* curPtr = (__m128i*) mask;
     //for (int i = 0; i < m_scaledWidth*2; ++i)
     //   curPtr[i] = _mm_sub_epi8(curPtr[i], sse_0x80_const); // SSE2
}

void QnMotionEstimation::reallocateMask(int width, int height)
{
    qFreeAligned(m_scaledMask);
    qFreeAligned(m_motionSensScaledMask);
    qFreeAligned(m_frameDeltaBuffer);
    for (int i = 0; i < FRAMES_BUFFER_SIZE; ++i)
        qFreeAligned(m_frameBuffer[i]);
    qFreeAligned(m_filteredFrame);
    qFreeAligned(m_linkedNums);
    delete [] m_resultMotion;

    m_lastImgWidth = width;
    m_lastImgHeight = height;
    // calculate scaled image size and allocate memory
#if !defined(DEBUG_CPU_MODE) && (defined(__i386) || defined(__amd64) || defined(_WIN32))
    for (m_xStep = 8; m_lastImgWidth/m_xStep > Qn::kMotionGridWidth; m_xStep += 8);
    if (m_lastImgWidth/m_xStep <= (Qn::kMotionGridWidth*3)/4 && m_xStep > 8)
        m_xStep -= 8;
    m_scaledWidth = m_lastImgWidth / m_xStep;
    if (m_lastImgWidth > m_scaledWidth*m_xStep)
        m_scaledWidth++;
#else
    m_scaledWidth = Qn::kMotionGridWidth;
#endif

    int swUp = m_scaledWidth+1; // reserve one extra column because of analyzeFrame() for x8 width can write 1 extra byte
    m_scaledMask = (quint8*) qMallocAligned(Qn::kMotionGridHeight * swUp, 32);
    m_linkedNums = (int*) qMallocAligned(Qn::kMotionGridHeight * swUp * sizeof(int), 32);
    m_motionSensScaledMask = (quint8*) qMallocAligned(Qn::kMotionGridHeight * swUp, 32);
    m_frameDeltaBuffer = (uint8_t*) qMallocAligned(Qn::kMotionGridHeight * swUp, 32);

    m_frameBuffer[0] = (quint8*) qMallocAligned(Qn::kMotionGridHeight * swUp, 32);
    m_frameBuffer[1] = (quint8*) qMallocAligned(Qn::kMotionGridHeight * swUp, 32);
    memset(m_frameBuffer[0], 0, Qn::kMotionGridHeight * swUp);
    memset(m_frameBuffer[1], 0, Qn::kMotionGridHeight * swUp);

    m_filteredFrame = (quint8*) qMallocAligned(Qn::kMotionGridHeight * swUp, 32);
    m_resultMotion = new quint32[Qn::kMotionGridHeight/4 * swUp];
    memset(m_resultMotion, 0, Qn::kMotionGridHeight * swUp);

    // scale motion mask to scaled size
    if (m_scaledWidth != Qn::kMotionGridWidth) {
        scaleMask(m_motionMask, m_scaledMask);
        scaleMask(m_motionSensMask, m_motionSensScaledMask);
    }
    else {
        memcpy(m_scaledMask, m_motionMask, Qn::kMotionGridWidth * Qn::kMotionGridHeight);
        memcpy(m_motionSensScaledMask, m_motionSensMask, Qn::kMotionGridWidth * Qn::kMotionGridHeight);
    }
    m_isNewMask = false;
}

void QnMotionEstimation::analyzeMotionAmount(quint8* frame)
{
    // 1. filtering. If a lot of motion around current pixel, mark it too, also remove motion if diff < mask
    quint8* curPtr = frame;
    quint8* dstPtr = m_filteredFrame;
    const quint8* maskPtr = m_scaledMask;

    for (int y = 0; y < Qn::kMotionGridHeight; ++y) {
        *dstPtr = *curPtr <= *maskPtr ? 0 : *curPtr;
        curPtr++;
        maskPtr++;
        dstPtr++;
    }

    for (int x = 1; x < m_scaledWidth-1; ++x)
    {
        *dstPtr = *curPtr <= *maskPtr ? 0 : *curPtr;
        curPtr++;
        maskPtr++;
        dstPtr++;
        for (int y = 1; y < Qn::kMotionGridHeight-1; ++y)
        {
            if (*curPtr <= *maskPtr)
            {
                int aroundMotionAmount =(int(curPtr[-1] > maskPtr[-1]) +
                                         int(curPtr[1]  > maskPtr[-1]) +
                                         int(curPtr[-Qn::kMotionGridHeight] >maskPtr[-Qn::kMotionGridHeight]) +
                                         int(curPtr[Qn::kMotionGridHeight] >maskPtr[Qn::kMotionGridHeight]))*2 +
                                         int(curPtr[-1-Qn::kMotionGridHeight] >maskPtr[-1-Qn::kMotionGridHeight]) +
                                         int(curPtr[-1+Qn::kMotionGridHeight] >maskPtr[-1+Qn::kMotionGridHeight]) +
                                         int(curPtr[1-Qn::kMotionGridHeight] >maskPtr[1-Qn::kMotionGridHeight]) +
                                         int(curPtr[1+Qn::kMotionGridHeight] >maskPtr[1+Qn::kMotionGridHeight]);
                if (aroundMotionAmount >= 6)
                    *dstPtr = *maskPtr;
                else
                    *dstPtr = 0;
            }
            else {
                *dstPtr = *curPtr;
            }

            curPtr++;
            maskPtr++;
            dstPtr++;
        }
        *dstPtr = *curPtr <= *maskPtr ? 0 : *curPtr;
        curPtr++;
        maskPtr++;
        dstPtr++;
    }

    for (int y = 0; y < Qn::kMotionGridHeight; ++y) {
        *dstPtr = *curPtr <= *maskPtr ? 0 : *curPtr;
        curPtr++;
        maskPtr++;
        dstPtr++;
    }

    // 2. Determine linked areas
    int currentLinkIndex = 1;
    memset(m_linkedNums, 0, sizeof(int) * Qn::kMotionGridHeight * m_scaledWidth);
    memset(m_linkedSquare, 0, sizeof(m_linkedSquare));
    for (int i = 0; i < Qn::kMotionGridHeight*m_scaledWidth/2;++i)
        m_linkedMap[i] = i;

    int idx = 0;
    // 2.1 top left pixel
    if (m_filteredFrame[idx++])
        *m_linkedNums = currentLinkIndex++;

    // 2.2 left col
    for (int y = 1; y < Qn::kMotionGridHeight; ++y, ++idx) {
        if (m_filteredFrame[idx]) {
            if (m_linkedNums[idx-1])
                m_linkedNums[idx] = m_linkedNums[idx-1];
            else
                m_linkedNums[idx] = currentLinkIndex++;
        }
    }

    for (int x = 1; x < m_scaledWidth; ++x)
    {
        // 2.3 top pixel
        if (m_filteredFrame[idx])
        {
            if (m_linkedNums[idx-Qn::kMotionGridHeight])
                m_linkedNums[idx] = m_linkedNums[idx-Qn::kMotionGridHeight];
            else
                m_linkedNums[idx] = currentLinkIndex++;
        }
        idx++;
        // 2.4 all other pixels
        for (int y = 1; y < Qn::kMotionGridHeight; ++y, ++idx)
        {
            if (m_filteredFrame[idx])
            {
                if (m_linkedNums[idx-1]) {
                    m_linkedNums[idx] = m_linkedNums[idx-1];
                    if (m_linkedNums[idx-Qn::kMotionGridHeight] && m_linkedNums[idx-Qn::kMotionGridHeight] != m_linkedNums[idx])
                    {
                        if (m_linkedNums[idx-Qn::kMotionGridHeight] < m_linkedNums[idx])
                            m_linkedMap[m_linkedNums[idx]] = m_linkedNums[idx-Qn::kMotionGridHeight];
                        else
                            m_linkedMap[m_linkedNums[idx-Qn::kMotionGridHeight]] = m_linkedNums[idx];
                    }
                }
                else if (m_linkedNums[idx-Qn::kMotionGridHeight]) {
                    m_linkedNums[idx] = m_linkedNums[idx-Qn::kMotionGridHeight];
                }
#ifndef DISABLE_8_WAY_AREA
                else if (m_linkedNums[idx-1-Qn::kMotionGridHeight]) {
                    m_linkedNums[idx] = m_linkedNums[idx-1-Qn::kMotionGridHeight];
                }
#endif
                else {
                    m_linkedNums[idx] = currentLinkIndex++;
                }
            }
        }
    }

    // 3. path compression
    m_linkedMap[0] = 0;
    for (int i = 1; i < currentLinkIndex; ++i)
    {
        if (m_linkedMap[m_linkedMap[i]])
            m_linkedMap[i] = m_linkedMap[m_linkedMap[i]];
    }

    // 4. merge linked areas
    for (int i = 0; i < Qn::kMotionGridHeight*m_scaledWidth; ++i)
        m_linkedNums[i] = m_linkedMap[m_linkedNums[i]];

    // 5. calculate square of each area
    for (int i = 0; i < Qn::kMotionGridHeight*m_scaledWidth; ++i)
    {
        m_linkedSquare[m_linkedNums[i]] += m_filteredFrame[i];
    }

#if 0
    for (int i = 1; i < currentLinkIndex; ++i)
    {
        if (m_linkedSquare[i] >= 20 && m_linkedSquare[i] <= 50)
        {
            // print square.
            qDebug() << "-determine square size" << m_linkedSquare[i];
            for (int k = 0; k < Qn::kMotionGridHeight*m_scaledWidth; ++k)
            {
                if (m_linkedNums[k] == i)
                    qDebug() << "---- square data at" << k/Qn::kMotionGridHeight << 'x' << k%Qn::kMotionGridHeight << "=" << m_filteredFrame[k];
            }
        }
    }
#endif
    // 6. remove motion if motion square is not enough and write result to bitarray
    for (int i = 0; i < Qn::kMotionGridHeight*m_scaledWidth;)
    {
        quint32 data = 0;
        for (int k = 0; k < 32; ++k, ++i)
        {
            data = (data << 1) + int(m_linkedSquare[m_linkedNums[i]] > MIN_SQUARE_BY_SENS[m_motionSensScaledMask[i]]);
            //data = (data << 1) + (int)(m_filteredFrame[i] >= m_scaledMask[i]);
        }
        m_resultMotion[i/32-1] |= htonl(data);
    }
}

__forceinline int8_t fastAbs( int8_t value )
{
    uint8_t temp = value >> 7;     // make a mask of the sign bit
    value ^= temp;                   // toggle the bits if value is negative
    value += temp & 1;               // add one if value was negative
    return value;
};

void QnMotionEstimation::scaleFrame(const uint8_t* data, int width, int height, int stride, uint8_t* frameBuffer,
                                     uint8_t* prevFrameBuffer, uint8_t* deltaBuffer)
{
    //saveFrame(data, width, height, stride, QString::fromUtf8("c:/src_orig.bmp"));

    // scale frame to m_scaledWidth* Qn::kMotionGridHeight and rotate it to 90 degree
    uint8_t* dst = frameBuffer;
    const uint8_t* src = data;
    int offset = 0;
    int curLine = 32768;
    while ((curLine >> 16) < height)
    {
        int nextLine = curLine + m_scaleYStep;
        int lines = (nextLine >> 16) - (curLine >> 16);
        uint8_t* curDst = dst;
        int curX = 32768;
        //for (int x = 0; x < xMax; ++x)
        while ((curX >> 16) < width)
        {
            int nextX = curX + m_scaleXStep;
            int xPixels = (nextX >> 16) - (curX >> 16);
            // aggregate pixel
            uint16_t pixel = 0;
            const uint8_t* srcPtr = src;
            for (int y = 0; y < lines; ++y)
            {
                for (int rep = 0; rep < xPixels; ++rep)
                    pixel += *srcPtr++;
                srcPtr += stride - xPixels; // go to next src line
            }
            pixel /= (lines*xPixels); // get avg value
            *curDst = pixel;
            int8_t value = (int8_t)pixel - (int8_t)prevFrameBuffer[offset];
            uint8_t pixelDelta = abs(value);
            //qDebug()<<(int16_t)value<<" "<< pixelDelta<<" "<<abs(value);

            deltaBuffer[offset] = pixelDelta;

            src += xPixels;
            curDst += Qn::kMotionGridHeight;
            offset += Qn::kMotionGridHeight;
            curX = nextX;
        }
        src += lines * stride - width;
        dst++;
        offset = offset - (Qn::kMotionGridHeight*Qn::kMotionGridWidth) + 1;
        curLine = nextLine;
    }
}

CLConstVideoDecoderOutputPtr QnMotionEstimation::decodeFrame(const QnCompressedVideoDataPtr& frame)
{
    QnMutexLocker lock(&m_mutex);
    CLVideoDecoderOutputPtr videoDecoderOutput(new CLVideoDecoderOutput());

    // TODO: This always makes a deep copy of the frame, which is helpful to supply the frame to
    // metadata plugins. To optimize, rewrite video decoding via AbstractVideoDecoder: the frame
    // will use its internal reference counting.
    videoDecoderOutput->setUseExternalData(true);

    if (!m_decoder && !(frame->flags & AV_PKT_FLAG_KEY))
        return CLConstVideoDecoderOutputPtr{nullptr};
    if (!m_decoder || m_decoder->getContext()->codec_id != frame->compressionType)
    {
        NX_VERBOSE(this, "Recreating decoder, old codec_id: %1",
            !m_decoder ? -1 : m_decoder->getContext()->codec_id);
        m_decoder = std::make_unique<QnFfmpegVideoDecoder>(
            m_config.decoderConfig, m_metrics, frame);
    }

    m_decoder->getContext()->flags &= ~CODEC_FLAG_GRAY; //< Turn off Y-only mode.

    if (!m_decoder->decode(frame, &videoDecoderOutput))
        return CLConstVideoDecoderOutputPtr{nullptr};

    return videoDecoderOutput;
}

bool QnMotionEstimation::analyzeFrame(const QnCompressedVideoDataPtr& frame,
    CLConstVideoDecoderOutputPtr* outVideoDecoderOutput)
{
    QnMutexLocker lock(&m_mutex);

    if (!m_decoder && !(frame->flags & AV_PKT_FLAG_KEY))
        return false;
    if( !m_motionMask )
        return false; //< No motion mask set.
    if (!m_decoder || m_decoder->getContext()->codec_id != frame->compressionType)
    {
        m_decoder = std::make_unique<QnFfmpegVideoDecoder>(
            m_config.decoderConfig, m_metrics, frame);
    }

    // Turn on Y-only mode if the decoded frame was not requested from this function.
    if (outVideoDecoderOutput)
        m_decoder->getContext()->flags &= ~CODEC_FLAG_GRAY;
    else
        m_decoder->getContext()->flags |= CODEC_FLAG_GRAY;

    int idx = m_totalFrames % FRAMES_BUFFER_SIZE;
    int prevIdx = (m_totalFrames-1) % FRAMES_BUFFER_SIZE;

    if (!m_decoder->decode(frame, &m_frames[idx]))
        return false;
    if (outVideoDecoderOutput)
        *outVideoDecoderOutput = m_frames[idx];
    if (m_frames[idx]->width < Qn::kMotionGridWidth
        || m_frames[idx]->height < Qn::kMotionGridHeight)
    {
        return false;
    }
    m_videoResolution.setWidth(m_frames[idx]->width);
    m_videoResolution.setHeight(m_frames[idx]->height);
    if (m_firstFrameTime == qint64(AV_NOPTS_VALUE))
        m_firstFrameTime = m_frames[idx]->pkt_dts;
    m_lastFrameTime = m_frames[idx]->pkt_dts;
    m_firstFrameTime = qMin(m_firstFrameTime, m_lastFrameTime); //< protection if time goes to past

#if 0
    // test
    fillFrameRect(m_frames[idx], QRect(0,0,m_frames[idx]->width, m_frames[idx]->height), 225);
    if (m_totalFrames % 2 == 1)
    {
        fillFrameRect(m_frames[idx], QRect(QPoint(0, 20), QPoint(m_frames[idx]->width, 32)), 20);
        fillFrameRect(m_frames[idx],
            QRect(
                QPoint(0, m_frames[idx]->height/2),
                QPoint(m_frames[idx]->width, m_frames[idx]->height/2+8)),
            20);

        fillFrameRect(m_frames[idx],
            QRect(
                QPoint(0, m_frames[idx]->height/2-8),
                QPoint(m_frames[idx]->width/4, m_frames[idx]->height/2)),
            20);
        fillFrameRect(m_frames[idx],
            QRect(
                QPoint(m_frames[idx]->width/2, m_frames[idx]->height/2-24),
                QPoint(m_frames[idx]->width*3/4, m_frames[idx]->height/2)),
            20);
    }
    //else
    //    fillFrameRect(m_frames[idx],
    //        QRect(
    //            QPoint(0, m_frames[idx]->height/2),
    //            QPoint(m_frames[idx]->width, m_frames[idx]->height)),
    //        40);
#endif

    if (m_frames[idx]->width != m_lastImgWidth
        || m_frames[idx]->height != m_lastImgHeight
        || m_isNewMask)
    {
        reallocateMask(m_frames[idx]->width, m_frames[idx]->height);
        m_scaleXStep = m_lastImgWidth * 65536 / Qn::kMotionGridWidth;
        m_scaleYStep = m_lastImgHeight * 65536 / Qn::kMotionGridHeight;
        m_totalFrames = 0;
    }


#if 0
    LARGE_INTEGER timer;
    double PCFreq = 0.0;
    if (QueryPerformanceFrequency(&timer))
    {
        PCFreq = (double) timer.QuadPart / 1000.0;
        //qDebug() << "start" << m_numFrame;
    }
    QueryPerformanceCounter(&timer);
#endif // 0

    #if !defined(DEBUG_CPU_MODE) && (defined(__i386) || defined(__amd64) || defined(_WIN32))
        // Do not calculate motion if resolution just changed.
        if (m_frames[0]->width == m_frames[1]->width
            && m_frames[0]->height == m_frames[1]->height
            && m_frames[0]->format == m_frames[1]->format
            && m_frames[idx]->width >= Qn::kMotionGridWidth
            && m_frames[idx]->height >= Qn::kMotionGridHeight)
        {
            // Calculate difference between frames.
            if (m_xStep == 8)
            {
                getFrame_avgY_array_8_x(
                    m_frames[idx].data(), m_frames[prevIdx].data(), m_frameBuffer[idx]);
            }
            else if (m_xStep == 16)
            {
                getFrame_avgY_array_16_x(
                    m_frames[idx].data(), m_frames[prevIdx].data(), m_frameBuffer[idx]);
            }
            else
            {
                getFrame_avgY_array_x_x(
                    m_frames[idx].data(), m_frames[prevIdx].data(), m_frameBuffer[idx], m_xStep);
            }
            analyzeMotionAmount(m_frameBuffer[idx]);
        }
    #else
        if (m_totalFrames == 0)
        {
            for (int i = 0; i < FRAMES_BUFFER_SIZE; ++i)
            {
                scaleFrame(
                    m_frames[idx].data()->data[0],
                    m_frames[idx]->width,
                    m_frames[idx]->height,
                    m_frames[idx]->linesize[0],
                    m_frameBuffer[i],
                    m_frameBuffer[0],
                    m_frameDeltaBuffer);
            }
        }
        else
        {
            scaleFrame(
                m_frames[idx].data()->data[0],
                m_frames[idx]->width,
                m_frames[idx]->height,
                m_frames[idx]->linesize[0],
                m_frameBuffer[idx],
                m_frameBuffer[prevIdx],
                m_frameDeltaBuffer);
        }
        analyzeMotionAmount(m_frameDeltaBuffer);
    #endif

#if 0
    LARGE_INTEGER end_time;
    if ( QueryPerformanceCounter(&end_time) )
    {
        m_sumLogTime += double(end_time.QuadPart-timer.QuadPart)/(PCFreq);
        //qDebug()<<"finish"<< m_numFrame;
    };

    if ( m_numFrame % 100 == 0 )
    {
        qDebug() << "Motion estimation speed:" << (m_sumLogTime / 100.0);
        m_sumLogTime = 0.0;
    };
    //qDebug() << m_numFrame;
    m_numFrame++;
#endif // 0

    if (m_totalFrames == 0)
        m_totalFrames++;

    m_motionHasBeenTaken = false;
    return true;
}

void QnMotionEstimation::postFiltering()
{
    for (int y = 0; y < Qn::kMotionGridHeight; ++y)
    {
        for (int x = 0; x < m_scaledWidth; ++x)
        {
            int aroundCnt = 0;
            if (x > 0 && isMotionAt((quint8*)m_resultMotion, x-1,y))
                aroundCnt+=2;
            if (x < m_scaledWidth-1 && isMotionAt((quint8*)m_resultMotion, x+1,y))
                aroundCnt+=2;
            if (y > 0 && isMotionAt((quint8*)m_resultMotion, x,y-1))
                aroundCnt+=2;
            if (y < Qn::kMotionGridHeight-1 && isMotionAt((quint8*)m_resultMotion, x,y+1))
                aroundCnt+=2;

            if (x > 0 && y > 0 && isMotionAt((quint8*)m_resultMotion, x-1,y-1))
                aroundCnt++;
            if (x < m_scaledWidth-1 && y < Qn::kMotionGridHeight-1 && isMotionAt((quint8*)m_resultMotion, x+1,y+1))
                aroundCnt++;
            if (x < m_scaledWidth-1 && y > 0 && isMotionAt((quint8*)m_resultMotion, x+1,y-1))
                aroundCnt++;
            if (x > 0 && y < Qn::kMotionGridHeight-1 && isMotionAt((quint8*)m_resultMotion, x-1,y+1))
                aroundCnt++;

            if (aroundCnt >= 6 && !isMotionAt((quint8*)m_resultMotion, x,y))
                setMotionAt((quint8*)m_resultMotion, x,y);
        }
    }
}


QnMetaDataV1Ptr QnMotionEstimation::getMotion()
{
    if (m_motionHasBeenTaken)
        return QnMetaDataV1Ptr();

    if (!m_lastMotionData)
        m_lastMotionData = makeMotion();

    return m_lastMotionData;
}

QnMetaDataV1Ptr QnMotionEstimation::takeMotion()
{
    QnMetaDataV1Ptr result = getMotion();
    m_motionHasBeenTaken = true;
    m_lastMotionData.reset();

    return result;
}

QnMetaDataV1Ptr QnMotionEstimation::makeMotion()
{
    QnMetaDataV1Ptr rez(new QnMetaDataV1());
    rez->timestamp = (m_lastFrameTime == (qint64)AV_NOPTS_VALUE)
        ? qnSyncTime->currentMSecsSinceEpoch() * 1000
        : m_lastFrameTime;

    rez->channelNumber = m_channelNum;
    rez->m_duration = 1000 * 1000 * 1000; // 1000 sec ;
    if (m_decoder == 0)
        return rez;

    // Scale the result motion data (height is already valid, scale width only).
    // Data rotates, so actually we duplicate or remove some lines
    int lineStep = (m_scaledWidth * 65536) / Qn::kMotionGridWidth;
    int scaledLineNum = 0;
    int prevILineNum = -1;
    quint32* dst = (quint32*)rez->data();

    for (int y = 0; y < Qn::kMotionGridWidth; ++y)
    {
        int iLineNum = (scaledLineNum + 32768) >> 16;
        if (iLineNum > prevILineNum)
        {
            dst[y] = m_resultMotion[iLineNum];
            for (int i = 1; i < iLineNum - prevILineNum; ++i)
                dst[y] |= m_resultMotion[iLineNum + i];
        }
        else {
            dst[y] |= m_resultMotion[iLineNum];
        }
        prevILineNum = iLineNum;
        scaledLineNum += lineStep;
    }

    memset(m_resultMotion, 0, Qn::kMotionGridHeight * m_scaledWidth);
    m_firstFrameTime = m_lastFrameTime;
    m_totalFrames++;

    return rez;
}

bool QnMotionEstimation::existsMetadata() const
{
    return !m_motionHasBeenTaken
        && m_lastFrameTime - m_firstFrameTime >= MOTION_AGGREGATION_PERIOD; // 30 ms agg period
}

QSize QnMotionEstimation::videoResolution() const
{
    return m_videoResolution;
}

void QnMotionEstimation::setMotionMask(const QnMotionRegion& region)
{
    QnMutexLocker lock( &m_mutex );
    qFreeAligned(m_motionMask);
    qFreeAligned(m_motionSensMask);
    m_motionMask = (quint8*) qMallocAligned(Qn::kMotionGridWidth * Qn::kMotionGridHeight, 32);
    m_motionSensMask = (quint8*) qMallocAligned(Qn::kMotionGridWidth * Qn::kMotionGridHeight, 32);

    memset(m_motionMask, 255, Qn::kMotionGridWidth * Qn::kMotionGridHeight);
    for (int sens = 0; sens < QnMotionRegion::kSensitivityLevelCount; ++sens)
    {
        for(const QRect& rect: region.getRectsBySens(sens))
        {
            for (int y = rect.top(); y <= rect.bottom(); ++y)
            {
                for (int x = rect.left(); x <= rect.right(); ++x)
                {
                    m_motionMask[x * Qn::kMotionGridHeight + y] = sensitivityToMask[sens];
                    m_motionSensMask[x * Qn::kMotionGridHeight + y] = sens;
                }

            }
        }
    }
    m_lastImgWidth = m_lastImgHeight = 0;
}

void QnMotionEstimation::setChannelNum(int value)
{
    m_channelNum = value;
}

void QnMotionEstimation::stop()
{
    m_decoder.reset();
}
