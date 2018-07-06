#pragma once

#include <plugins/resource/hanwha/hanwha_resource.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/aio/timer.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaPtzExecutor
{
    struct ParameterContext
    {
        std::set<int> range;
        int speed = 0;
        std::unique_ptr<nx::network::http::AsyncClient> httpClient;
        std::unique_ptr<nx::network::aio::Timer> timer;
    };

public:
    HanwhaPtzExecutor(
        const HanwhaResourcePtr& hanwhaResource,
        const std::map<QString, std::set<int>>& ranges);
    virtual ~HanwhaPtzExecutor();

    void setSpeed(const QString& parameterName, qreal speed);

private:
    void startMovement(const QString& parameterName);
    void scheduleNextRequest(const QString& parameterName);
    void sendValueToDevice(const QString& parameterName, int parameterValue);

    std::unique_ptr<nx::network::http::AsyncClient> makeHttpClient() const;
    boost::optional<int> toHanwhaSpeed(const QString& parameterName, qreal speed) const;
    ParameterContext& context(const QString& parameterName);
    std::set<int> range(const QString& parameterName) const;

private:
    HanwhaResourcePtr m_hanwhaResource;
    std::map<QString, ParameterContext> m_parameterContexts;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
