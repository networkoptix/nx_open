// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_session.h"

#include <chrono>

#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <client/client_message_processor.h>
#include <client/client_module.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <nx/reflect/json.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_connection_error.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <utils/common/synctime.h>

using namespace nx::vms::client::core;

namespace nx::vms::client::desktop {

namespace {

constexpr auto kKeepUnauthorizedServerTimeout = std::chrono::seconds(15);

} // namespace

RemoteSession::RemoteSession(RemoteConnectionPtr connection, QObject* parent)
    :
    base_type(connection, parent)
{
    const auto username = QString::fromStdString(connection->credentials().username);
    const auto localSystemId = connection->moduleInformation().localSystemId.toString();
    m_sessionId = SessionId(username, localSystemId);

    auto sharedMemoryManager = qnClientModule->sharedMemoryManager();

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
    const bool sessionIsStillActive = qnClientModule->sharedMemoryManager()->leaveSession();
    if (sessionIsStillActive)
        setAutoTerminate(false);
}

SessionId RemoteSession::sessionId() const
{
    return m_sessionId;
}

bool RemoteSession::keepCurrentServerOnError(RemoteConnectionErrorCode error)
{
    // Session token can be updated using shared memory.
    if (error == RemoteConnectionErrorCode::unauthorized
        || error == RemoteConnectionErrorCode::sessionExpired)
    {
        return m_keepUnauthorizedServer;
    }

    return base_type::keepCurrentServerOnError(error);
}


} // namespace nx::vms::client::desktop
