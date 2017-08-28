#pragma once

#include <plugins/resource/hanwha/hanwha_common.h>

#include <QtCore/QSize>

#include <boost/optional/optional.hpp>

#include <common/common_globals.h>
#include <utils/media/h264_common.h>
#include <utils/media/hevc_common.h>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

namespace nx {
namespace mediaserver_core {
namespace plugins {

QString channelParameter(int channelNumber, const QString& parameterName);

boost::optional<bool> toBool(const boost::optional<QString>& str);

boost::optional<int> toInt(const boost::optional<QString>& str);

boost::optional<double> toDouble(const boost::optional<QString>& str);

template<typename T>
T fromHanwhaString(const QString& str)
{
    static_assert(std::is_same<T, bool>::value
        || std::is_same<T, int>::value
        || std::is_same<T, AVCodecID>::value
        || std::is_same<T, QSize>::value
        || std::is_same<T, Qn::BitrateControl>::value
        || std::is_same<T, Qn::EncodingPriority>::value
        || std::is_same<T, Qn::EntropyCoding>::value
        || std::is_same<T, nx::media_utils::h264::Profile>::value
        || std::is_same<T, nx::media_utils::hevc::Profile>::value
        || std::is_same<T, HanwhaMediaType>::value
        || std::is_same<T, HanwhaStreamingMode>::value
        || std::is_same<T, HanwhaStreamingType>::value
        || std::is_same<T, HanwhaTransportProtocol>::value
        || std::is_same<T, HanwhaClientType>::value
        "No specialization for type");
}

template<>
bool fromHanwhaString(const QString& str);

template<>
int fromHanwhaString(const QString& str);

template<>
AVCodecID fromHanwhaString(const QString& str);

template<>
QSize fromHanwhaString(const QString& str);

template<>
Qn::BitrateControl fromHanwhaString(const QString& str);

template<>
Qn::EncodingPriority fromHanwhaString(const QString& str);

template<>
Qn::EntropyCoding fromHanwhaString(const QString& str);

template<>
nx::media_utils::h264::Profile fromHanwhaString(const QString& str);

template<>
nx::media_utils::hevc::Profile fromHanwhaString(const QString& str);

template<>
HanwhaMediaType fromHanwhaString(const QString& str);

template<>
HanwhaStreamingMode fromHanwhaString(const QString& str);

template<>
HanwhaStreamingType fromHanwhaString(const QString& str);

template<>
HanwhaTransportProtocol fromHanwhaString(const QString& str);

template<>
HanwhaClientType fromHanwhaString(const QString& str);


QString toHanwhaString(bool value);

QString toHanwhaString(AVCodecID codecId);

QString toHanwhaString(const QSize& value);

QString toHanwhaString(Qn::BitrateControl codecId);

QString toHanwhaString(Qn::EncodingPriority encodingPriority);

QString toHanwhaString(Qn::EntropyCoding entropyCoding);

QString toHanwhaString(HanwhaMediaType mediaType);

QString toHanwhaString(HanwhaStreamingMode streamingMode);

QString toHanwhaString(HanwhaStreamingType encodingPriority);

QString toHanwhaString(HanwhaTransportProtocol codecId);

QString toHanwhaString(HanwhaClientType entropyCoding);

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
