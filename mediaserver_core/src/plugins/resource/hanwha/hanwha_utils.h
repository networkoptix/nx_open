#pragma once

#include <plugins/resource/hanwha/hanwha_common.h>

#include <set>

#include <QtCore/QSize>

#include <boost/optional/optional.hpp>

#include <common/common_globals.h>
#include <utils/media/h264_common.h>
#include <utils/media/hevc_common.h>
#include <utils/camera/camera_diagnostics.h>

#include <plugins/resource/hanwha/hanwha_response.h>
#include <plugins/resource/hanwha/hanwha_video_profile.h>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

namespace nx {
namespace mediaserver_core {
namespace plugins {

using HanwhaProfileNumber = int;
using HanwhaChannelNumber = int;
using HanwhaProfileMap = std::map<HanwhaProfileNumber, HanwhaVideoProfile>;
using HanwhaChannelProfiles = std::map<HanwhaChannelNumber, HanwhaProfileMap>;

using HanwhaProfileParameterName = QString;
using HanwhaProfileParameterValue = QString;
using HanwhaProfileParameters = std::map<QString, QString>;

template<typename T>
CameraDiagnostics::Result error(
    const T& response,
    const CameraDiagnostics::Result& authorizedResult)
{
    auto statusCode = response.statusCode();
    bool unauthorizedResponse = statusCode == nx_http::StatusCode::unauthorized
        || statusCode == nx_http::StatusCode::notAllowed;

    if (!unauthorizedResponse)
        return authorizedResult;

    return CameraDiagnostics::NotAuthorisedResult(authorizedResult.errorParams[0]);
}

QStringList fromHanwhaInternalRange(const QStringList& internalRange);

QString channelParameter(int channelNumber, const QString& parameterName);

boost::optional<bool> toBool(const boost::optional<QString>& str);

boost::optional<int> toInt(const boost::optional<QString>& str);

boost::optional<double> toDouble(const boost::optional<QString>& str);

boost::optional<AVCodecID> toCodecId(const boost::optional<QString>& str);

boost::optional<QSize> toQSize(const boost::optional<QString>& str);

HanwhaChannelProfiles parseProfiles(const HanwhaResponse& response);

QString nxProfileName(Qn::ConnectionRole role);

bool isNxProfile(const QString& profileName);

template<typename T>
T fromHanwhaString(const QString& str)
{
    static_assert(std::is_same<T, bool>::value
        || std::is_same<T, AVCodecID>::value
        || std::is_same<T, QSize>::value
        || std::is_same<T, Qn::BitrateControl>::value
        || std::is_same<T, Qn::EncodingPriority>::value
        || std::is_same<T, Qn::EntropyCoding>::value
        || std::is_same<T, HanwhaMediaType>::value
        || std::is_same<T, HanwhaStreamingMode>::value
        || std::is_same<T, HanwhaStreamingType>::value
        || std::is_same<T, HanwhaTransportProtocol>::value
        || std::is_same<T, HanwhaClientType>::value,
        "No specialization for type");
}

template<typename T>
T fromHanwhaString(const QString& str, bool* outSuccess)
{
    static_assert(std::is_same<T, int>::value, "No specialization for type");
}

template<>
int fromHanwhaString<int>(const QString& str, bool* outSuccess);

template<>
bool fromHanwhaString(const QString& str);

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

bool areaComparator(const QString& lhs, const QString& rhs);

bool ratioComparator(const QString& lhs, const QString& rhs);

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
