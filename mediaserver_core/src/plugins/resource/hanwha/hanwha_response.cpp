#include "hanwha_response.h"
#include "hanwha_utils.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

const QString kErrorStart = lit("NG");
const QByteArray kErrorCodePrefix("Error Code:");
const QByteArray kErrorDetailsPrefix("Error Details:");
const QString kUnknownErrorString = lit("Unknown error");

} // namespace


HanwhaResponse::HanwhaResponse(nx_http::StatusCode::Value statusCode):
    m_errorCode(HanwhaError::kUnknownError),
    m_errorString(kUnknownErrorString),
    m_statusCode(statusCode)
{
}

HanwhaResponse::HanwhaResponse(
    const nx::Buffer& rawBuffer,
    nx_http::StatusCode::Value statusCode,
    const QString& groupBy)
    :
    m_statusCode(statusCode),
    m_groupBy(groupBy)
{
    parseBuffer(rawBuffer);
}

bool HanwhaResponse::isSuccessful() const
{
    return m_errorCode == HanwhaError::kNoError
        && nx_http::StatusCode::isSuccessCode(m_statusCode);
}

int HanwhaResponse::errorCode() const
{
    return m_errorCode;
}

QString HanwhaResponse::errorString() const
{
    return m_errorString;
}

std::map<QString, QString> HanwhaResponse::response() const
{
    return m_response;
}

nx_http::StatusCode::Value HanwhaResponse::statusCode() const
{
    return m_statusCode;
}

void HanwhaResponse::parseBuffer(const nx::Buffer& rawBuffer)
{
    const auto groupBy = m_groupBy.toUtf8();
    auto lines = rawBuffer.split(L'\n');
    bool isError = false;
    bool gotErrorDetails = false;
    QString currentGroupPrefix;
    
    for (const auto& line: lines)
    {
        const auto trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;

        if (trimmed == kErrorStart)
        {
            isError = true;
            continue;
        }
        else if (!m_groupBy.isEmpty())
        {
            const auto split = trimmed.split('.');
            const auto splitSize = split.size();

            if (split.isEmpty())
                continue;

            for (auto i = 0; i < splitSize; ++i)
            {
                const auto part = split[i];
                const bool lastValue = i == splitSize - 1;

                if (part == groupBy && !lastValue)
                {
                    currentGroupPrefix = lit("%1.%2.")
                        .arg(QString::fromUtf8(groupBy))
                        .arg(QString::fromUtf8(split[i + 1]));

                    continue;
                }

                if (i == splitSize - 1)
                {
                    auto nameAndValue = part.split(L'=');

                    if (nameAndValue.size() != 2)
                        continue;

                    if (nameAndValue[0] == groupBy)
                    {
                        currentGroupPrefix = lit("%1.%2.")
                            .arg(QString::fromUtf8(groupBy))
                            .arg(QString::fromUtf8(nameAndValue[1]));
                    }

                    continue;
                }
            }
        }

        if (isError)
        {
            if (trimmed.startsWith(kErrorCodePrefix))
            {
                auto split = trimmed.split(L':');
                if (split.size() != 2)
                    continue;

                bool success = false;
                int errorCode = split[1].toInt(&success);

                if (success)
                    m_errorCode = errorCode;
                else
                    m_errorCode = HanwhaError::kUnknownError;
            }
            else if (trimmed.startsWith(kErrorDetailsPrefix))
            {
                gotErrorDetails = true;
                continue;
            }
            else if (gotErrorDetails)
            {
                m_errorString = trimmed;
            }
        }
        else
        {
            auto split = line.split(L'=');
            if (split.size() != 2)
                continue;

            m_response[currentGroupPrefix + split[0].trimmed()] = split[1].trimmed();
            m_errorCode = HanwhaError::kNoError;
        }
    }
}

boost::optional<QString> HanwhaResponse::findParameter(const QString& parameterName) const
{
    auto itr = m_response.find(parameterName);
    if (itr == m_response.cend())
        return boost::none;

    return itr->second;
}

template<>
boost::optional<bool> HanwhaResponse::parameter<bool>(const QString& parameterName) const
{
    return toBool(findParameter(parameterName));
}

template<>
boost::optional<int> HanwhaResponse::parameter<int>(const QString& parameterName) const
{
    return toInt(findParameter(parameterName));
}

template<>
boost::optional<double> HanwhaResponse::parameter<double>(const QString& parameterName) const
{
    return toDouble(findParameter(parameterName));
}

template<>
boost::optional<AVCodecID> HanwhaResponse::parameter<AVCodecID>(const QString& parameterName) const
{
    return toCodecId(findParameter(parameterName));
}

template<>
boost::optional<QSize> HanwhaResponse::parameter<QSize>(const QString& parameterName) const
{
    return toQSize(findParameter(parameterName));
}

template<>
boost::optional<QString> HanwhaResponse::parameter<QString>(const QString& parameterName) const
{
    return findParameter(parameterName);
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
