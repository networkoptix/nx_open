// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_session_token_updater.h"

#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>

#include <client_core/client_core_module.h>
#include <nx/cloud/db/api/connection.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/network/cloud_connection_factory.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::core {

using namespace std::chrono;

namespace {

static constexpr auto kTokenIsExpiringRatio = 0.1;
static constexpr auto kTokenUpdateInterval = 1min;
static constexpr auto kTokenUpdateThreshold = 10min;
static constexpr auto kMaxTokenUpdateRequestTime = 20s;

} // namespace

CloudSessionTokenUpdater::CloudSessionTokenUpdater(QObject* parent):
    QObject(parent),
    m_timer(new QTimer(this))
{
    m_timer->callOnTimeout([this]() { updateTokenIfNeeded(); });

    connect(qApp, &QGuiApplication::applicationStateChanged, this,
        [this](Qt::ApplicationState state)
        {
            switch (state)
            {
                case Qt::ApplicationSuspended:
                    NX_DEBUG(this, "Application is suspended. "
                        "Will try to refresh token on awakeness");
                    m_wasSuspended = true;
                    break;
                case Qt::ApplicationActive:
                    if (m_wasSuspended)
                    {
                        NX_DEBUG(this, "Application is active, forcing token update try");
                        updateTokenIfNeeded();
                        m_wasSuspended = false;
                    }
                    break;
                default:
                    break;
            }
        });
}

CloudSessionTokenUpdater::~CloudSessionTokenUpdater()
{
}

void CloudSessionTokenUpdater::updateTokenIfNeeded()
{
    const bool expired = m_expirationTimer.hasExpired();
    const bool requestInProgress = !m_requestInProgressTimer.hasExpired();
    if (!expired || requestInProgress)
    {
        NX_DEBUG(this, "Do not update token, expiring/expired: %1, request in progress: %2",
           expired, requestInProgress);
        return;
    }

    NX_DEBUG(this, "Access token expiring or expired, requesting for the update");
    emit sessionTokenExpiring();
}

void CloudSessionTokenUpdater::onTokenUpdated(microseconds expirationTime)
{
    // Actual token lifetime.
    const auto tokenDuration = expirationTime - qnSyncTime->currentTimePoint();

    // Interval before the token expiration point when the token is considered expiring.
    const auto updateInterval = std::clamp(
        duration_cast<microseconds>(kTokenIsExpiringRatio * tokenDuration),
        duration_cast<microseconds>(kMaxTokenUpdateRequestTime),
        duration_cast<microseconds>(kTokenUpdateThreshold));

    // Time left before the token is considered expiring.
    const auto timeToUpdate = tokenDuration - updateInterval;

    // Set expiration timer.
    m_expirationTimer.setRemainingTime(timeToUpdate > microseconds::zero()
        ? timeToUpdate
        : microseconds::max());

    // Reset timed "request in progress" flag.
    m_requestInProgressTimer.setRemainingTime(microseconds::zero());

    NX_DEBUG(this, "Access token updated, expires at: %1, expiring/expired: %2",
        expirationTime, m_expirationTimer.hasExpired());

    // Access token lifetime can't exceed the lifespan of the refresh token. Therefore, we can
    // receive a new token which already expires. In that case it's wortheless to update it again.
    if (tokenDuration > kMaxTokenUpdateRequestTime)
    {
        // Adjust timer interval. For tokens with a very short lifespan, the token will be checked
        // and updated right after it's marked as expiring. For others, the timer interval will be
        // linearly increased depending on the token's lifespan until it reaches the value of
        // kTokenUpdateInterval. As a future improvement, we could think about enforcing several
        // update attempts for short-living tokens by using a decreased update interval for them.
        const auto interval = std::min(
            duration_cast<milliseconds>(timeToUpdate + kTokenUpdateInterval
                * (kTokenIsExpiringRatio * tokenDuration / kTokenUpdateThreshold)),
            duration_cast<milliseconds>(kTokenUpdateInterval));

        m_timer->start(interval);
    }
    else
    {
        m_timer->stop();
    }
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

    // Initialize timed "request in progress" flag.
    m_requestInProgressTimer.setRemainingTime(kMaxTokenUpdateRequestTime);

    m_cloudConnection->oauthManager()->issueToken(request, executor.bind(handler));
}

} // namespace nx::vms::client::core
