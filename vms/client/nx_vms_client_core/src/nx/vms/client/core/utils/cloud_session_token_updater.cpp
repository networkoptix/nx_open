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

using namespace std::chrono;

namespace nx::vms::client::core {

namespace {

static constexpr auto kTokenUpdateInterval = 1min;
static constexpr auto kTokenUpdateThreshold = 10min;
static constexpr auto kMaxTokenUpdateRequestTime = 20s;

} // namespace

CloudSessionTokenUpdater::CloudSessionTokenUpdater(QObject* parent):
    QObject(parent),
    m_timer(new QTimer(this))
{
    m_timer->setInterval(kTokenUpdateInterval);
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

void CloudSessionTokenUpdater::updateTokenIfNeeded(bool force)
{
    const bool expired = force || m_expirationTimer.hasExpired();
    const bool requestInProgress = !m_requestInProgressTimer.hasExpired();
    if (!expired || requestInProgress)
    {
        NX_DEBUG(
            this,
            "Do not update token, expired: %1, remaining time: %2, request in progress: %3",
            expired,
            m_expirationTimer.remainingTime(),
            requestInProgress);
        return;
    }

    NX_DEBUG(this, "Access token expiring or expired, requesting for the update");
    emit sessionTokenExpiring();
}

void CloudSessionTokenUpdater::onTokenUpdated(microseconds expirationTime)
{
    const auto duration = duration_cast<milliseconds>(
        expirationTime - kTokenUpdateThreshold - qnSyncTime->currentTimePoint());
    m_expirationTimer.setRemainingTime(duration > milliseconds::zero()
        ? duration
        : milliseconds::max());

    // Reset timed "request in progress" flag.
    m_requestInProgressTimer.setRemainingTime(milliseconds::zero());

    NX_DEBUG(this, "Access token updated, expires at: %1, expiring/expired: %2",
        expirationTime, m_expirationTimer.hasExpired());

    // Do not update already expiring token.
    if (expirationTime - qnSyncTime->currentTimePoint() > kTokenUpdateThreshold)
        m_timer->start();
    else
        m_timer->stop();
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
