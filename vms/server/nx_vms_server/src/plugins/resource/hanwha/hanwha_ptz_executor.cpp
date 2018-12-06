#include "hanwha_ptz_executor.h"

#include <chrono>

#include <core/resource_management/resource_data_pool.h>

#include <nx/utils/thread/barrier_handler.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace vms::server {
namespace plugins {

namespace {

static const std::chrono::milliseconds kHanwhaExecutorSendTimeout(5000);
static const std::chrono::milliseconds kHanwhaExecutorReceiveTimeout(5000);
static const std::chrono::milliseconds kRequestInterval(500);

static const QString kHanwhaConfigurationalPan = lit("Pan");
static const QString kHanwhaConfigurationalTilt = lit("Tilt");
static const QString kHanwhaConfigurationalRotate = lit("Rotate");
static const QString kHanwhaConfigurationalZoom = lit("Zoom");
static const QString kHanwhaConfigurationalFocus = lit("Focus");

QString commandToParameterName(HanwhaConfigurationalPtzCommandType command)
{
    switch (command)
    {
        case HanwhaConfigurationalPtzCommandType::focus:
            return lit("Focus");
        case HanwhaConfigurationalPtzCommandType::zoom:
            return lit("Zoom");
        default:
            NX_ASSERT(false, lit("Wrong command type. We should never be here."));
            return QString();
    }
}

} // namespace

HanwhaPtzExecutor::HanwhaPtzExecutor(
    const HanwhaResourcePtr& hanwhaResource,
    const std::map<QString, HanwhaRange>& ranges)
    :
    m_hanwhaResource(hanwhaResource),
    m_ranges(ranges)
{
}

HanwhaPtzExecutor::~HanwhaPtzExecutor()
{
    stop();
}

bool HanwhaPtzExecutor::executeCommand(
    const HanwhaConfigurationalPtzCommand& command,
    int64_t sequenceId)
{
    switch (command.command)
    {
        case HanwhaConfigurationalPtzCommandType::focus:
        case HanwhaConfigurationalPtzCommandType::zoom:
            return executeFocusCommand(command, sequenceId);
        case HanwhaConfigurationalPtzCommandType::ptr:
            return executePtrCommand(command, sequenceId);
        default:
            NX_ASSERT(false, lit("Wrong command type. We should never be here."));
            return false;
    }
}

void HanwhaPtzExecutor::setCommandDoneCallback(CommandDoneCallback callback)
{
    m_callback = std::move(callback);
}

void HanwhaPtzExecutor::stop()
{
    decltype(m_httpClient) httpClient;
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
        m_httpClient.swap(httpClient);
    }

    if (httpClient)
        httpClient->pleaseStopSync();
}

bool HanwhaPtzExecutor::executeFocusCommand(
    const HanwhaConfigurationalPtzCommand& command,
    int64_t sequenceId)
{
    const auto parameterName = commandToParameterName(command.command);
    if (parameterName.isEmpty())
        return false;

    const auto parameterValue = toHanwhaParameterValue(
        lm("Continuous.%1").args(parameterName),
        parameterName == kHanwhaZoomProperty ? command.speed.zoom : command.speed.focus);

    if (parameterValue == std::nullopt)
        return false;

    const auto url = HanwhaRequestHelper::buildRequestUrl(
        m_hanwhaResource->sharedContext().get(),
        lit("image/focus/control"),
        {
            // TODO: #dmishin think about bypass here.
            {parameterName, *parameterValue},
            {kHanwhaChannelProperty, QString::number(m_hanwhaResource->getChannel())},
            {kHanwhaSequenceIdProperty, QString::number(sequenceId)}
        },
        m_hanwhaResource->bypassChannel());

    {
        QnMutexLocker lock(&m_mutex);
        m_httpClient = makeHttpClientThreadUnsafe();
        if (!m_httpClient)
            return false;

        m_httpClient->doGet(
            url,
            [this, commandType = command.command]()
            {
                if (m_callback)
                    m_callback(commandType, m_httpClient->hasRequestSucceeded());
            });
    }

    return true;
}

bool HanwhaPtzExecutor::executePtrCommand(
    const HanwhaConfigurationalPtzCommand& command,
    int64_t sequenceId)
{
    const auto url = makePtrUrl(command, sequenceId);
    if (url == std::nullopt)
        return false;

    {
        QnMutexLocker lock(&m_mutex);
        m_httpClient = makeHttpClientThreadUnsafe();
        if (!m_httpClient)
            return false;

        m_httpClient->doGet(
            *url,
            [this, commandType = command.command]()
            {
                if (m_callback)
                    m_callback(commandType, m_httpClient->hasRequestSucceeded());
            });
    }

    return true;
}

std::unique_ptr<nx::network::http::AsyncClient> HanwhaPtzExecutor::makeHttpClientThreadUnsafe() const
{
    if (m_terminated)
        return nullptr;
    auto auth = m_hanwhaResource->getAuth();
    auto httpClient = std::make_unique<nx::network::http::AsyncClient>();
    httpClient->setUserName(auth.user());
    httpClient->setUserPassword(auth.password());
    httpClient->setSendTimeout(kHanwhaExecutorSendTimeout);
    httpClient->setMessageBodyReadTimeout(kHanwhaExecutorReceiveTimeout);

    return httpClient;
}

std::optional<QString> HanwhaPtzExecutor::toHanwhaParameterValue(
    const QString& parameterName,
    qreal speed) const
{
    const auto itr = m_ranges.find(parameterName);
    if (itr == m_ranges.cend())
        return std::nullopt;

    const auto& range = itr->second;
    return range.mapValue(speed);
}

std::optional<nx::utils::Url> HanwhaPtzExecutor::makePtrUrl(
    const HanwhaConfigurationalPtzCommand& command,
    int64_t sequenceId) const
{
    if (command.command != HanwhaConfigurationalPtzCommandType::ptr)
    {
        NX_ASSERT(false, lit("Wrong command. PTR command is expected"));
        return std::nullopt;
    }

    HanwhaRequestHelper::Parameters requestParameters = {
        {kHanwhaChannelProperty, QString::number(m_hanwhaResource->getChannel())},
        {kHanwhaSequenceIdProperty, QString::number(sequenceId)}
    };

    const std::map<QString, qreal> parameters = {
        {kHanwhaConfigurationalPan, command.speed.pan},
        {kHanwhaConfigurationalTilt, command.speed.tilt},
        {kHanwhaConfigurationalRotate, command.speed.rotation}
    };

    for (const auto& parameter: parameters)
    {
        const auto& parameterName = parameter.first;
        const auto parameterValue = parameter.second;

        const auto deviceValue = toHanwhaParameterValue(
            lm("Continuous.%1").args(parameterName),
            parameterValue);

        if (deviceValue == std::nullopt || deviceValue->isEmpty())
            return std::nullopt;

        requestParameters[parameterName] = *deviceValue;
    }

    return HanwhaRequestHelper::buildRequestUrl(
        m_hanwhaResource->sharedContext().get(),
        lit("image/ptr/control"),
        requestParameters,
        m_hanwhaResource->bypassChannel());
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
