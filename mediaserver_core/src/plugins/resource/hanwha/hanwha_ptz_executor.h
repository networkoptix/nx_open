#pragma once

#include <plugins/resource/hanwha/hanwha_resource.h>
#include <plugins/resource/hanwha/hanwha_range.h>

#include <nx/network/http/http_async_client.h>
#include <nx/network/aio/timer.h>
#include <nx/utils/std/optional.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

enum class HanwhaConfigurationalPtzCommandType
{
    zoom,
    focus,
    ptr
};

struct HanwhaConfigurationalPtzCommand
{
    HanwhaConfigurationalPtzCommandType command;
    // For zoom and focus speed.x() is used for speed.
    nx::core::ptz::Vector speed;
};

class HanwhaPtzExecutor
{

public:
    HanwhaPtzExecutor(
        const HanwhaResourcePtr& hanwhaResource,
        const std::map<QString, HanwhaRange>& ranges);
    virtual ~HanwhaPtzExecutor();

    bool continuousMove(const nx::core::ptz::Vector& speed);
    bool continuousFocus(qreal speed);

private:
    bool executeCommand(const HanwhaConfigurationalPtzCommand& command);

    // For focus and zoom, since they're both controlled via the 'focus' submenu.
    bool executeFocusCommand(const HanwhaConfigurationalPtzCommand& command);

    // For PTR.
    bool executePtrCommand(const HanwhaConfigurationalPtzCommand& command);

    std::unique_ptr<nx_http::AsyncClient> makeHttpClient() const;
    std::optional<QString> toHanwhaParameterValue(const QString& parameterName, qreal speed) const;
    std::optional<QUrl> makePtrUrl(const HanwhaConfigurationalPtzCommand& command) const;

private:
    HanwhaResourcePtr m_hanwhaResource;
    std::map<QString, HanwhaRange> m_ranges;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
