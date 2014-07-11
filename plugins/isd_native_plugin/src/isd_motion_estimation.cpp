#include "isd_motion_estimation.h"

#ifndef WIN32
#   include <arpa/inet.h>
#else
#   include <WinSock2.h>
#   
#endif
#include <assert.h>
#include <plugins/resource/third_party/motion_data_picture.h>
#include "limits.h"

static const int sensitivityToMask[10] = 
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
    9  // 9
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


ISDMotionEstimation::ISDMotionEstimation():
    m_resultMotion(nullptr),
    m_scaledMask(nullptr),
    m_frameDeltaBuffer(nullptr),
    m_filteredFrame(nullptr),
    m_linkedNums(nullptr),
    m_motionSensScaledMask(nullptr),
    m_totalFrames(1),
    m_motionMask(nullptr),
    m_motionSensMask(nullptr),

    m_scaleXStep(0),
    m_scaleYStep(0),
    m_lastImgWidth(0),
    m_lastImgHeight(0),
    m_isNewMask(false)
{
    m_frameDeltaBuffer = 0;
    for (int i = 0; i < FRAMES_BUFFER_SIZE; ++i)
        m_frameBuffer[i] = 0;

    m_motionMask = (uint8_t*) nxpt::mallocAligned(MD_WIDTH * MD_HEIGHT, 32);
    m_motionSensMask = (uint8_t*) nxpt::mallocAligned(MD_WIDTH * MD_HEIGHT, 32);
    memset(m_motionMask, 255, MD_WIDTH * MD_HEIGHT);
    memset(m_motionSensMask, 0, MD_WIDTH * MD_HEIGHT);
    //memset(m_motionMask, sensitivityToMask[6], MD_WIDTH * MD_HEIGHT);
    //memset(m_motionSensMask, 6, MD_WIDTH * MD_HEIGHT);
}

#define min(a,b) (a < b ? a : b)

ISDMotionEstimation::~ISDMotionEstimation()
{
    nxpt::freeAligned(m_scaledMask);
    nxpt::freeAligned(m_motionSensScaledMask);
    nxpt::freeAligned(m_frameDeltaBuffer);
    for (int i = 0; i < FRAMES_BUFFER_SIZE; ++i)
        nxpt::freeAligned(m_frameBuffer[i]);
    nxpt::freeAligned(m_filteredFrame);
    nxpt::freeAligned(m_motionMask);
    nxpt::freeAligned(m_motionSensMask);
    nxpt::freeAligned(m_linkedNums);
}

void ISDMotionEstimation::reallocateMask(int width, int height)
{
    nxpt::freeAligned(m_scaledMask);
    nxpt::freeAligned(m_motionSensScaledMask);
    nxpt::freeAligned(m_frameDeltaBuffer);
    for (int i = 0; i < FRAMES_BUFFER_SIZE; ++i)
        nxpt::freeAligned(m_frameBuffer[i]);
    nxpt::freeAligned(m_filteredFrame);
    nxpt::freeAligned(m_linkedNums);
    delete [] m_resultMotion;

    m_lastImgWidth = width;
    m_lastImgHeight = height;
    
    m_scaledMask = (uint8_t*) nxpt::mallocAligned(MD_HEIGHT * MD_WIDTH, 32);
    m_linkedNums = (int*) nxpt::mallocAligned(MD_HEIGHT * MD_WIDTH * sizeof(int), 32);
    m_motionSensScaledMask = (uint8_t*) nxpt::mallocAligned(MD_HEIGHT * MD_WIDTH, 32);
    m_frameDeltaBuffer = (uint8_t*) nxpt::mallocAligned(MD_HEIGHT * MD_WIDTH, 32);
    for (int i = 0; i < FRAMES_BUFFER_SIZE; ++i) {
        m_frameBuffer[i] = (uint8_t*) nxpt::mallocAligned(MD_HEIGHT * MD_WIDTH, 32);
        memset(m_frameBuffer[i], 0, MD_HEIGHT * MD_WIDTH);
    }
    m_filteredFrame = (uint8_t*) nxpt::mallocAligned(MD_HEIGHT * MD_WIDTH, 32);
    m_resultMotion = new uint32_t[MD_HEIGHT * MD_WIDTH / 32];
    memset(m_resultMotion, 0, MD_HEIGHT * MD_WIDTH / 8);

    // scale motion mask to scaled 
    memcpy(m_scaledMask, m_motionMask, MD_WIDTH * MD_HEIGHT);
    memcpy(m_motionSensScaledMask, m_motionSensMask, MD_WIDTH * MD_HEIGHT);
    m_isNewMask = false;
    
    m_scaleXStep = width * 65536 / MD_WIDTH;
    m_scaleYStep = height * 65536 / MD_HEIGHT;
}

void ISDMotionEstimation::scaleFrame(const uint8_t* data, int width, int height, int stride, uint8_t* frameBuffer,
                                     uint8_t* prevFrameBuffer, uint8_t* deltaBuffer)
{
    // scale frame to m_scaledWidth* MD_HEIGHT and rotate it to 90 degree
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
            uint8_t pixelDelta = abs(pixel - prevFrameBuffer[offset]);
            deltaBuffer[offset] = pixelDelta;

            src += xPixels;
            curDst += MD_HEIGHT;
            offset += MD_HEIGHT;
            curX = nextX;
        }
        src += lines * stride - width;
        dst++;
        offset = offset - (MD_HEIGHT*MD_WIDTH) + 1;
        curLine = nextLine;
    }
}

bool ISDMotionEstimation::analizeFrame(const uint8_t* data, int width, int height, int stride)
{
    Mutex::ScopedLock lock(&m_mutex);

    int idx = m_totalFrames % FRAMES_BUFFER_SIZE;
    int prevIdx = (m_totalFrames-1) % FRAMES_BUFFER_SIZE;


    if (width != m_lastImgWidth || height != m_lastImgHeight || m_isNewMask)
        reallocateMask(width, height);
    scaleFrame(data, width, height, stride, m_frameBuffer[idx], m_frameBuffer[prevIdx], m_frameDeltaBuffer);
    analizeMotionAmount(m_frameDeltaBuffer);

    return true;
}


void ISDMotionEstimation::analizeMotionAmount(uint8_t* frame)
{
    // 1. filtering. If a lot of motion around current pixel, mark it too, also remove motion if diff < mask
    uint8_t* curPtr = frame;
    uint8_t* dstPtr = m_filteredFrame;
    const uint8_t* maskPtr = m_scaledMask;

    for (int y = 0; y < MD_HEIGHT; ++y) {
        *dstPtr = *curPtr <= *maskPtr ? 0 : *curPtr;
        curPtr++;
        maskPtr++;
        dstPtr++;
    }

    for (int x = 1; x < MD_WIDTH-1; ++x)
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
    memset(m_linkedNums, 0, sizeof(int) * MD_HEIGHT * MD_WIDTH);
    memset(m_linkedSquare, 0, sizeof(m_linkedSquare));
    for (int i = 0; i < MD_HEIGHT*MD_WIDTH/2;++i)
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

    for (int x = 1; x < MD_WIDTH; ++x)
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
    for (int i = 0; i < MD_HEIGHT*MD_WIDTH; ++i)
        m_linkedNums[i] = m_linkedMap[m_linkedNums[i]];

    // 5. calculate square of each area
    for (int i = 0; i < MD_HEIGHT*MD_WIDTH; ++i)
    {
        m_linkedSquare[m_linkedNums[i]] += m_filteredFrame[i];
    }

    // 6. remove motion if motion square is not enough and write result to bitarray
    for (int i = 0; i < MD_HEIGHT*MD_WIDTH;)
    {
        uint32_t data = 0;
        for (int k = 0; k < 32; ++k, ++i) 
        {
            data = (data << 1) + int(m_linkedSquare[m_linkedNums[i]] > MIN_SQUARE_BY_SENS[m_motionSensScaledMask[i]]);
        }
        m_resultMotion[i/32-1] |= htonl(data);
    }
}

void ISDMotionEstimation::setMotionMask(const uint8_t* mask)
{
    Mutex::ScopedLock lock(&m_mutex);
    m_isNewMask = true;
    nxpt::freeAligned(m_motionMask);
    nxpt::freeAligned(m_motionSensMask);
    m_motionMask = (uint8_t*) nxpt::mallocAligned(MD_WIDTH * MD_HEIGHT, 32);
    m_motionSensMask = (uint8_t*) nxpt::mallocAligned(MD_WIDTH * MD_HEIGHT, 32);
    memcpy(m_motionMask, mask, MD_WIDTH * MD_HEIGHT);
    for (int i = 0; i < MD_WIDTH * MD_HEIGHT; ++i) 
    {
        m_motionSensMask[i] = 0;
        for (int j = 0; j < sizeof(sensitivityToMask)/sizeof(int); ++j)
        {
            if (sensitivityToMask[j] >= mask[i])
                m_motionSensMask[i] = j;
            else
                break;
        }
    }
}

MotionDataPicture* ISDMotionEstimation::getMotion()
{
    MotionDataPicture* motionData = new MotionDataPicture(nxcip::PIX_FMT_MONOBLACK);
    memcpy(motionData->data(), m_resultMotion, MD_WIDTH * MD_HEIGHT / 8);
    memset(m_resultMotion, 0, MD_HEIGHT * MD_WIDTH / 8);
    m_totalFrames++;
    return motionData;
}
