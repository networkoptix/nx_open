#pragma once

#include <QtCore/QString>

extern "C"
{
    #include <libavcodec/avcodec.h>
};

namespace nx::rtsp {

AVCodecID findEncoderCodecId(const QString& codecName);

} // namespace nx::rtsp
