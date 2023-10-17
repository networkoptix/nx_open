// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/desktop/state/session_id.h>

namespace nx::vms::client::desktop {

class SystemContext;

/**
 * Desktop remote session takes into account the ability to run multiple client instances within
 * the same session.
 */
class RemoteSession: public core::RemoteSession
{
    Q_OBJECT
    using base_type = core::RemoteSession;

public:
    RemoteSession(
        core::RemoteConnectionPtr connection,
        SystemContext* systemContext,
        QObject* parent = nullptr);
    virtual ~RemoteSession() override;

    SessionId sessionId() const;

    /**
     * Auto-terminate current session on closing if user did not save the token locally.
     */
    void autoTerminateIfNeeded();

protected:
    virtual bool keepCurrentServerOnError(core::RemoteConnectionErrorCode error) override;

private:
    SessionId m_sessionId;
    bool m_keepUnauthorizedServer = true;
};

} // namespace nx::vms::client::desktop
