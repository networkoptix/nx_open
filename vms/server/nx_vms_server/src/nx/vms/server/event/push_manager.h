#pragma once

#include <set>
#include <queue>
#include <QtCore/QJsonObject>

#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/actions/actions_fwd.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/network/retry_timer.h>

namespace nx::vms::server::event {

struct PushPayload
{
    QString url;
    QString imageUrl;
};
#define PushPayload_Fields (url)(imageUrl)

struct PushNotification
{
    QString title;
    QString body;
    PushPayload payload;
    QJsonObject options;
};
#define PushNotification_Fields (title)(body)(payload)(options)

struct PushRequest
{
    QString systemId;
    std::set<QString> targets;
    PushNotification notification;
};
#define PushRequest_Fields (systemId)(targets)(notification)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (PushPayload)(PushNotification)(PushRequest),
    (json))

// -----------------------------------------------------------------------------------------------

class PushManager:
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    static const nx::network::RetryPolicy kDefaultRetryPolicy;

    PushManager(
        QnMediaServerModule* serverModule,
        nx::network::RetryPolicy retryPolicy = kDefaultRetryPolicy,
        bool useEncryption = true);
    ~PushManager();

    bool send(const vms::event::AbstractActionPtr& action);

private:
    PushPayload makePayload(const vms::event::EventParameters& event, bool isCamera) const;
    PushNotification makeNotification(const vms::event::AbstractActionPtr& action) const;
    std::set<QString> cloudUsers(const vms::event::AbstractActionPtr& action) const;

private:
    class Pipeline;
    class Dispatcher;

private:
    const nx::network::RetryPolicy m_retryPolicy;
    std::unique_ptr<Pipeline> m_pipeline;
    std::list<std::unique_ptr<Dispatcher>> m_dispatchers;
};

} // namespace nx::vms::server::event
