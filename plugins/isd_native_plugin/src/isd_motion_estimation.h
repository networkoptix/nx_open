#ifndef __ISD_MOTION_ESTIMATION_H__
#define __ISD_MOTION_ESTIMATION_H__

#include <stdint.h>
#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include "mutex.h"

class MotionDataPicture;

class ISDMotionEstimation
{
public:
    ISDMotionEstimation();
    ~ISDMotionEstimation();

    static const int FRAMES_BUFFER_SIZE = 2;
    static const int MD_WIDTH = nxcip::DEFAULT_MOTION_DATA_PICTURE_WIDTH;
    static const int MD_HEIGHT = nxcip::DEFAULT_MOTION_DATA_PICTURE_HEIGHT;

    void setMotionMask(const uint8_t* mask);
    bool analizeFrame(const uint8_t* data, int width, int height, int stride);
    MotionDataPicture* getMotion();
private:
    void analizeMotionAmount(uint8_t* frame);
    void reallocateMask(int width, int height);
    void scaleFrame(const uint8_t* data, int width, int height, int stride, uint8_t* frameBuffer, uint8_t* prevFrameBuffer, uint8_t* deltaBuffer);
private:
    Mutex m_mutex;
    uint32_t* m_resultMotion;
    uint8_t* m_scaledMask; 
    uint8_t* m_frameDeltaBuffer;
    uint8_t* m_frameBuffer[FRAMES_BUFFER_SIZE];
    uint8_t* m_filteredFrame;
    int m_linkedMap[MD_WIDTH*MD_HEIGHT];
    int* m_linkedNums;
    int m_linkedSquare[MD_WIDTH*MD_HEIGHT];
    uint8_t* m_motionSensScaledMask;
    int m_totalFrames;
    uint8_t* m_motionMask;
    uint8_t* m_motionSensMask;
    
    int m_scaleXStep;
    int m_scaleYStep;
    
    int m_lastImgWidth;
    int m_lastImgHeight;
    bool m_isNewMask;
};


#endif // __ISD_MOTION_ESTIMATION_H__
