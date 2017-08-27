#pragma once

#include <map>

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

    bool isSucccessful() const;
    int errorCode() const;
    QString errorString() const;
    std::map<QString, QString> response() const;

private:
    void parseBuffer(const nx::Buffer& rawBuffer);
    
private:
    int m_errorCode = HanwhaError::kNoError;
    QString m_errorString;
    std::map<QString, QString> m_response;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
