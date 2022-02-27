// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "settings.h"

#include <algorithm>

#include <nx/utils/deprecated_settings.h>
#include <nx/utils/string.h>
#include <nx/utils/time.h>

namespace nx::network::http::server {

static constexpr char kTcpBacklogSize[] = "tcpBacklogSize";
static constexpr char kConnectionInactivityPeriod[] = "connectionInactivityPeriod";
static constexpr char kEndpointsToListen[] = "endpoints";
static constexpr char kServerName[] = "serverName";
static constexpr char kRedirectHttpToHttps[] = "redirectHttpToHttps";

static constexpr char kLegacySslEndpointsToListen[] = "sslEndpoints";
static constexpr char kSslCertificatePath[] = "certificatePath";
static constexpr char kSslCertificateMonitorTimeout[] = "certificateMonitorTimeout";
static constexpr char kSslAllowedSslVersions[] = "allowedSslVersions";

Settings::Settings(const char* groupName):
    m_groupName(groupName)
{
}

void Settings::load(const QnSettings& settings)
{
    using namespace std::chrono;

    tcpBacklogSize = settings.value(
        nx::format("%1/%2").args(m_groupName, kTcpBacklogSize),
        tcpBacklogSize).toInt();

    if (auto name = nx::format("%1/%2").args(m_groupName, kConnectionInactivityPeriod);
        settings.contains(name))
    {
        if (auto val = nx::utils::parseDuration(settings.value(name).toString().toStdString()))
            connectionInactivityPeriod = *val;
    }

    loadEndpoints(
        settings,
        nx::format("%1/%2").args(m_groupName, kEndpointsToListen).toStdString(),
        &endpoints);

    serverName = settings.value(
        nx::format("%1/%2").args(m_groupName, kServerName)).toString().toStdString();

    redirectHttpToHttps = settings.value(
        nx::format("%1/%2").args(m_groupName, kRedirectHttpToHttps),
        redirectHttpToHttps).toBool();

    loadSsl(settings);
}

void Settings::loadEndpoints(
    const QnSettings& settings,
    const std::string_view& paramFullName,
    std::vector<SocketAddress>* endpoints)
{
    const auto endpointsStr = settings.value(paramFullName).toString().toStdString();

    nx::utils::split(
        endpointsStr, ',',
        [&endpoints](const auto& token) { endpoints->push_back(SocketAddress(token)); });
}

void Settings::loadSsl(const QnSettings& settings)
{
    const auto groupName = m_groupName + "/ssl";

    loadEndpoints(
        settings,
        nx::format("%1/%2").args(groupName, kEndpointsToListen).toStdString(),
        &ssl.endpoints);

    if (ssl.endpoints.empty())
    {
        // Trying to use legacy setting name.
        loadEndpoints(
            settings,
            nx::format("%1/%2").args(m_groupName, kLegacySslEndpointsToListen).toStdString(),
            &ssl.endpoints);
    }

    ssl.certificatePath = settings.value(
        nx::format("%1/%2").args(groupName, kSslCertificatePath),
        ssl.certificatePath.c_str()).toString().toStdString();

    if (auto name = nx::format("%1/%2").args(groupName, kSslCertificateMonitorTimeout);
        settings.contains(name))
    {
        ssl.certificateMonitorTimeout = nx::utils::parseDuration(
            settings.value(name).toString().toStdString());
    }

    ssl.allowedSslVersions = settings.value(
        nx::format("%1/%2").args(groupName, kSslAllowedSslVersions),
        ssl.allowedSslVersions.c_str()).toString().toStdString();
}

} // namespace nx::network::http::server
