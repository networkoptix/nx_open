#include "hanwha_response.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

const QString kErrorStart = lit("ng");
const QByteArray kErrorCodePrefix("error code:");
const QByteArray kErrorDetailsPrefix("error details:");
const QString kUnknownErrorString = lit("unknown error");

} // namespace

HanwhaResponse::HanwhaResponse(const nx::Buffer& rawBuffer)
{
    parseBuffer(rawBuffer);
}

HanwhaResponse::HanwhaResponse():
    m_errorCode(HanwhaError::kUnknownError),
    m_errorString(kUnknownErrorString)
{
}

bool HanwhaResponse::isSucccessful() const
{
    return m_errorCode == HanwhaError::kNoError;
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

void HanwhaResponse::parseBuffer(const nx::Buffer& rawBuffer)
{
    auto lines = rawBuffer.split(L'\n');
    bool isError = false;
    bool gotErrorDetails = false;

    for (const auto& line: lines)
    {
        auto trimmed = line.trimmed().toLower();
        if (trimmed == kErrorStart)
        {
            isError = true;
            continue;
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

            m_response[split[0].trimmed()] = split[1].trimmed();
            m_errorCode = HanwhaError::kNoError;
        }
    }
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx