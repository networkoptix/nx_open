#include "motion_estimation.h"
#include "utils/media/sse_helper.h"

#include <QImage>
#include <QTime>
#include <QDebug>
#include "utils/network/socket.h"
#include "utils/common/synctime.h"



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
static const __m128i sse_0x80_const = _mm_setr_epi32(0x80808080, 0x80808080, 0x80808080, 0x80808080);

quint32 reverseBits(quint32 v)
{
    return (BitReverseTable256[v & 0xff]) | 
        (BitReverseTable256[(v >> 8) & 0xff] << 8) | 
        (BitReverseTable256[(v >> 16) & 0xff] << 16) |
        (BitReverseTable256[(v >> 24) & 0xff] << 24);
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

void getFrame_avgY_array_8_x(const CLVideoDecoderOutput* frame, const CLVideoDecoderOutput* prevFrame, quint8* dst)
{
    //saveFrame(frame->data[0], frame->width, frame->height, frame->linesize[0], "c:/src_orig.bmp");

    Q_ASSERT(frame->width % 16 == 0);
    Q_ASSERT(frame->linesize[0] % 16 == 0);

    const __m128i* curLinePtr = (const __m128i*) frame->data[0];
    const __m128i* curLinePtrPrev = (const __m128i*) prevFrame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = frame->width/16;
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
                __m128i partSum = _mm_sad_epu8(*src, *srcPrev); // src 16 bytes sum
                src += lineSize;
                srcPrev += lineSize;
                ++i;
                blockSum = _mm_add_epi32(blockSum, partSum);
            }
            linePtr++;
            linePtrPrev++;

            // get avg value
            int pixels = rowCnt * 8;
            *dstCurLine = _mm_cvtsi128_si32(blockSum) / pixels;
            dstCurLine[MD_HEIGHT] = _mm_cvtsi128_si32(_mm_srli_si128(blockSum, 8)) / pixels;
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

    Q_ASSERT(frame->width % 16 == 0);
    Q_ASSERT(frame->linesize[0] % 16 == 0);

    const __m128i* curLinePtr = (const __m128i*) frame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = frame->width/16;
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
            blockSum = _mm_xor_si128(blockSum, blockSum);
            const __m128i* src = linePtr;
            for (int i = 0; i < rowCnt;)
            {
                __m128i partSum = _mm_sad_epu8(*src, zerroConst); // src 16 bytes sum
                src += lineSize;
                ++i;
                blockSum = _mm_add_epi32(blockSum, partSum);
            }
            linePtr++;

            // get avg value
            int pixels = rowCnt * 8;
            *dstCurLine = _mm_cvtsi128_si32(blockSum) / pixels;
            dstCurLine[MD_HEIGHT] = _mm_cvtsi128_si32(_mm_srli_si128(blockSum, 8)) / pixels;
            dstCurLine += MD_HEIGHT*2;
        }  
        curLineNum = nextLineNum;
        curLinePtr += lineSize*rowCnt;
        dst++;
    }

    //saveFrame(frame->data[0], frame->width, frame->height, frame->linesize[0], "c:/src_orig.bmp");
}

void getFrame_avgY_array_16_x(const CLVideoDecoderOutput* frame, const CLVideoDecoderOutput* prevFrame, quint8* dst)
{
    Q_ASSERT(frame->width % 16 == 0);
    Q_ASSERT(frame->linesize[0] % 16 == 0);

    const __m128i* curLinePtr = (const __m128i*) frame->data[0];
    const __m128i* prevLinePtr = (const __m128i*) prevFrame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = frame->width/16;
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
            blockSum = _mm_xor_si128(blockSum, blockSum);
            const __m128i* src = linePtr;
            const __m128i* src2 = linePtr2;
            for (int i = 0; i < rowCnt;)
            {
                __m128i partSum = _mm_sad_epu8(*src, *src2); // src 16 bytes sum
                src += lineSize;
                src2 += lineSize;
                ++i;
                blockSum = _mm_add_epi32(blockSum, partSum);
            }
            linePtr++;
            linePtr2++;

            // get avg value
            int pixels = rowCnt * 16;
            *dstCurLine = (_mm_cvtsi128_si32(_mm_srli_si128(blockSum, 8)) + _mm_cvtsi128_si32(blockSum)) / pixels;
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
    Q_ASSERT(frame->width % 16 == 0);
    Q_ASSERT(frame->linesize[0] % 16 == 0);

    const __m128i* curLinePtr = (const __m128i*) frame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = frame->width/16;
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
            blockSum = _mm_xor_si128(blockSum, blockSum);
            const __m128i* src = linePtr;
            for (int i = 0; i < rowCnt;)
            {
                __m128i partSum = _mm_sad_epu8(*src, zerroConst); // src 16 bytes sum
                src += lineSize;
                ++i;
                blockSum = _mm_add_epi32(blockSum, partSum);
            }
            linePtr++;

            // get avg value
            int pixels = rowCnt * 16;
            *dstCurLine = (_mm_cvtsi128_si32(_mm_srli_si128(blockSum, 8)) + _mm_cvtsi128_si32(blockSum)) / pixels;
            dstCurLine += MD_HEIGHT;
        }  
        curLineNum = nextLineNum;
        curLinePtr += lineSize*rowCnt;
        dst++;
    }
}

void getFrame_avgY_array_x_x(const CLVideoDecoderOutput* frame, const CLVideoDecoderOutput* prevFrame, quint8* dst, int sqWidth)
{
#define flushData() \
{\
    int pixels = rowCnt * sqWidth;\
    *dstCurLine = squareSum / pixels;\
    squareStep = 0;\
    squareSum = 0;\
    dstCurLine += MD_HEIGHT;\
}
    Q_ASSERT(frame->width % 16 == 0);
    Q_ASSERT(frame->linesize[0] % 16 == 0);
    Q_ASSERT(sqWidth % 8 == 0);
    sqWidth /= 8;

    const __m128i* curLinePtr = (const __m128i*) frame->data[0];
    const __m128i* prevLinePtr = (const __m128i*) prevFrame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = frame->width/16;
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
            blockSum = _mm_xor_si128(blockSum, blockSum);
            const __m128i* src = linePtr;
            const __m128i* src2 = linePtr2;
            for (int i = 0; i < rowCnt;)
            {
                __m128i partSum = _mm_sad_epu8(*src, *src2); // src 16 bytes sum
                src += lineSize;
                src2 += lineSize;
                ++i;
                blockSum = _mm_add_epi32(blockSum, partSum);
            }
            linePtr++;
            linePtr2++;

            // get avg value
            squareSum += _mm_cvtsi128_si32(blockSum);
            squareStep++;
            if (squareStep == sqWidth)
                flushData();
            squareSum += _mm_cvtsi128_si32(_mm_srli_si128(blockSum, 8));
            if (squareStep == sqWidth)
                flushData();
        }
        if (squareStep)
            flushData();
        curLineNum = nextLineNum;
        curLinePtr += lineSize*rowCnt;
        prevLinePtr += lineSize*rowCnt;
        dst++;
    }
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
    Q_ASSERT(frame->width % 16 == 0);
    Q_ASSERT(frame->linesize[0] % 16 == 0);
    Q_ASSERT(sqWidth % 8 == 0);
    sqWidth /= 8;

    const __m128i* curLinePtr = (const __m128i*) frame->data[0];
    int lineSize = frame->linesize[0] / 16;
    int xSteps = frame->width/16;
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
            blockSum = _mm_xor_si128(blockSum, blockSum);
            const __m128i* src = linePtr;
            for (int i = 0; i < rowCnt;)
            {
                __m128i partSum = _mm_sad_epu8(*src, zerroConst); // src 16 bytes sum
                src += lineSize;
                ++i;
                blockSum = _mm_add_epi32(blockSum, partSum);
            }
            linePtr++;

            // get avg value
            squareSum += _mm_cvtsi128_si32(blockSum);
            squareStep++;
            if (squareStep == sqWidth)
                flushData();
            squareSum += _mm_cvtsi128_si32(_mm_srli_si128(blockSum, 8));
            if (squareStep == sqWidth)
                flushData();
        }
        if (squareStep)
            flushData();
        curLineNum = nextLineNum;
        curLinePtr += lineSize*rowCnt;
        dst++;
    }
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
    m_frameBuffer[0] = m_frameBuffer[1] = 0;
    m_scaledWidth = 0;
    m_xStep = 0; // 8, 16, 24 e.t.c value
    m_lastImgWidth = 0;
    m_lastImgHeight = 0;
    m_motionMask = 0;
    m_totalFrames = 0;
    m_resultMotion = 0;
    m_firstFrameTime = AV_NOPTS_VALUE;
    m_lastFrameTime = AV_NOPTS_VALUE;
    m_isNewMask = false;
}

QnMotionEstimation::~QnMotionEstimation()
{
    delete m_decoder;
    qFreeAligned(m_scaledMask);
    qFreeAligned(m_frameBuffer[0]);
    qFreeAligned(m_frameBuffer[1]);
    qFreeAligned(m_motionMask);
    delete [] m_resultMotion;
    delete m_frames[0];
    delete m_frames[1];
}

// rescale motion mask width (mask rotated, so actually remove or duplicate some lines)
void QnMotionEstimation::scaleMask()
{
    float lineStep = m_scaledWidth / (float) MD_WIDTH;
    float scaledLineNum = 0;
    int prevILineNum = -1;
    for (int y = 0; y < MD_WIDTH; ++y)
    {
        int iLineNum = (int) (scaledLineNum+0.5);
        __m128i* dst = (__m128i*) (m_scaledMask + MD_HEIGHT*iLineNum);
        __m128i* src = (__m128i*) (m_motionMask + MD_HEIGHT*y);
        if (iLineNum > prevILineNum) 
        {
            for (int i = 0; i < iLineNum - prevILineNum; ++i)
                memcpy(dst + i*MD_HEIGHT , src + i*MD_HEIGHT, MD_HEIGHT);
        }
        else {
            dst[0] = _mm_min_epu8(dst[0], src[0]);
            dst[1] = _mm_min_epu8(dst[1], src[1]);
        }
        prevILineNum = iLineNum;
        scaledLineNum += lineStep;
    }
    
    __m128i* mask = (__m128i*) m_scaledMask;
     for (int i = 0; i < m_scaledWidth; ++i)
        mask[i] = _mm_sub_epi8(mask[i], sse_0x80_const);
}

void QnMotionEstimation::reallocateMask(int width, int height)
{
    qFreeAligned(m_scaledMask);
    qFreeAligned(m_frameBuffer[0]);
    qFreeAligned(m_frameBuffer[1]);
    delete [] m_resultMotion;

    m_lastImgWidth = width;
    m_lastImgHeight = height;
    // calculate scaled image size and allocate memory
    for (m_xStep = 8; m_lastImgWidth/m_xStep > MD_WIDTH; m_xStep += 8);
    if (m_lastImgWidth/m_xStep <= (MD_WIDTH*3)/4 && m_xStep > 8)
        m_xStep -= 8;
    m_scaledWidth = m_lastImgWidth / m_xStep;
    m_scaledMask = (quint8*) qMallocAligned(MD_HEIGHT * m_scaledWidth, 32);
    m_frameBuffer[0] = (quint8*) qMallocAligned(MD_HEIGHT * m_scaledWidth, 32);
    m_frameBuffer[1] = (quint8*) qMallocAligned(MD_HEIGHT * m_scaledWidth, 32);
    m_resultMotion = new quint32[MD_HEIGHT/4 * m_scaledWidth];
    memset(m_resultMotion, 0, MD_HEIGHT * m_scaledWidth);

    // scale motion mask to scaled size
    if (m_scaledWidth != MD_WIDTH)
        scaleMask();
    else
        memcpy(m_scaledMask, m_motionMask, MD_WIDTH * MD_HEIGHT);
    m_isNewMask = false;
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

    int idx = m_totalFrames % 2;
    int prevIdx = (m_totalFrames-1) % 2;

    if (!m_decoder->decode(videoData, m_frames[idx]))
        return;
    if (m_firstFrameTime == AV_NOPTS_VALUE)
        m_firstFrameTime = m_frames[idx]->pkt_dts;
    m_lastFrameTime = m_frames[idx]->pkt_dts;

#if 0
    // test
    fillFrameRect(m_frames[idx], QRect(0,0,m_frames[idx]->width, m_frames[idx]->height), 130);
    if (m_totalFrames % 2 == 1)
        fillFrameRect(m_frames[idx], QRect(QPoint(0, 0), QPoint(m_frames[idx]->width, m_frames[idx]->height/2)), 40);
    else
        fillFrameRect(m_frames[idx], QRect(QPoint(0, m_frames[idx]->height/2), QPoint(m_frames[idx]->width, m_frames[idx]->height)), 40);
#endif

    if (m_decoder->getWidth() != m_lastImgWidth || m_decoder->getHeight() != m_lastImgHeight || m_isNewMask)
        reallocateMask(m_decoder->getWidth(), m_decoder->getHeight());

#define ANALIZE_PER_PIXEL_DIF
#ifdef ANALIZE_PER_PIXEL_DIF
    if (m_totalFrames > 0)
    {
        if (m_xStep == 8)
            getFrame_avgY_array_8_x (m_frames[idx], m_frames[prevIdx], m_frameBuffer[idx]);
        else if (m_xStep == 16)
            getFrame_avgY_array_16_x(m_frames[idx], m_frames[prevIdx], m_frameBuffer[idx]);
        else
            getFrame_avgY_array_x_x (m_frames[idx], m_frames[prevIdx], m_frameBuffer[idx], m_xStep);

        // calculate difference between frames
        __m128i* cur = (__m128i*) m_frameBuffer[idx];
        __m128i* prev = (__m128i*) m_frameBuffer[prevIdx];
        __m128i* mask = (__m128i*) m_scaledMask;
        for (int i = 0; i < m_scaledWidth; ++i)
        {
            __m128i rez1 = _mm_cmpgt_epi8(_mm_sub_epi8(*cur, sse_0x80_const), mask[0]);
            __m128i rez2 = _mm_cmpgt_epi8(_mm_sub_epi8(cur[1], sse_0x80_const), mask[1]);

            m_resultMotion[i] |= reverseBits( (_mm_movemask_epi8(rez2) << 16) + _mm_movemask_epi8(rez1));
            cur += 2;
            prev += 2;
            mask += 2;
        }
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

    postFiltering();

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

void QnMotionEstimation::setMotionMask(const QByteArray& mask)
{
    QMutexLocker lock(&m_mutex);
    Q_ASSERT(mask.size() == MD_WIDTH * MD_HEIGHT);
    qFreeAligned(m_motionMask);
    m_motionMask = (quint8*) qMallocAligned(MD_WIDTH * MD_HEIGHT, 32);
    memcpy(m_motionMask, mask.data(), MD_WIDTH * MD_HEIGHT);
    m_isNewMask = true;
}

bool QnMotionEstimation::existsMetadata() const
{
    return m_lastFrameTime - m_firstFrameTime >= 300; // 30 ms agg period
}



    /*
    QFile testFile("c:/test.264");
    testFile.open(QFile::ReadOnly);
    QByteArray tmpData = testFile.readAll();
    videoData = QnCompressedVideoDataPtr(new QnCompressedVideoData(tmpData.size(), 32));
    videoData->data.write(tmpData);
    videoData->compressionType = CODEC_ID_H264;
    videoData->flags = AV_PKT_FLAG_KEY;
    */
    /*
    QTime t;
    t.restart();
    for (int i = 0; i < 1000; ++i)
    {
        if (!m_decoder->decode(videoData, &m_outFrame))
            return;
    }
    qDebug() << "decode time=" << t.elapsed();
    */

