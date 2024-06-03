// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ldap_status_watcher.h"

#include <api/server_rest_connection.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr auto kLdapStatusRefreshInterval = std::chrono::minutes(1);

} // namespace

LdapStatusWatcher::LdapStatusWatcher(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext)
{
    const auto enableTimerWhenLdapIsConfigured =
        [this]()
        {
            const auto ldap = this->systemContext()->globalSettings()->ldap();
            const bool hasConfig = !ldap.uri.isEmpty() || !ldap.adminDn.isEmpty()
                || !ldap.filters.empty();

            if (hasConfig)
                m_refreshTimer.start();
            else
                m_refreshTimer.stop();
        };

    m_refreshTimer.setInterval(kLdapStatusRefreshInterval);
    connect(&m_refreshTimer, &QTimer::timeout, this, &LdapStatusWatcher::refresh);

    connect(systemContext->globalSettings(), &common::SystemSettings::ldapSettingsChanged, this,
        enableTimerWhenLdapIsConfigured);

    enableTimerWhenLdapIsConfigured();
}

LdapStatusWatcher::~LdapStatusWatcher()
{
    if (auto api = connectedServerApi(); api && m_refreshHandle > 0)
        connectedServerApi()->cancelRequest(m_refreshHandle);
}

std::optional<api::LdapStatus> LdapStatusWatcher::status() const
{
    return m_status;
}

bool LdapStatusWatcher::isOnline() const
{
    return m_status && m_status->state == api::LdapStatus::State::online;
}

void LdapStatusWatcher::refresh()
{
    NX_DEBUG(this, "Status refresh was requested.");

    if (m_refreshHandle > 0)
    {
        NX_VERBOSE(this, "Refresh is in progress. Skipping the request.");
        emit refreshFinished();
        return;
    }

    const auto statusCallback = nx::utils::guarded(this,
        [this](
            bool /*success*/, int handle, rest::ErrorOrData<api::LdapStatus> errorOrData)
        {
            if (handle != m_refreshHandle)
            {
                NX_VERBOSE(this, "Ignoring reply with an unexpected request ID");
                return;
            }

            m_refreshHandle = 0;

            if (const auto status = std::get_if<api::LdapStatus>(&errorOrData))
            {
                NX_DEBUG(this, "Successfully received LDAP status. State: %1", status->state);
                NX_VERBOSE(this, "LDAP status: %1", nx::reflect::json::serialize(*status));
                setStatus(*status);
            }
            else if (const auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
            {
                NX_DEBUG(this, "Request error: %1 (%2).", error->error, error->errorString);
                setStatus({});
            }

            emit refreshFinished();
        });

    if (auto api = connectedServerApi())
        m_refreshHandle = connectedServerApi()->getLdapStatusAsync(statusCallback, thread());
}

void LdapStatusWatcher::setStatus(const api::LdapStatus& status)
{
    if (m_status && *m_status == status)
        return;

    m_status = status;
    NX_DEBUG(this, "LDAP status was updated.");
    emit statusChanged();
}

} // namespace nx::vms::client::desktop
