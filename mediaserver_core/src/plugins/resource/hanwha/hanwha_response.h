#pragma once

#include <map>

#include <boost/optional/optional.hpp>

#include <nx/network/buffer.h>
#include <nx/network/http/http_types.h>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace HanwhaError
{

static const int kNoError = 0;
static const int kUnknownError = 1;

}

class HanwhaResponse
{
public:
    explicit HanwhaResponse(nx_http::StatusCode::Value statusCode);
    explicit HanwhaResponse(
        const nx::Buffer& rawBuffer,
        nx_http::StatusCode::Value statusCode,
        const QString& groupBy = QString());

    bool isSuccessful() const;
    int errorCode() const;
    QString errorString() const;
    std::map<QString, QString> response() const;
    nx_http::StatusCode::Value statusCode() const;
    
    template<typename T>
    boost::optional<T> parameter(const QString& parameterName) const
    {
        static_assert(std::is_same<T, int>::value
            || std::is_same<T, double>::value
            || std::is_same<T, QString>::value
            || std::is_same<T, bool>::value
            || std::is_same<T, AVCodecID>::value
            || std::is_same<T, QSize>::value,
            "Only QString, int, double, bool, AVCodecID, QSize specializations are allowed");
    }

private:
    void parseBuffer(const nx::Buffer& rawBuffer);
    boost::optional<QString> findParameter(const QString& parameterName) const;
    
private:
    int m_errorCode = HanwhaError::kNoError;
    QString m_errorString;
    std::map<QString, QString> m_response;
    nx_http::StatusCode::Value m_statusCode = nx_http::StatusCode::undefined;
    QString m_groupBy;
};

template<>
boost::optional<bool> HanwhaResponse::parameter<bool>(const QString& parameterName) const;

template<>
boost::optional<int> HanwhaResponse::parameter<int>(const QString& parameterName) const;

template<>
boost::optional<double> HanwhaResponse::parameter<double>(const QString& parameterName) const;

template<>
boost::optional<AVCodecID> HanwhaResponse::parameter<AVCodecID>(const QString& parameterName) const;

template<>
boost::optional<QSize> HanwhaResponse::parameter<QSize>(const QString& parameterName) const;

template<>
boost::optional<QString> HanwhaResponse::parameter<QString>(const QString& parameterName) const;

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
