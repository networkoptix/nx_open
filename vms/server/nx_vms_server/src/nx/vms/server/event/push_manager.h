#pragma once

#include <set>
#include <queue>

#include <QtCore/QJsonObject>

#include <nx/network/retry_timer.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/actions/actions_fwd.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/utils/thread/mutex.h>

#include <translation/preloaded_translation_reference.h>

class QnVirtualCameraResource;

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
    public QObject,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT

public:
    static const nx::network::RetryPolicy kDefaultRetryPolicy;

    PushManager(
        QnMediaServerModule* serverModule,
        nx::network::RetryPolicy retryPolicy = kDefaultRetryPolicy,
        bool useEncryption = true,
        QObject* parent = 0);
    ~PushManager();

    bool send(const vms::event::AbstractActionPtr& action);

    // Force push notifications language update. Used by unit tests.
    void updateTargetLanguage();

private:
    PushPayload makePayload(
        const vms::event::EventParameters& event,
        const QnVirtualCameraResource* camera) const;
    PushNotification makeNotification(const vms::event::AbstractActionPtr& action) const;
    std::set<QString> cloudUsers(const vms::event::AbstractActionPtr& action) const;

private:
    class Pipeline;
    class Dispatcher;

private:
    const nx::network::RetryPolicy m_retryPolicy;
    std::unique_ptr<Pipeline> m_pipeline;
    std::list<std::unique_ptr<Dispatcher>> m_dispatchers;

    mutable nx::Mutex m_mutex;
    nx::vms::translation::PreloadedTranslationReference m_pushLanguage;
};

} // namespace nx::vms::server::event
