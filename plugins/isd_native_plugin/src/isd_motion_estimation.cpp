#include "isd_motion_estimation.h"

#ifndef WIN32
#   include <sys/socket.h>
#else
#   include <WinSock2.h>
#   
#endif
#include <assert.h>
#include "plugins/resources/third_party/motion_data_picture.h"

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
    m_scaledWidth(0),
    m_resultMotion(0),
    m_scaledMask(0),
    m_filteredFrame(0),
    m_linkedNums(0),
    m_motionSensScaledMask(0),
    m_totalFrames(0),
    m_motionMask(0),
    m_motionSensMask(0),

    m_lastImgWidth(0),
    m_lastImgHeight(0),
    m_isNewMask(false),
    m_xAggregateCoeff(0)
{
    for (int i = 0; i < FRAMES_BUFFER_SIZE; ++i)
        m_frameBuffer[i] = 0;

    m_motionMask = (uint8_t*) nxpt::mallocAligned(MD_WIDTH * MD_HEIGHT, 32);
    m_motionSensMask = (uint8_t*) nxpt::mallocAligned(MD_WIDTH * MD_HEIGHT, 32);
    memset(m_motionMask, 255, MD_WIDTH * MD_HEIGHT);
    memset(sensitivityToMask, 0, MD_WIDTH * MD_HEIGHT);
}

#define min(a,b) (a < b ? a : b)

ISDMotionEstimation::~ISDMotionEstimation()
{
    nxpt::freeAligned(m_scaledMask);
    nxpt::freeAligned(m_motionSensScaledMask);
    for (int i = 0; i < FRAMES_BUFFER_SIZE; ++i)
        nxpt::freeAligned(m_frameBuffer[i]);
    nxpt::freeAligned(m_filteredFrame);
    nxpt::freeAligned(m_motionMask);
    nxpt::freeAligned(m_motionSensMask);
    nxpt::freeAligned(m_linkedNums);
}


// rescale motion mask width (mask rotated, so actually remove or duplicate some lines)
void ISDMotionEstimation::scaleMask(uint8_t* mask, uint8_t* scaledMask)
{
    float lineStep = (m_scaledWidth-1) / float(MD_WIDTH-1);
    float scaledLineNum = 0;
    int prevILineNum = -1;
    for (int y = 0; y < MD_WIDTH; ++y)
    {
        int iLineNum = (int) (scaledLineNum+0.5);
        assert(iLineNum < m_scaledWidth);
        //simd128i* dst = (simd128i*) (scaledMask + MD_HEIGHT*iLineNum);
        //simd128i* src = (simd128i*) (mask + MD_HEIGHT*y);

        uint8_t* dst = scaledMask + MD_HEIGHT*iLineNum;
        uint8_t* src = mask + MD_HEIGHT*y;
        if (iLineNum > prevILineNum) 
        {
            for (int i = 0; i < iLineNum - prevILineNum; ++i) 
                memcpy(dst - i*MD_HEIGHT , src, MD_HEIGHT);
        }
        else {
            for (int i = 0; i < 32; ++i)
                dst[i] = min(dst[i], src[i]); // SSE2
        }
        prevILineNum = iLineNum;
        scaledLineNum += lineStep;
    }
}

void ISDMotionEstimation::reallocateMask(int width, int height)
{
    nxpt::freeAligned(m_scaledMask);
    nxpt::freeAligned(m_motionSensScaledMask);
    for (int i = 0; i < FRAMES_BUFFER_SIZE; ++i)
        nxpt::freeAligned(m_frameBuffer[i]);
    nxpt::freeAligned(m_filteredFrame);
    nxpt::freeAligned(m_linkedNums);
    delete [] m_resultMotion;

    m_lastImgWidth = width;
    m_lastImgHeight = height;
    
    m_scaledWidth = width;
    m_xAggregateCoeff = 1;
    while (m_xAggregateCoeff < 16) {
        if (width % m_xAggregateCoeff == 0 && m_scaledWidth/(float)m_xAggregateCoeff <MD_WIDTH * 1.5)
            break;
        m_xAggregateCoeff++;
    }
    m_scaledWidth = width / m_xAggregateCoeff;

    int swUp = m_scaledWidth+1; // reserve one extra column because of analize_frame function for x8 width can write 1 extra byte
    m_scaledMask = (uint8_t*) nxpt::mallocAligned(MD_HEIGHT * swUp, 32);
    m_linkedNums = (int*) nxpt::mallocAligned(MD_HEIGHT * swUp * sizeof(int), 32);
    m_motionSensScaledMask = (uint8_t*) nxpt::mallocAligned(MD_HEIGHT * swUp, 32);
    m_frameBuffer[0] = (uint8_t*) nxpt::mallocAligned(MD_HEIGHT * swUp, 32);
    m_frameBuffer[1] = (uint8_t*) nxpt::mallocAligned(MD_HEIGHT * swUp, 32);
    m_filteredFrame = (uint8_t*) nxpt::mallocAligned(MD_HEIGHT * swUp, 32);
    m_resultMotion = new uint32_t[MD_HEIGHT/4 * swUp];
    memset(m_resultMotion, 0, MD_HEIGHT * swUp);

    // scale motion mask to scaled 
    if (m_scaledWidth != MD_WIDTH) {
        scaleMask(m_motionMask, m_scaledMask);
        scaleMask(m_motionSensMask, m_motionSensScaledMask);
    }
    else {
        memcpy(m_scaledMask, m_motionMask, MD_WIDTH * MD_HEIGHT);
        memcpy(m_motionSensScaledMask, m_motionSensMask, MD_WIDTH * MD_HEIGHT);
    }
    m_isNewMask = false;
    
    float yCoeff = height / (float)MD_HEIGHT;
    m_yStep = (int) yCoeff;
    m_yStepFrac = (yCoeff - m_yStep) * 65536;
}

void ISDMotionEstimation::scaleFrame(const uint8_t* data, int width, int height, int stride, uint8_t* frameBuffer)
{
    // scale frame to m_scaledWidth* MD_HEIGHT and rotate it to 90 degree
    uint8_t* dst = frameBuffer;
    const uint8_t* src = data;
    int xMax = width / m_xAggregateCoeff;
    
    int curLine = 0;
    int curLineFrac = 0;
    while (curLine < height)
    {
        int nextLine = curLine + m_yStep;
        curLineFrac += m_yStepFrac;
        if (curLineFrac >= 32768) {
            curLineFrac -= 65536; // fixed point arithmetic
            nextLine++;
        }
        int lines = nextLine - curLine;
        uint8_t* curDst = dst;
        for (int x = 0; x < xMax; ++x)
        {
            // aggregate pixel
            uint16_t pixel = 0;
            const uint8_t* srcPtr = src;
            for (int y = 0; y < lines; ++y) 
            {
                for (int rep = 0; rep < m_xAggregateCoeff; ++rep)
                    pixel += *srcPtr++;
                srcPtr += stride - m_xAggregateCoeff; // go to next src line
            }
            pixel /= (lines*m_xAggregateCoeff); // get avg value
            *curDst = pixel;
            src += m_xAggregateCoeff;
            curDst += MD_HEIGHT;
        }
        src += lines * stride - width;
        dst++;
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
    scaleFrame(data, width, height, stride, m_frameBuffer[idx]);
    analizeMotionAmount(m_frameBuffer[idx]);
    if (m_totalFrames == 0)
        m_totalFrames++;

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

    // 6. remove motion if motion square is not enough and write result to bitarray
    for (int i = 0; i < MD_HEIGHT*m_scaledWidth;)
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

    // scale result motion (height already valid, scale width ony. Data rotates, so actually duplicate or remove some lines
    int lineStep = (m_scaledWidth*65536) / MD_WIDTH;
    int scaledLineNum = 0;
    int prevILineNum = -1;
    uint32_t* dst = (uint32_t*) motionData->data();

    //postFiltering();

    for (int y = 0; y < MD_WIDTH; ++y)
    {
        int iLineNum = (scaledLineNum+32768) >> 16;
        if (iLineNum > prevILineNum) 
        {
            dst[y] = m_resultMotion[iLineNum];
            for (int i = 1; i < iLineNum - prevILineNum; ++i)
                dst[y] |= m_resultMotion[iLineNum+i];
        }
        else {
            dst[y] |= m_resultMotion[iLineNum];
        }
        prevILineNum = iLineNum;
        scaledLineNum += lineStep;
    }

    memset(m_resultMotion, 0, MD_HEIGHT * m_scaledWidth);
    m_totalFrames++;
    return motionData;
}
