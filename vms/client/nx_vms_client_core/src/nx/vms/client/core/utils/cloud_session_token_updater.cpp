// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_session_token_updater.h"

#include <QtCore/QTimer>

#include <client_core/client_core_module.h>
#include <nx/cloud/db/api/connection.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/network/cloud_connection_factory.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <utils/common/synctime.h>
#include <watchers/cloud_status_watcher.h>

namespace nx::vms::client::core {

using namespace std::chrono;

namespace {

static constexpr auto kTokenUpdateInterval = 1min;
static constexpr auto kTokenUpdateThreshold = 10min;

bool isTokenExpiring(microseconds expirationTime)
{
    const auto timeLeft = expirationTime - qnSyncTime->currentTimePoint();
    return timeLeft > microseconds::zero() && timeLeft <= kTokenUpdateThreshold;
}

} // namespace

CloudSessionTokenUpdater::CloudSessionTokenUpdater(QObject* parent):
    QObject(parent),
    m_timer(new QTimer(this))
{
    m_timer->setInterval(kTokenUpdateInterval);
    m_timer->callOnTimeout([this]() { onTimer(); });
}

CloudSessionTokenUpdater::~CloudSessionTokenUpdater()
{
}

void CloudSessionTokenUpdater::onTimer()
{
    if (isTokenExpiring(m_expirationTime))
    {
        NX_DEBUG(this, "Access token expiring, request for update");
        emit sessionTokenExpiring();
    }
}

void CloudSessionTokenUpdater::onTokenUpdated(microseconds expirationTime)
{
    m_expirationTime = expirationTime;
    const bool isExpiring = isTokenExpiring(m_expirationTime);
    NX_DEBUG(
        this,
        "Access token updated, expires at: %1, expiring: %2",
        expirationTime,
        isExpiring);

    // We need to stop trying to update already expiring token.
    if (isExpiring || (expirationTime <= microseconds::zero()))
        m_timer->stop();
    else
        m_timer->start();
}

void CloudSessionTokenUpdater::issueToken(
    const nx::cloud::db::api::IssueTokenRequest& request,
    IssueTokenHandler handler,
    nx::utils::AsyncHandlerExecutor executor)
{
    if (!m_cloudConnection)
    {
        m_cloudConnectionFactory = std::make_unique<CloudConnectionFactory>();
        m_cloudConnection = m_cloudConnectionFactory->createConnection();
    }

    m_cloudConnection->oauthManager()->issueToken(request, executor.bind(handler));
}

} // namespace nx::vms::client::core
