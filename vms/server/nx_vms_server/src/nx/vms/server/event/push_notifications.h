#pragma once

#include <set>

#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/actions/actions_fwd.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::event {

struct PushPayload;
struct PushNotification;
struct PushRequest;

class PushManager:
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    PushManager(QnMediaServerModule* serverModule);
    bool send(const vms::event::AbstractActionPtr& action);

private:
    PushPayload makePayload(const vms::event::EventParameters& event, bool isCamera) const;
    PushNotification makeNotification(const vms::event::AbstractActionPtr& action) const;
    std::set<QString> cloudUsers(std::vector<QnUuid> filter) const;
};

} // namespace nx::vms::server::event
