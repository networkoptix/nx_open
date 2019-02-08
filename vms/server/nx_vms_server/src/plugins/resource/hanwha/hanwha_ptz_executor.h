#pragma once

#include <plugins/resource/hanwha/hanwha_resource.h>
#include <plugins/resource/hanwha/hanwha_range.h>

#include <nx/core/ptz/vector.h>

#include <nx/network/http/http_async_client.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/url.h>

namespace nx {
namespace vms::server {
namespace plugins {

enum class HanwhaConfigurationalPtzCommandType
{
    undefined,
    zoom,
    focus,
    ptr //< Pan, tilt, rotation.
};

struct HanwhaConfigurationalPtzCommand
{
    HanwhaConfigurationalPtzCommandType command = HanwhaConfigurationalPtzCommandType::undefined;
    nx::core::ptz::Vector speed;
};

class HanwhaPtzExecutor
{
public:
    using CommandDoneCallback = nx::utils::MoveOnlyFunc<
        void(HanwhaConfigurationalPtzCommandType, bool hasRequestSucceeded)>;

public:
    HanwhaPtzExecutor(
        const HanwhaResourcePtr& hanwhaResource,
        const std::map<QString, HanwhaRange>& ranges);

    HanwhaPtzExecutor(HanwhaPtzExecutor&&) = default;

    virtual ~HanwhaPtzExecutor();

    bool executeCommand(const HanwhaConfigurationalPtzCommand& command, int64_t sequenceId);
    void setCommandDoneCallback(CommandDoneCallback callback);
    void stop();

private:
    // For focus and zoom, since they're both controlled via the 'focus' submenu.
    bool executeFocusCommand(const HanwhaConfigurationalPtzCommand& command, int64_t sequenceId);

    // For PTR.
    bool executePtrCommand(const HanwhaConfigurationalPtzCommand& command, int64_t sequenceId);

    std::unique_ptr<nx::network::http::AsyncClient> makeHttpClientThreadUnsafe() const;
    std::optional<QString> toHanwhaParameterValue(const QString& parameterName, qreal speed) const;
    std::optional<nx::utils::Url> makePtrUrl(const HanwhaConfigurationalPtzCommand& command) const;
    std::optional<nx::utils::Url> makePtrUrl(
        const HanwhaConfigurationalPtzCommand& command,
        int64_t sequenceId) const;

private:
    mutable QnMutex m_mutex;
    HanwhaResourcePtr m_hanwhaResource;
    std::map<QString, HanwhaRange> m_ranges;
    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
    CommandDoneCallback m_callback;
    bool m_terminated = false;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
