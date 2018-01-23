#pragma once

#include <map>

#include <boost/optional/optional.hpp>

#include <QDateTime>

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
    static const int kNoChannel = -1;

    HanwhaResponse() {}

    explicit HanwhaResponse(
        nx_http::StatusCode::Value statusCode,
        const QString& requestUrl);

    explicit HanwhaResponse(
        const nx::Buffer& rawBuffer,
        nx_http::StatusCode::Value statusCode,
        const QString& requestUrl,
        const QString& groupBy = QString(),
        bool isListMode = false);

    bool isSuccessful() const;
    int errorCode() const;
    QString errorString() const;
    std::map<QString, QString> response() const;
    nx_http::StatusCode::Value statusCode() const;
    QString requestUrl() const;

    template<typename T>
    boost::optional<T> parameter(const QString& parameterName, int channel = kNoChannel) const
    {
        static_assert(std::is_same<T, int>::value
            || std::is_same<T, double>::value
            || std::is_same<T, QString>::value
            || std::is_same<T, bool>::value
            || std::is_same<T, AVCodecID>::value
            || std::is_same<T, QSize>::value,
            "Only QString, int, double, bool, AVCodecID, QSize specializations are allowed");
    }

    template<typename T = QString>
    T getOrThrow(const QString& name)
    {
        if (auto value = parameter<T>(name))
            return *value;

        static const auto errorMessage = QStringLiteral("Paramiter %1 is not found in response");
        throw std::runtime_error(errorMessage.arg(name).toStdString()) ;
    }

private:
    void parseBuffer(const nx::Buffer& rawBuffer, bool isListMode = false);
    boost::optional<QString> findParameter(const QString& parameterName, int channel) const;

private:
    int m_errorCode = HanwhaError::kNoError;
    QString m_errorString;
    std::map<QString, QString> m_response;
    nx_http::StatusCode::Value m_statusCode = nx_http::StatusCode::undefined;
    QString m_groupBy;
    QString m_requestUrl;
};

template<>
boost::optional<bool> HanwhaResponse::parameter<bool>(const QString& parameterName, int channel) const;

template<>
boost::optional<int> HanwhaResponse::parameter<int>(const QString& parameterName, int channel) const;

template<>
boost::optional<double> HanwhaResponse::parameter<double>(const QString& parameterName, int channel) const;

template<>
boost::optional<AVCodecID> HanwhaResponse::parameter<AVCodecID>(const QString& parameterName, int channel) const;

template<>
boost::optional<QSize> HanwhaResponse::parameter<QSize>(const QString& parameterName, int channel) const;

template<>
boost::optional<QString> HanwhaResponse::parameter<QString>(const QString& parameterName, int channel) const;

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
