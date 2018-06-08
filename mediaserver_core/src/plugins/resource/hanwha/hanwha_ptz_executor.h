#pragma once

#include <plugins/resource/hanwha/hanwha_resource.h>
#include <plugins/resource/hanwha/hanwha_range.h>

#include <nx/network/http/http_async_client.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/url.h>

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
    QVector4D speed;
};

class HanwhaPtzExecutor
{

public:
    HanwhaPtzExecutor(
        const HanwhaResourcePtr& hanwhaResource,
        const std::map<QString, HanwhaRange>& ranges);
    virtual ~HanwhaPtzExecutor();

    bool continuousMove(const QVector3D& speed);
    bool continuousFocus(qreal speed);

private:
    bool executeCommand(const HanwhaConfigurationalPtzCommand& command);

    // For focus and zoom, since they're both controlled via the 'focus' submenu.
    bool executeFocusCommand(const HanwhaConfigurationalPtzCommand& command);

    // For PTR.
    bool executePtrCommand(const HanwhaConfigurationalPtzCommand& command);

    std::unique_ptr<nx::network::http::AsyncClient> makeHttpClient() const;
    std::optional<QString> toHanwhaParameterValue(const QString& parameterName, qreal speed) const;
    std::optional<nx::utils::Url> makePtrUrl(const HanwhaConfigurationalPtzCommand& command) const;

private:
    HanwhaResourcePtr m_hanwhaResource;
    std::map<QString, HanwhaRange> m_ranges;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
