// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <nx/vms/client/mobile/session/session_manager.h>

namespace nx::vms::client::mobile {

struct SystemContext::Private
{
    std::unique_ptr<SessionManager> sessionManager;
};

SystemContext::SystemContext(Mode mode, nx::Uuid peerId, QObject* parent):
    base_type(mode, peerId, parent),
    d{new Private{}}
{
    d->sessionManager = std::make_unique<SessionManager>(this);
}

SystemContext::~SystemContext()
{
}

SessionManager* SystemContext::sessionManager() const
{
    return d->sessionManager.get();
}

} // namespace nx::vms::client::mobile
