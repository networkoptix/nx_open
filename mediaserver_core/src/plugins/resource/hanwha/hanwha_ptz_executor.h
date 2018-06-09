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

    std::unique_ptr<nx_http::AsyncClient> makeHttpClientThreadUnsafe() const;
    std::optional<QString> toHanwhaParameterValue(const QString& parameterName, qreal speed) const;
    std::optional<QUrl> makePtrUrl(
        const HanwhaConfigurationalPtzCommand& command,
        int64_t sequenceId) const;

private:
    mutable QnMutex m_mutex;
    HanwhaResourcePtr m_hanwhaResource;
    std::map<QString, HanwhaRange> m_ranges;
    std::unique_ptr<nx_http::AsyncClient> m_httpClient;
    CommandDoneCallback m_callback;
    bool m_terminated = false;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
