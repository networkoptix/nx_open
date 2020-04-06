#pragma once

#include <set>
#include <QtCore/QJsonObject>

#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/actions/actions_fwd.h>
#include <nx/vms/server/server_module_aware.h>

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
    PushManager(QnMediaServerModule* serverModule, bool useEncryption = true);
    bool send(const vms::event::AbstractActionPtr& action);

private:
    PushPayload makePayload(const vms::event::EventParameters& event, bool isCamera) const;
    PushNotification makeNotification(const vms::event::AbstractActionPtr& action) const;
    std::set<QString> cloudUsers(const vms::event::ActionParameters& params) const;

private:
    const bool m_useEncryption = true;
};

} // namespace nx::vms::server::event
