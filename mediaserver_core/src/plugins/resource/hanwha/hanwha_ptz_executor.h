#pragma once

#include <plugins/resource/hanwha/hanwha_resource.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/aio/timer.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

struct HanwhaAlternativePtzParameterContext
{
    std::set<int> range;
    int speed = 0;
    std::unique_ptr<nx_http::AsyncClient> httpClient;
    std::unique_ptr<nx::network::aio::Timer> timer;
};

class HanwhaPtzExecutor
{
public:
    HanwhaPtzExecutor(const HanwhaResourcePtr& hanwhaResource);
    virtual ~HanwhaPtzExecutor();

    // Should not be called after first call to setSpeed
    void setRange(const QString& parameterName, const std::set<int>& range);
    void setSpeed(const QString& parameterName, qreal speed);

private:
    void startMovement(const QString& parameterName);
    void scheduleNextRequest(const QString& parameterName);
    void doRequest(const QString& parameterName, int parameterValue);

    QUrl makeUrl(const QString& parameterName, int parameterValue);
    std::unique_ptr<nx_http::AsyncClient> makeHttpClient();
    boost::optional<int> toHanwhaSpeed(const QString& parameterName, qreal speed);
    HanwhaAlternativePtzParameterContext& context(const QString& parameterName);

private:
    HanwhaResourcePtr m_hanwhaResource;
    std::map<QString, HanwhaAlternativePtzParameterContext> m_parameterContexts;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
