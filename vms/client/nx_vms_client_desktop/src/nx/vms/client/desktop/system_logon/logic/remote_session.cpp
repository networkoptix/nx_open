// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_session.h"

#include <chrono>

#include <QtCore/QTimer>

#include <client_core/client_core_module.h>
#include <network/system_helpers.h>
#include <nx/reflect/json.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_connection_error.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <utils/common/synctime.h>

using namespace nx::vms::client::core;

namespace nx::vms::client::desktop {

namespace {

constexpr auto kKeepUnauthorizedServerTimeout = std::chrono::seconds(15);

} // namespace

RemoteSession::RemoteSession(
    RemoteConnectionPtr connection,
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(connection, systemContext, parent)
{
    const auto username = QString::fromStdString(connection->credentials().username);
    const auto localSystemId = connection->moduleInformation().localSystemId.toString();
    m_sessionId = SessionId(username, localSystemId);

    auto sharedMemoryManager = appContext()->sharedMemoryManager();

    auto tokenUpdated =
        [this, sharedMemoryManager]
        {
            const auto token = this->connection()->credentials().authToken;
            const bool isCloud = this->connection()->userType() == nx::vms::api::UserType::cloud;
            if (token.isBearerToken() && !isCloud)
                sharedMemoryManager->updateSessionToken(token.value);
        };

    connect(this, &RemoteSession::credentialsChanged, this, tokenUpdated);
    connect(sharedMemoryManager, &SharedMemoryManager::sessionTokenChanged,
        this, &RemoteSession::updateBearerToken);

    auto keepServerTimer = new QTimer(this);
    keepServerTimer->setInterval(kKeepUnauthorizedServerTimeout);
    keepServerTimer->callOnTimeout([this] { m_keepUnauthorizedServer = false; });

    connect(this, &RemoteSession::stateChanged, this,
        [this, keepServerTimer](State state)
        {
            NX_DEBUG(this, "Remote session state changed: %1, keep unauthorized server", state);

            m_keepUnauthorizedServer = true;
            if (state == State::reconnecting)
                keepServerTimer->start();
            else
                keepServerTimer->stop();
        });

    sharedMemoryManager->enterSession(m_sessionId);
    tokenUpdated();
}

RemoteSession::~RemoteSession()
{
    const bool sessionIsStillActive = appContext()->sharedMemoryManager()->leaveSession();
    if (sessionIsStillActive)
    {
        NX_VERBOSE(this, "Current session is not the latest one, keep token alive");
        setAutoTerminate(false);
    }
}

SessionId RemoteSession::sessionId() const
{
    return m_sessionId;
}

void RemoteSession::autoTerminateIfNeeded()
{
    NX_VERBOSE(this, "User ended session manually");

    auto connection = this->connection();
    if (!NX_ASSERT(connection))
        return;

    const auto systemId = ::helpers::getLocalSystemId(connection->moduleInformation());
    const auto storedCredentials = CredentialsManager::credentials(
        systemId,
        connection->credentials().username);
    const bool hasStoredCredentials = storedCredentials
        && storedCredentials->authToken.isBearerToken()
        && !storedCredentials->authToken.value.empty();

    if (hasStoredCredentials)
    {
        NX_VERBOSE(this, "Keep token alive as it is stored locally");
    }
    else
    {
        setAutoTerminate(true);
    }
}

bool RemoteSession::keepCurrentServerOnError(RemoteConnectionErrorCode error)
{
    // Session token can be updated using shared memory.
    // Cloud acccess token may be reacquired from the refresh token.
    if (error == RemoteConnectionErrorCode::unauthorized
        || error == RemoteConnectionErrorCode::sessionExpired
        || error == RemoteConnectionErrorCode::cloudSessionExpired)
    {
        return m_keepUnauthorizedServer;
    }

    return base_type::keepCurrentServerOnError(error);
}


} // namespace nx::vms::client::desktop
