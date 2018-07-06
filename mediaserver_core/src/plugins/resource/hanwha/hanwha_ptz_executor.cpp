#include "hanwha_ptz_executor.h"

#include <chrono>

#include <common/static_common_module.h>
#include <core/resource_management/resource_data_pool.h>

#include <nx/utils/thread/barrier_handler.h>
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

HanwhaPtzExecutor::HanwhaPtzExecutor(
    const HanwhaResourcePtr& hanwhaResource,
    const std::map<QString, std::set<int>>& ranges)
    :
    m_hanwhaResource(hanwhaResource)
{
    for (auto& item: ranges)
    {
        const auto& parameterName = item.first;
        const auto& range = item.second;

        m_parameterContexts[parameterName] = ParameterContext();
        auto& context = m_parameterContexts[parameterName];
        context.httpClient = makeHttpClient();
        context.timer = std::make_unique<nx::network::aio::Timer>(
            context.httpClient->getAioThread());

        context.range = range;
    }
}

HanwhaPtzExecutor::~HanwhaPtzExecutor()
{
    nx::utils::promise<void> promise;
    {
        nx::utils::BarrierHandler allIoStopped([&promise]() { promise.set_value(); });
        for (auto& item: m_parameterContexts)
        {
            auto& context = item.second;
            context.httpClient->pleaseStop(
                [this, &context, handler = allIoStopped.fork()]()
                {
                    context.timer->pleaseStopSync();
                    handler();
                });
        }
    }
    promise.get_future().wait();
}

void HanwhaPtzExecutor::setSpeed(const QString& parameterName, qreal speed)
{
    auto& parameterContext = context(parameterName);
    parameterContext.timer->post(
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
        sendValueToDevice(parameterName, speed);
}

void HanwhaPtzExecutor::scheduleNextRequest(const QString& parameterName)
{
    auto& timer = context(parameterName).timer;
    timer->start(kRequestInterval, [this, parameterName]() { startMovement(parameterName); });
}

void HanwhaPtzExecutor::sendValueToDevice(const QString& parameterName, int parameterValue)
{
    auto& timer = context(parameterName).timer;
    timer->cancelAsync(
        [this, parameterName, parameterValue]()
        {
            auto& httpClient = context(parameterName).httpClient;
            NX_ASSERT(httpClient, lm("Wrong parameter name: %1").arg(parameterName));

            const auto url = HanwhaRequestHelper::buildRequestUrl(
                m_hanwhaResource->sharedContext().get(),
                lit("image/focus/control"),
                {
                    { parameterName, QString::number(parameterValue) },
                    { kHanwhaChannelProperty, QString::number(m_hanwhaResource->getChannel())}
                });

            httpClient->doGet(
                url,
                [this, parameterName]() { scheduleNextRequest(parameterName); });
        });
}

std::unique_ptr<nx::network::http::AsyncClient> HanwhaPtzExecutor::makeHttpClient() const
{
    auto auth = m_hanwhaResource->getAuth();
    auto httpClient = std::make_unique<nx::network::http::AsyncClient>();
    httpClient->setUserName(auth.user());
    httpClient->setUserPassword(auth.password());
    httpClient->setSendTimeout(kHanwhaExecutorSendTimeout);
    httpClient->setMessageBodyReadTimeout(kHanwhaExecutorReceiveTimeout);

    return httpClient;
}

boost::optional<int> HanwhaPtzExecutor::toHanwhaSpeed(
    const QString& parameterName,
    qreal speed) const
{
    if (qFuzzyIsNull(speed))
        return 0;

    const auto& parameterRange = range(parameterName);
    if (parameterRange.empty())
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
        return speed > 0 ? *parameterRange.rbegin() : *parameterRange.begin();

    // For focus use the next value after the maximum/minimum one.
    if (parameterName == kHanwhaFocusProperty)
    {
        if (speed > 0)
        {
            const auto nextPositiveValue = ++parameterRange.rbegin();
            return nextPositiveValue != parameterRange.rend() && *nextPositiveValue > 0
                ? *nextPositiveValue
                : *parameterRange.rbegin();
        }
        else
        {
            const auto nextNegativeValue = ++parameterRange.begin();
            return nextNegativeValue != parameterRange.end() && *nextNegativeValue < 0
                ? *nextNegativeValue
                : *parameterRange.begin();
        }
    }

    NX_ASSERT(false, lm("Invalid property: %1").arg(parameterName));
    return boost::none;
}

HanwhaPtzExecutor::ParameterContext& HanwhaPtzExecutor::context(const QString& parameterName)
{
    NX_CRITICAL(m_parameterContexts.find(parameterName) != m_parameterContexts.cend());
    return m_parameterContexts[parameterName];
}

std::set<int> HanwhaPtzExecutor::range(const QString& parameterName) const
{
    NX_CRITICAL(m_parameterContexts.find(parameterName) != m_parameterContexts.cend());
    return m_parameterContexts.at(parameterName).range;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
