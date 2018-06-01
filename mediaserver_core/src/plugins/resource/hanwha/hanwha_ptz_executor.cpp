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
}

bool HanwhaPtzExecutor::continuousMove(const QVector3D& speed)
{
    HanwhaConfigurationalPtzCommand ptrCommand;
    ptrCommand.command = HanwhaConfigurationalPtzCommandType::focus;
    ptrCommand.speed = QVector4D(speed.x(), speed.y(), /*zoom*/ 0, /*rotate*/ 0);

    HanwhaConfigurationalPtzCommand zoomCommand;
    zoomCommand.command = HanwhaConfigurationalPtzCommandType::focus;
    zoomCommand.speed = QVector4D(speed.z(), 0, 0, 0);

    executeCommand(ptrCommand);
    executeCommand(zoomCommand);

    return true;
}

bool HanwhaPtzExecutor::continuousFocus(qreal speed)
{
    HanwhaConfigurationalPtzCommand command;
    command.command = HanwhaConfigurationalPtzCommandType::focus;
    command.speed = QVector4D(speed, 0, 0, 0);
    return executeFocusCommand(command);
}

bool HanwhaPtzExecutor::executeCommand(const HanwhaConfigurationalPtzCommand& command)
{
    switch (command.command)
    {
        case HanwhaConfigurationalPtzCommandType::focus:
        case HanwhaConfigurationalPtzCommandType::zoom:
            return executeFocusCommand(command);
        case HanwhaConfigurationalPtzCommandType::ptr:
            return executePtrCommand(command);
        default:
            NX_ASSERT(false, lit("Wrong command type. We should never be here."));
            return false;
    }
}

bool HanwhaPtzExecutor::executeFocusCommand(const HanwhaConfigurationalPtzCommand& command)
{
    const auto parameterName = commandToParameterName(command.command);
    if (parameterName.isEmpty())
        return false;

    const auto parameterValue = toHanwhaParameterValue(parameterName, command.speed.x());
    if (parameterValue == std::nullopt)
        return false;

    auto httpClient = makeHttpClient();
    const auto url = HanwhaRequestHelper::buildRequestUrl(
        m_hanwhaResource->sharedContext().get(),
        lit("image/focus/control"),
        {
            {parameterName, *parameterValue},
            // TODO: #dmishin think about bypass here.
            {kHanwhaChannelProperty, QString::number(m_hanwhaResource->getChannel())}
        });

    httpClient->doGet(
        url,
        [this, parameterName]() { /* Do something here.*/ });

    return true;
}

bool HanwhaPtzExecutor::executePtrCommand(const HanwhaConfigurationalPtzCommand& command)
{
    const auto httpClient = makeHttpClient();
    const auto url = makePtrUrl(command);

    if (url == std::nullopt)
        return false;

    httpClient->doGet(
        *url,
        []() {/* Do something here.*/});

    return true;
}

std::unique_ptr<nx_http::AsyncClient> HanwhaPtzExecutor::makeHttpClient() const
{
    auto auth = m_hanwhaResource->getAuth();
    auto httpClient = std::make_unique<nx_http::AsyncClient>();
    httpClient->setAuth({auth.user(), auth.password()});
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
        return QString();

    const auto& range = itr->second;
    return range.mapValue(speed);
}

std::optional<QUrl> HanwhaPtzExecutor::makePtrUrl(
    const HanwhaConfigurationalPtzCommand& command) const
{
    if (command.command == HanwhaConfigurationalPtzCommandType::ptr)
    {
        NX_ASSERT(false, lit("Wrong command. PTR command is expected"));
        return QUrl();
    }

    HanwhaRequestHelper::Parameters requestParameters = {
        {kHanwhaChannelProperty, QString::number(m_hanwhaResource->getChannel())}
    };

    const std::map<QString, qreal> parameters = {
        {kHanwhaConfigurationalPan, command.speed.x()},
        {kHanwhaConfigurationalTilt, command.speed.y()},
        {kHanwhaConfigurationalRotate, 0.0} //< TODO: #dmishin fix rotate.
    };

    for (const auto& parameter: parameters)
    {
        const auto& parameterName = parameter.first;
        const auto parameterValue = parameter.second;

        const auto deviceValue = toHanwhaParameterValue(parameterName, parameterValue);
        if (deviceValue == std::nullopt || deviceValue->isEmpty())
            return std::nullopt;

        requestParameters[parameterName] = *deviceValue;
    }

    return HanwhaRequestHelper::buildRequestUrl(
        m_hanwhaResource->sharedContext().get(),
        lit("image/ptr/control"),
        requestParameters);
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
