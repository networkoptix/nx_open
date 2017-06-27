#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>

#include <errno.h>
#include <fstream>
#include <iostream>
#include <linux/videodev2.h>
#include <malloc.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>

#include "NvUtils.h"
#include "NvCudaProc.h"
#include "nvbuf_utils.h"
#include "video_dec_gie.h"
#include "gie_inference.h"

class LockGurad
{
public:
    LockGurad(pthread_mutex_t* mutex): m_mutex(mutex) { pthread_mutex_lock(m_mutex); }
    ~LockGurad() { pthread_mutex_unlock(m_mutex); }

private:
    pthread_mutex_t* m_mutex;
};

class Detector
{
public:
    Detector():
        m_firstTimePush(true),
        m_firstTimePushBufferNum(0)
    {
    }

    int startInference(std::string modelFileName, std::string deployFileName, std::string cacheFileName);
    bool stopSync();
    bool pushCompressedFrame(const uint8_t* data, int dataSize);
    bool hasRectangles();
    std::vector<cv::Rect> getRectangles();
    int getNetWidth() const;
    int getNetHeight() const;

private:
    int fillBuffer(const uint8_t* data, int dataSize, NvBuffer* buffer);

private:
    std::mutex m_mutex;
    std::queue<std::vector<cv::Rect>> m_rects;
    context_t m_ctx;
    bool m_firstTimePush;
    int m_firstTimePushBufferNum;
};
