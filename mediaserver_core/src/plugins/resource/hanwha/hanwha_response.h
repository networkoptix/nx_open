#pragma once

#include <map>

#include <boost/optional/optional.hpp>

#include <nx/network/buffer.h>

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
    explicit HanwhaResponse();
    HanwhaResponse(const nx::Buffer& rawBuffer);

    bool isSuccessful() const;
    int errorCode() const;
    QString errorString() const;
    std::map<QString, QString> response() const;
    
    template<typename T>
    boost::optional<T> parameter(const QString& parameterName) const
    {
        static_assert(std::is_same<T, int>::value
            || std::is_same<T, double>::value
            || std::is_same<T, QString>::value
            || std::is_same<T, bool>::value,
            "Only QString, int, double and bool specializations are allowed");
    }

private:
    void parseBuffer(const nx::Buffer& rawBuffer);
    boost::optional<QString> findParameter(const QString& parameterName) const;
    
private:
    int m_errorCode = HanwhaError::kNoError;
    QString m_errorString;
    std::map<QString, QString> m_response;
};

template<>
boost::optional<bool> HanwhaResponse::parameter<bool>(const QString& parameterName) const;

template<>
boost::optional<int> HanwhaResponse::parameter<int>(const QString& parameterName) const;

template<>
boost::optional<double> HanwhaResponse::parameter<double>(const QString& parameterName) const;

template<>
boost::optional<QString> HanwhaResponse::parameter<QString>(const QString& parameterName) const;

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
