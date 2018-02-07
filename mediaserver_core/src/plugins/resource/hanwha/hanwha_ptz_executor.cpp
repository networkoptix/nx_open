#include "hanwha_ptz_executor.h"

#include <chrono>

#include <common/static_common_module.h>
#include <core/resource_management/resource_data_pool.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const std::chrono::milliseconds kHanwhaExecutorSendTimeout(5000);
static const std::chrono::milliseconds kHanwhaExecutorReceiveTimeout(5000);
static const std::chrono::milliseconds kRequestInterval(500);

} // namespace

HanwhaPtzExecutor::HanwhaPtzExecutor(const HanwhaResourcePtr& hanwhaResource):
    m_hanwhaResource(hanwhaResource)
{
    m_parameterContexts[kHanwhaZoomProperty] = HanwhaAlternativePtzParameterContext();
    m_parameterContexts[kHanwhaFocusProperty] = HanwhaAlternativePtzParameterContext();

    for (auto& item: m_parameterContexts)
    {
        auto& context = item.second;
        context.httpClient = makeHttpClient();
        context.timer = std::make_unique<nx::network::aio::Timer>();
        context.timer->bindToAioThread(context.httpClient->getAioThread());
    }
}

HanwhaPtzExecutor::~HanwhaPtzExecutor()
{
    for (auto& item: m_parameterContexts)
    {
        nx::utils::promise<void> promise;
        auto& context = item.second;
        context.httpClient->pleaseStop(
            [this, &context, &promise]()
            {
                context.timer->pleaseStopSync();
                promise.set_value();
            });

        promise.get_future().wait();
    }
}

void HanwhaPtzExecutor::setRange(const QString& parameterName, const std::set<int>& range)
{
    context(parameterName).range = range;
}

void HanwhaPtzExecutor::setSpeed(const QString& parameterName, qreal speed)
{
    auto& ctx = context(parameterName);
    ctx.timer->post(
        [this, parameterName, speed]()
        {
            const auto hanwhaSpeed = toHanwhaSpeed(parameterName, speed);
            if (hanwhaSpeed == boost::none)
            {
                NX_WARNING(
                    this,
                    lm("Can't convert internal speed to Hanwha. Parameter %1, speed %2")
                        .args(parameterName, speed));

                return;
            }
            context(parameterName).speed = hanwhaSpeed.get();
            startMovement(parameterName);
        });
}

void HanwhaPtzExecutor::startMovement(const QString& parameterName)
{
    const auto speed = context(parameterName).speed;
    if (speed == 0)
        scheduleNextRequest(parameterName);
    else
        doRequest(parameterName, speed);
}

void HanwhaPtzExecutor::scheduleNextRequest(const QString& parameterName)
{
    auto& timer = context(parameterName).timer;
    timer->start(kRequestInterval, [this, parameterName]() { startMovement(parameterName); });
}

void HanwhaPtzExecutor::doRequest(const QString& parameterName, int parameterValue)
{
    auto& timer = context(parameterName).timer;
    timer->cancelAsync(
        [this, parameterName, parameterValue]()
        {
            auto& httpClient = context(parameterName).httpClient;
            NX_ASSERT(httpClient, lm("Wrong parameter name: %1").arg(parameterName));
            httpClient->doGet(
                makeUrl(parameterName, parameterValue),
                [this, parameterName]() { scheduleNextRequest(parameterName); });
        });
}

nx::utils::Url HanwhaPtzExecutor::makeUrl(const QString& parameterName, int parameterValue)
{
    nx::utils::Url url(m_hanwhaResource->getUrl());
    url.setPath(lit("/stw-cgi/image.cgi"));

    QUrlQuery query;
    query.addQueryItem(lit("msubmenu"), lit("focus"));
    query.addQueryItem(lit("action"), lit("control"));
    query.addQueryItem(parameterName, QString::number(parameterValue));
    url.setQuery(query);
    return url;
}

std::unique_ptr<nx::network::http::AsyncClient> HanwhaPtzExecutor::makeHttpClient()
{
    auto auth = m_hanwhaResource->getAuth();
    auto httpClient = std::make_unique<nx::network::http::AsyncClient>();
    httpClient->setUserName(auth.user());
    httpClient->setUserPassword(auth.password());
    httpClient->setSendTimeout(kHanwhaExecutorSendTimeout);
    httpClient->setMessageBodyReadTimeout(kHanwhaExecutorReceiveTimeout);

    return httpClient;
}

boost::optional<int> HanwhaPtzExecutor::toHanwhaSpeed(const QString& parameterName, qreal speed)
{
    if (qFuzzyIsNull(speed))
        return 0;

    const auto& range = context(parameterName).range;
    if (range.empty())
        return boost::none;

    const auto resData = qnStaticCommon->dataPool()->data(m_hanwhaResource);
    const auto calibratedSpeed = resData.value<int>(
        lit("%1Alt%2Speed")
            .arg(speed > 0 ? lit("positive") : lit("negative"))
            .arg(parameterName),
        0);

    if (calibratedSpeed != 0)
        return calibratedSpeed;

    // Always use the maximum speed value for zoom, since others are too slow.
    if (parameterName == kHanwhaZoomProperty)
        return speed > 0 ? *range.rbegin() : *range.begin();

    // For focus use the next value after the maximum/minimum one.
    if (parameterName == kHanwhaFocusProperty)
    {
        if (speed > 0)
        {
            const auto nextPositiveValue = ++range.rbegin();
            return nextPositiveValue != range.rend() && *nextPositiveValue > 0
                ? *nextPositiveValue
                : *range.rbegin();
        }
        else
        {
            const auto nextNegativeValue = ++range.begin();
            return nextNegativeValue != range.end() && *nextNegativeValue < 0
                ? *nextNegativeValue
                : *range.begin();
        }
    }

    NX_ASSERT(false, lm("Invalid property: %1").arg(parameterName));
    return boost::none;
}

HanwhaAlternativePtzParameterContext& HanwhaPtzExecutor::context(const QString& parameterName)
{
    NX_ASSERT(m_parameterContexts.find(parameterName) != m_parameterContexts.cend());
    return m_parameterContexts[parameterName];
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
