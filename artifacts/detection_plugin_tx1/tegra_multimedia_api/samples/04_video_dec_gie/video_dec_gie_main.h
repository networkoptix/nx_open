#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>

#include <errno.h>
#include <fstream>
#include <iostream>
#include <malloc.h>
#include <pthread.h>
#include <string.h>

#include <unistd.h>

#include "NvUtils.h"
#include "NvCudaProc.h"
#include "nvbuf_utils.h"
#include "video_dec_gie.h"
#include "gie_inference.h"

class Detector
{
public:
    Detector(const char* id):
        m_id(id),
        m_videoDecoderName(std::string("dec") + id),
        m_videoConverterName(std::string("conv") + id),
        m_firstTimePush(true),
        m_firstTimePushBufferNum(0)
    {
    }

    int startInference(
        const std::string& modelFileName,
        const std::string& deployFileName,
        const std::string& cacheFileName);

    bool stopSync();
    bool pushCompressedFrame(const uint8_t* data, int dataSize, int64_t ptUs);
    bool hasRectangles();
    std::vector<cv::Rect> getRectangles(int64_t* outPts);
    int getNetWidth() const;
    int getNetHeight() const;

private:
    int fillBuffer(const uint8_t* data, int dataSize, NvBuffer* buffer);

private:
    const std::string m_id;
    const std::string m_videoDecoderName;
    const std::string m_videoConverterName;
    std::mutex m_mutex;
    std::queue<std::vector<cv::Rect>> m_rects;
    context_t m_ctx;
    bool m_firstTimePush;
    int m_firstTimePushBufferNum;
};
