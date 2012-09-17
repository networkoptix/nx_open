#include "motion_estimation.h"

#include <cmath>

#include "utils/media/sse_helper.h"

#include <QImage>
#include <QTime>
#include <QDebug>
#include "utils/network/socket.h"
#include "utils/common/synctime.h"
#include "utils/common/math.h"

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
static const int ZERR_THRESHOLD = 8;
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



static const __m128i sse_0x80_const = _mm_setr_epi32(0x80808080, 0x80808080, 0x80808080, 0x80808080);
static const int LARGE_FILTER_MIN_SENSETIVITY = 5;

quint32 reverseBits(quint32 v)
{
    return (BitReverseTable256[v & 0xff]) | 
        (BitReverseTable256[(v >> 8) & 0xff] << 8) | 
        (BitReverseTable256[(v >> 16) & 0xff] << 16) |
        (BitReverseTable256[(v >> 24) & 0xff] << 24);
}

inline bool isMotionAt(const quint8* data, int x, int y)
{
    int shift = x*MD_HEIGHT + y;
    return data[shift/8] & (128 >> (shift&7));
}

inline void setMotionAt(quint8* data, int x, int y) 
{
    int shift = x*MD_HEIGHT + y;
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

static const __m128i zerroConst = _mm_setr_epi32(0, 0, 0, 0);

void saveFrame(quint8* data, int width, int height, int linesize, const QString& fileName)
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

void getFrame_avgY_array_8_x(const CLVideoDecoderOutput* frame, const CLVideoDecoderOutput* prevFrame, quint8* dst)
{
    //saveFrame(frame->data[0], frame->width, frame->height, frame->linesize[0], "c:/src_orig.bmp");

    Q_ASSERT(frame->width % 8 == 0);
    Q_ASSERT(frame->linesize[0] % 16 == 0);

    const __m128i* curLinePtr = (const __m128i*) frame->data[0];
    const __m128i* curLinePtrPrev = (const __m128i*) prevFrame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = qPower2Ceil((unsigned)frame->width,16)/16;
    int linesInStep = (frame->height*65536)/ MD_HEIGHT;
    int ySteps = MD_HEIGHT;

    int curLineNum = 0;
    for (int y = 0; y < ySteps; ++y)
    {
        quint8* dstCurLine = dst;
        int nextLineNum = curLineNum + linesInStep;
        int rowCnt = (nextLineNum>>16) - (curLineNum>>16);
        const __m128i* linePtr = curLinePtr;
        const __m128i* linePtrPrev = curLinePtrPrev;
        for (int x = 0; x < xSteps; ++x)
        {
            __m128i blockSum;
            blockSum = _mm_xor_si128(blockSum, blockSum);
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
            dstCurLine[MD_HEIGHT] = _mm_cvtsi128_si32(_mm_srli_si128(blockSum, 8)) / pixels; // SSE2
            dstCurLine += MD_HEIGHT*2;
        }  
        curLineNum = nextLineNum;
        curLinePtr += lineSize*rowCnt;
        curLinePtrPrev += lineSize*rowCnt;
        dst++;
    }

    //saveFrame(frame->data[0], frame->width, frame->height, frame->linesize[0], "c:/src_orig.bmp");
}

void getFrame_avgY_array_8_x_mc(const CLVideoDecoderOutput* frame, quint8* dst)
{
    //saveFrame(frame->data[0], frame->width, frame->height, frame->linesize[0], "c:/src_orig.bmp");

    Q_ASSERT(frame->width % 8 == 0);
    Q_ASSERT(frame->linesize[0] % 16 == 0);

    const __m128i* curLinePtr = (const __m128i*) frame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = qPower2Ceil((unsigned)frame->width,16)/16;
    int linesInStep = (frame->height*65536)/ MD_HEIGHT;
    int ySteps = MD_HEIGHT;

    int curLineNum = 0;
    for (int y = 0; y < ySteps; ++y)
    {
        quint8* dstCurLine = dst;
        int nextLineNum = curLineNum + linesInStep;
        int rowCnt = (nextLineNum>>16) - (curLineNum>>16);
        const __m128i* linePtr = curLinePtr;
        for (int x = 0; x < xSteps; ++x)
        {
            __m128i blockSum;
            blockSum = _mm_xor_si128(blockSum, blockSum); // SSE2
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
            dstCurLine[MD_HEIGHT] = _mm_cvtsi128_si32(_mm_srli_si128(blockSum, 8)) / pixels; // SSE2
            dstCurLine += MD_HEIGHT*2;
        }  
        curLineNum = nextLineNum;
        curLinePtr += lineSize*rowCnt;
        dst++;
    }

    //saveFrame(frame->data[0], frame->width, frame->height, frame->linesize[0], "c:/src_orig.bmp");
}

void fillRightEdge8(const CLVideoDecoderOutput* frame)
{
    Q_ASSERT(frame->linesize[0] >= frame->width + 8);
    quint64* dataPtr = (quint64*) (frame->data[0]+frame->width);
    for (int i = 0; i < frame->height; ++i) {
        dataPtr[0] = dataPtr[-1]; // replicate most right 8 pixels
        dataPtr += frame->linesize[0]/sizeof(quint64);
    }
}

void getFrame_avgY_array_16_x(const CLVideoDecoderOutput* frame, const CLVideoDecoderOutput* prevFrame, quint8* dst)
{
    Q_ASSERT(frame->width % 8 == 0);
    Q_ASSERT(frame->linesize[0] % 16 == 0);

    const __m128i* curLinePtr = (const __m128i*) frame->data[0];
    const __m128i* prevLinePtr = (const __m128i*) prevFrame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = qPower2Ceil((unsigned)frame->width,16)/16;
    if (frame->width % 16)
        fillRightEdge8(frame);

    int linesInStep = (frame->height*65536)/ MD_HEIGHT;
    int ySteps = MD_HEIGHT;

    int curLineNum = 0;
    for (int y = 0; y < ySteps; ++y)
    {
        quint8* dstCurLine = dst;
        int nextLineNum = curLineNum + linesInStep;
        int rowCnt = (nextLineNum>>16) - (curLineNum>>16);
        const __m128i* linePtr = curLinePtr;
        const __m128i* linePtr2 = prevLinePtr;
        for (int x = 0; x < xSteps; ++x)
        {

            __m128i blockSum;
            blockSum = _mm_xor_si128(blockSum, blockSum); // SSE2
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
            dstCurLine += MD_HEIGHT;
        }
        curLineNum = nextLineNum;
        curLinePtr += lineSize*rowCnt;
        prevLinePtr += lineSize*rowCnt;
        dst++;
    }
}

void getFrame_avgY_array_16_x_mc(const CLVideoDecoderOutput* frame, quint8* dst)
{
    Q_ASSERT(frame->width % 8 == 0);
    Q_ASSERT(frame->linesize[0] % 16 == 0);

    const __m128i* curLinePtr = (const __m128i*) frame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = qPower2Ceil((unsigned)frame->width,16)/16;
    int linesInStep = (frame->height*65536)/ MD_HEIGHT;
    int ySteps = MD_HEIGHT;

    int curLineNum = 0;
    for (int y = 0; y < ySteps; ++y)
    {
        quint8* dstCurLine = dst;
        int nextLineNum = curLineNum + linesInStep;
        int rowCnt = (nextLineNum>>16) - (curLineNum>>16);
        const __m128i* linePtr = curLinePtr;
        for (int x = 0; x < xSteps; ++x)
        {

            __m128i blockSum;
            blockSum = _mm_xor_si128(blockSum, blockSum); // SSE2
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
            dstCurLine += MD_HEIGHT;
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
    dstCurLine += MD_HEIGHT;\
}
    Q_ASSERT(frame->width % 8 == 0);
    Q_ASSERT(frame->linesize[0] % 16 == 0);
    Q_ASSERT(sqWidth % 8 == 0);
    
    int sqWidthSteps = sqWidth / 8;

    const __m128i* curLinePtr = (const __m128i*) frame->data[0];
    const __m128i* prevLinePtr = (const __m128i*) prevFrame->data[0];
    int lineSize = frame->linesize[0] / 16;
    if (frame->width % 16)
        fillRightEdge8(frame);

    int xSteps = qPower2Ceil((unsigned)frame->width,16)/16;
    int linesInStep = (frame->height*65536)/ MD_HEIGHT;
    int ySteps = MD_HEIGHT;

    int squareSum = 0;
    int squareStep = 0;
    int curLineNum = 0;
    for (int y = 0; y < ySteps; ++y)
    {
        quint8* dstCurLine = dst;
        int nextLineNum = curLineNum + linesInStep;
        int rowCnt = (nextLineNum>>16) - (curLineNum>>16);
        const __m128i* linePtr = curLinePtr;
        const __m128i* linePtr2 = prevLinePtr;
        for (int x = 0; x < xSteps; ++x)
        {
            __m128i blockSum;
            blockSum = _mm_xor_si128(blockSum, blockSum); // SSE2
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
    dstCurLine += MD_HEIGHT;\
}
    Q_ASSERT(frame->width % 8 == 0);
    Q_ASSERT(frame->linesize[0] % 16 == 0);
    Q_ASSERT(sqWidth % 8 == 0);
    sqWidth /= 8;

    const __m128i* curLinePtr = (const __m128i*) frame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = qPower2Ceil((unsigned)frame->width,16)/16;
    int linesInStep = (frame->height*65536)/ MD_HEIGHT;
    int ySteps = MD_HEIGHT;

    int squareSum = 0;
    int squareStep = 0;
    int curLineNum = 0;
    for (int y = 0; y < ySteps; ++y)
    {
        quint8* dstCurLine = dst;
        int nextLineNum = curLineNum + linesInStep;
        int rowCnt = (nextLineNum>>16) - (curLineNum>>16);
        const __m128i* linePtr = curLinePtr;
        for (int x = 0; x < xSteps; ++x)
        {

            __m128i blockSum;
            blockSum = _mm_xor_si128(blockSum, blockSum); // SSE2
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
        }
        if (squareStep)
            flushData();
        curLineNum = nextLineNum;
        curLinePtr += lineSize*rowCnt;
        dst++;
    }
#undef flushData
}


QnMotionEstimation::QnMotionEstimation()
{
    m_decoder = 0;
    //m_outFrame.setUseExternalData(false);
    //m_prevFrame.setUseExternalData(false);
    m_frames[0] = new CLVideoDecoderOutput();
    m_frames[1] = new CLVideoDecoderOutput();
    m_frames[0]->setUseExternalData(false);
    m_frames[1]->setUseExternalData(false);

    m_scaledMask = 0; // mask scaled to x * MD_HEIGHT. for internal use
    m_motionSensScaledMask = 0;
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
    delete m_decoder;
    qFreeAligned(m_scaledMask);
    qFreeAligned(m_motionSensScaledMask);
    for (int i = 0; i < FRAMES_BUFFER_SIZE; ++i)
        qFreeAligned(m_frameBuffer[i]);
    qFreeAligned(m_filteredFrame);
    qFreeAligned(m_motionMask);
    qFreeAligned(m_motionSensMask);
    qFreeAligned(m_linkedNums);
    delete [] m_resultMotion;
    delete m_frames[0];
    delete m_frames[1];
}

// rescale motion mask width (mask rotated, so actually remove or duplicate some lines)
void QnMotionEstimation::scaleMask(quint8* mask, quint8* scaledMask)
{
    float lineStep = (m_scaledWidth-1) / float(MD_WIDTH-1);
    float scaledLineNum = 0;
    int prevILineNum = -1;
    for (int y = 0; y < MD_WIDTH; ++y)
    {
        int iLineNum = (int) (scaledLineNum+0.5);
        Q_ASSERT(iLineNum < m_scaledWidth);
        __m128i* dst = (__m128i*) (scaledMask + MD_HEIGHT*iLineNum);
        __m128i* src = (__m128i*) (mask + MD_HEIGHT*y);
        if (iLineNum > prevILineNum) 
        {
            for (int i = 0; i < iLineNum - prevILineNum; ++i) 
                memcpy(dst - i*MD_HEIGHT/sizeof(__m128i) , src, MD_HEIGHT);
        }
        else {
            dst[0] = _mm_min_epu8(dst[0], src[0]); // SSE2
            dst[1] = _mm_min_epu8(dst[1], src[1]); // SSE2
        }
        prevILineNum = iLineNum;
        scaledLineNum += lineStep;
    }
    
    //__m128i* curPtr = (__m128i*) mask;
     //for (int i = 0; i < m_scaledWidth*2; ++i)
     //   curPtr[i] = _mm_sub_epi8(curPtr[i], sse_0x80_const); // SSE2
}

void QnMotionEstimation::reallocateMask(int width, int height)
{
    qFreeAligned(m_scaledMask);
    qFreeAligned(m_motionSensScaledMask);
    for (int i = 0; i < FRAMES_BUFFER_SIZE; ++i)
        qFreeAligned(m_frameBuffer[i]);
    qFreeAligned(m_filteredFrame);
    qFreeAligned(m_linkedNums);
    delete [] m_resultMotion;

    m_lastImgWidth = width;
    m_lastImgHeight = height;
    // calculate scaled image size and allocate memory
    for (m_xStep = 8; m_lastImgWidth/m_xStep > MD_WIDTH; m_xStep += 8);
    if (m_lastImgWidth/m_xStep <= (MD_WIDTH*3)/4 && m_xStep > 8)
        m_xStep -= 8;
    m_scaledWidth = m_lastImgWidth / m_xStep;
    if (m_lastImgWidth > m_scaledWidth*m_xStep)
        m_scaledWidth++;

    int swUp = m_scaledWidth+1; // reserve one extra column because of analize_frame function for x8 width can write 1 extra byte
    m_scaledMask = (quint8*) qMallocAligned(MD_HEIGHT * swUp, 32);
    m_linkedNums = (int*) qMallocAligned(MD_HEIGHT * swUp * sizeof(int), 32);
    m_motionSensScaledMask = (quint8*) qMallocAligned(MD_HEIGHT * swUp, 32);
    m_frameBuffer[0] = (quint8*) qMallocAligned(MD_HEIGHT * swUp, 32);
    m_frameBuffer[1] = (quint8*) qMallocAligned(MD_HEIGHT * swUp, 32);
    m_filteredFrame = (quint8*) qMallocAligned(MD_HEIGHT * swUp, 32);
    m_resultMotion = new quint32[MD_HEIGHT/4 * swUp];
    memset(m_resultMotion, 0, MD_HEIGHT * swUp);

    // scale motion mask to scaled size
    if (m_scaledWidth != MD_WIDTH) {
        scaleMask(m_motionMask, m_scaledMask);
        scaleMask(m_motionSensMask, m_motionSensScaledMask);
    }
    else {
        memcpy(m_scaledMask, m_motionMask, MD_WIDTH * MD_HEIGHT);
        memcpy(m_motionSensScaledMask, m_motionSensMask, MD_WIDTH * MD_HEIGHT);
    }
    m_isNewMask = false;
}

void QnMotionEstimation::analizeMotionAmount(quint8* frame)
{
    // 1. filtering. If a lot of motion around current pixel, mark it too, also remove motion if diff < mask
    quint8* curPtr = frame;
    quint8* dstPtr = m_filteredFrame;
    const quint8* maskPtr = m_scaledMask;

    for (int y = 0; y < MD_HEIGHT; ++y) {
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
        for (int y = 1; y < MD_HEIGHT-1; ++y)
        {
            if (*curPtr <= *maskPtr)
            {
                int aroundMotionAmount =(int(curPtr[-1] > maskPtr[-1]) +
                                         int(curPtr[1]  > maskPtr[-1]) +
                                         int(curPtr[-MD_HEIGHT] >maskPtr[-MD_HEIGHT]) + 
                                         int(curPtr[MD_HEIGHT] >maskPtr[MD_HEIGHT]))*2 +
                                         int(curPtr[-1-MD_HEIGHT] >maskPtr[-1-MD_HEIGHT]) +
                                         int(curPtr[-1+MD_HEIGHT] >maskPtr[-1+MD_HEIGHT]) +
                                         int(curPtr[1-MD_HEIGHT] >maskPtr[1-MD_HEIGHT]) +
                                         int(curPtr[1+MD_HEIGHT] >maskPtr[1+MD_HEIGHT]);
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

    for (int y = 0; y < MD_HEIGHT; ++y) {
        *dstPtr = *curPtr <= *maskPtr ? 0 : *curPtr;
        curPtr++;
        maskPtr++;
        dstPtr++;
    }

    // 2. Determine linked areas
    int currentLinkIndex = 1;
    memset(m_linkedNums, 0, sizeof(int) * MD_HEIGHT * m_scaledWidth);
    memset(m_linkedSquare, 0, sizeof(m_linkedSquare));
    for (int i = 0; i < MD_HEIGHT*m_scaledWidth/2;++i)
        m_linkedMap[i] = i;

    int idx = 0;
    // 2.1 top left pixel
    if (m_filteredFrame[idx++])
        *m_linkedNums = currentLinkIndex++;

    // 2.2 left col
    for (int y = 1; y < MD_HEIGHT; ++y, ++idx) {
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
            if (m_linkedNums[idx-MD_HEIGHT])
                m_linkedNums[idx] = m_linkedNums[idx-MD_HEIGHT];
            else 
                m_linkedNums[idx] = currentLinkIndex++;
        }
        idx++;
        // 2.4 all other pixels
        for (int y = 1; y < MD_HEIGHT; ++y, ++idx)
        {
            if (m_filteredFrame[idx])
            {
                if (m_linkedNums[idx-1]) {
                    m_linkedNums[idx] = m_linkedNums[idx-1];
                    if (m_linkedNums[idx-MD_HEIGHT] && m_linkedNums[idx-MD_HEIGHT] != m_linkedNums[idx]) 
                    {
                        if (m_linkedNums[idx-MD_HEIGHT] < m_linkedNums[idx])
                            m_linkedMap[m_linkedNums[idx]] = m_linkedNums[idx-MD_HEIGHT];
                        else
                            m_linkedMap[m_linkedNums[idx-MD_HEIGHT]] = m_linkedNums[idx];
                    }
                }
                else if (m_linkedNums[idx-MD_HEIGHT]) {
                    m_linkedNums[idx] = m_linkedNums[idx-MD_HEIGHT];
                }
#ifndef DISABLE_8_WAY_AREA
                else if (m_linkedNums[idx-1-MD_HEIGHT]) {
                    m_linkedNums[idx] = m_linkedNums[idx-1-MD_HEIGHT];
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
    for (int i = 0; i < MD_HEIGHT*m_scaledWidth; ++i)
        m_linkedNums[i] = m_linkedMap[m_linkedNums[i]];

    // 5. calculate square of each area
    for (int i = 0; i < MD_HEIGHT*m_scaledWidth; ++i)
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
            for (int k = 0; k < MD_HEIGHT*m_scaledWidth; ++k)
            {
                if (m_linkedNums[k] == i)
                    qDebug() << "---- square data at" << k/MD_HEIGHT << 'x' << k%MD_HEIGHT << "=" << m_filteredFrame[k];
            }
        }
    }
#endif

    // 6. remove motion if motion square is not enough and write result to bitarray
    for (int i = 0; i < MD_HEIGHT*m_scaledWidth;)
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

void QnMotionEstimation::analizeFrame(QnCompressedVideoDataPtr videoData)
{
    QMutexLocker lock(&m_mutex);

    if (m_decoder == 0 && !(videoData->flags & AV_PKT_FLAG_KEY))
        return;
    if (m_decoder == 0 || m_decoder->getContext()->codec_id != videoData->compressionType)
    {
        delete m_decoder;
        m_decoder = new CLFFmpegVideoDecoder(videoData->compressionType, videoData, false);
        m_decoder->getContext()->flags |= CODEC_FLAG_GRAY;
    }

    int idx = m_totalFrames % FRAMES_BUFFER_SIZE;
    int prevIdx = (m_totalFrames-1) % FRAMES_BUFFER_SIZE;

    if (!m_decoder->decode(videoData, m_frames[idx]))
        return;
    if (m_firstFrameTime == qint64(AV_NOPTS_VALUE))
        m_firstFrameTime = m_frames[idx]->pkt_dts;
    m_lastFrameTime = m_frames[idx]->pkt_dts;

#if 0
    // test
    fillFrameRect(m_frames[idx], QRect(0,0,m_frames[idx]->width, m_frames[idx]->height), 225);
    if (m_totalFrames % 2 == 1) {
        fillFrameRect(m_frames[idx], QRect(QPoint(0, 20), QPoint(m_frames[idx]->width, 32)), 20);
        fillFrameRect(m_frames[idx], QRect(QPoint(0, m_frames[idx]->height/2), QPoint(m_frames[idx]->width, m_frames[idx]->height/2+8)), 20);

        fillFrameRect(m_frames[idx], QRect(QPoint(0, m_frames[idx]->height/2-8), QPoint(m_frames[idx]->width/4, m_frames[idx]->height/2)), 20);
        fillFrameRect(m_frames[idx], QRect(QPoint(m_frames[idx]->width/2, m_frames[idx]->height/2-24), QPoint(m_frames[idx]->width*3/4, m_frames[idx]->height/2)), 20);
    }
    //else
    //    fillFrameRect(m_frames[idx], QRect(QPoint(0, m_frames[idx]->height/2), QPoint(m_frames[idx]->width, m_frames[idx]->height)), 40);
#endif

    if (m_decoder->getWidth() != m_lastImgWidth || m_decoder->getHeight() != m_lastImgHeight || m_isNewMask)
        reallocateMask(m_decoder->getWidth(), m_decoder->getHeight());

#define ANALIZE_PER_PIXEL_DIF
#ifdef ANALIZE_PER_PIXEL_DIF
    // do not calc motion if resolution just changed
    if (m_frames[0]->width == m_frames[1]->width && m_frames[0]->height == m_frames[1]->height  && m_frames[0]->format == m_frames[1]->format) 
    {
        // calculate difference between frames
        if (m_xStep == 8)
            getFrame_avgY_array_8_x (m_frames[idx], m_frames[prevIdx], m_frameBuffer[idx]);
        else if (m_xStep == 16)
            getFrame_avgY_array_16_x(m_frames[idx], m_frames[prevIdx], m_frameBuffer[idx]);
        else
            getFrame_avgY_array_x_x (m_frames[idx], m_frames[prevIdx], m_frameBuffer[idx], m_xStep);


        analizeMotionAmount(m_frameBuffer[idx]);
        

        /*
        __m128i* cur = (__m128i*) m_frameBuffer[idx];
        __m128i* mask = (__m128i*) m_scaledMask;
        for (int i = 0; i < m_scaledWidth; ++i)
        {
            __m128i rez1 = _mm_cmpgt_epi8(_mm_sub_epi8(*cur, sse_0x80_const), mask[0]); // SSE2
            __m128i rez2 = _mm_cmpgt_epi8(_mm_sub_epi8(cur[1], sse_0x80_const), mask[1]); // SSE2

            m_resultMotion[i] |= reverseBits( (_mm_movemask_epi8(rez2) << 16) + _mm_movemask_epi8(rez1)); // SSE2
            cur += 2;
            mask += 2;
        }
        */
        
        
    }
#else
    // analize per macroblock diff
    if (m_totalFrames > 0)
    {
        if (m_xStep == 8)
            getFrame_avgY_array_8_x_mc(m_frames[idx], m_frameBuffer[idx]);
        else if (m_xStep == 16)
            getFrame_avgY_array_16_x_mc(m_frames[idx], m_frameBuffer[idx]);
        else
            getFrame_avgY_array_x_x_mc(m_frames[idx], m_frameBuffer[idx], m_xStep);

        //saveFrame(m_frameBuffer[idx], MD_HEIGHT, m_scaledWidth, MD_HEIGHT, "c:/src_scaled.bmp");

        // calculate difference between frames
        __m128i* cur = (__m128i*) m_frameBuffer[idx];
        __m128i* prev = (__m128i*) m_frameBuffer[prevIdx];
        __m128i* mask = (__m128i*) m_scaledMask;
        for (int i = 0; i < m_scaledWidth; ++i)
        {
            __m128i rez1 = _mm_sub_epi8(_mm_sub_epi8(_mm_max_epu8(*cur,*prev), _mm_min_epu8(*cur,*prev)), sse_0x80_const);
            rez1 = _mm_cmpgt_epi8(rez1, mask[0]);

            __m128i rez2 = _mm_sub_epi8(_mm_sub_epi8(_mm_max_epu8(cur[1],prev[1]), _mm_min_epu8(cur[1],prev[1])), sse_0x80_const);
            rez2 = _mm_cmpgt_epi8(rez2, mask[1]);
            
            m_resultMotion[i] |= reverseBits( (_mm_movemask_epi8(rez2) << 16) + _mm_movemask_epi8(rez1));
            cur += 2;
            prev += 2;
            mask += 2;
        }
    }
#endif
    if (m_totalFrames == 0)
        m_totalFrames++;
}

void QnMotionEstimation::postFiltering()
{
    for (int y = 0; y < MD_HEIGHT; ++y)
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
            if (y < MD_HEIGHT-1 && isMotionAt((quint8*)m_resultMotion, x,y+1))
                aroundCnt+=2;

            if (x > 0 && y > 0 && isMotionAt((quint8*)m_resultMotion, x-1,y-1))
                aroundCnt++;
            if (x < m_scaledWidth-1 && y < MD_HEIGHT-1 && isMotionAt((quint8*)m_resultMotion, x+1,y+1))
                aroundCnt++;
            if (x < m_scaledWidth-1 && y > 0 && isMotionAt((quint8*)m_resultMotion, x+1,y-1))
                aroundCnt++;
            if (x > 0 && y < MD_HEIGHT-1 && isMotionAt((quint8*)m_resultMotion, x-1,y+1))
                aroundCnt++;

            if (aroundCnt >= 6 && !isMotionAt((quint8*)m_resultMotion, x,y))
                setMotionAt((quint8*)m_resultMotion, x,y);
        }
    }
}


QnMetaDataV1Ptr QnMotionEstimation::getMotion()
{
    QnMetaDataV1Ptr rez(new QnMetaDataV1());
    //rez->timestamp = m_firstFrameTime == AV_NOPTS_VALUE ? qnSyncTime->currentMSecsSinceEpoch()*1000 : m_firstFrameTime;
    rez->timestamp = qnSyncTime->currentMSecsSinceEpoch()*1000;
    rez->m_duration = 1000*1000*1000; // 1000 sec ;
    if (m_decoder == 0)
        return rez;

    // scale result motion (height already valid, scale width ony. Data rotates, so actually duplicate or remove some lines
    int lineStep = (m_scaledWidth*65536) / MD_WIDTH;
    int scaledLineNum = 0;
    int prevILineNum = -1;
    quint32* dst = (quint32*) rez->data.data();

    //postFiltering();

    for (int y = 0; y < MD_WIDTH; ++y)
    {
        int iLineNum = (scaledLineNum+32768) >> 16;
        if (iLineNum > prevILineNum) 
        {
            for (int i = 0; i < iLineNum - prevILineNum; ++i)
                dst[y] = m_resultMotion[iLineNum+i];
        }
        else {
            dst[y] |= m_resultMotion[iLineNum];
        }
        prevILineNum = iLineNum;
        scaledLineNum += lineStep;
    }

    memset(m_resultMotion, 0, MD_HEIGHT * m_scaledWidth);
    m_firstFrameTime = m_lastFrameTime;
    m_totalFrames++;

    return rez;
}


bool QnMotionEstimation::existsMetadata() const
{
    return m_lastFrameTime - m_firstFrameTime >= 300 * 1000; // 30 ms agg period
}

void QnMotionEstimation::setMotionMask(const QnMotionRegion& region)
{
    QMutexLocker lock(&m_mutex);
    qFreeAligned(m_motionMask);
    qFreeAligned(m_motionSensMask);
    m_motionMask = (quint8*) qMallocAligned(MD_WIDTH * MD_HEIGHT, 32);
    m_motionSensMask = (quint8*) qMallocAligned(MD_WIDTH * MD_HEIGHT, 32);

    memset(m_motionMask, 255, MD_WIDTH * MD_HEIGHT);
    for (int sens = QnMotionRegion::MIN_SENSITIVITY; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {
        foreach(const QRect& rect, region.getRectsBySens(sens))
        {
            for (int y = rect.top(); y <= rect.bottom(); ++y)
            {
                for (int x = rect.left(); x <= rect.right(); ++x)
                {
                    m_motionMask[x * MD_HEIGHT + y] = sensitivityToMask[sens];
                    m_motionSensMask[x * MD_HEIGHT + y] = sens;
                }

            }
        }
    }
    m_lastImgWidth = m_lastImgHeight = 0;
}
