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
static constexpr char kReusePort[] = "reusePort";
static constexpr char kListeningConcurrency[] = "listeningConcurrency";

static constexpr char kLegacySslEndpointsToListen[] = "sslEndpoints";
static constexpr char kSslCertificatePath[] = "certificatePath";
static constexpr char kSslCertificateMonitorTimeout[] = "certificateMonitorTimeout";
static constexpr char kSslAllowedSslVersions[] = "allowedSslVersions";

void Settings::load(const SettingsReader& settings0, const char * groupName)
{
    using namespace std::chrono;

    QnSettingsGroupReader settings(settings0, groupName);

    tcpBacklogSize = settings.value(kTcpBacklogSize, tcpBacklogSize).toInt();

    if (settings.contains(kConnectionInactivityPeriod))
    {
        if (auto val = nx::utils::parseDuration(
            settings.value(kConnectionInactivityPeriod).toString().toStdString()))
        {
            connectionInactivityPeriod = *val;
        }
    }

    loadEndpoints(settings, kEndpointsToListen, &endpoints);
    serverName = settings.value(kServerName).toString().toStdString();
    redirectHttpToHttps = settings.value(kRedirectHttpToHttps, redirectHttpToHttps).toBool();
    reusePort = settings.value(kReusePort, reusePort).toBool();
    listeningConcurrency = settings.value(kListeningConcurrency, listeningConcurrency).toInt();

    loadSsl(settings);
    loadHeaders(settings);
}

void Settings::loadEndpoints(
    const SettingsReader& settings,
    const char* paramFullName,
    std::vector<SocketAddress>* endpoints)
{
    const auto endpointsStr = settings.value(paramFullName).toString().toStdString();

    nx::utils::split(
        endpointsStr, ',',
        [&endpoints](const auto& token) { endpoints->push_back(SocketAddress(token)); });
}

void Settings::loadSsl(const SettingsReader& settings0)
{
    QnSettingsGroupReader settings(settings0, "ssl");

    loadEndpoints(settings, kEndpointsToListen, &ssl.endpoints);

    if (ssl.endpoints.empty())
    {
        // Trying to use legacy setting name.
        loadEndpoints(settings, kLegacySslEndpointsToListen, &ssl.endpoints);
    }

    ssl.certificatePath = settings.value(
        kSslCertificatePath,
        ssl.certificatePath.c_str()).toString().toStdString();

    if (settings.contains(kSslCertificateMonitorTimeout))
    {
        ssl.certificateMonitorTimeout = nx::utils::parseDuration(
            settings.value(kSslCertificateMonitorTimeout).toString().toStdString());
    }

    ssl.allowedSslVersions = settings.value(
        kSslAllowedSslVersions,
        ssl.allowedSslVersions.c_str()).toString().toStdString();
}

void Settings::loadHeaders(const SettingsReader& settings)
{
    static constexpr char kHeaders[] = "extraResponseHeaders";

    if (!settings.containsGroup(kHeaders))
        return;

    const auto args = QnSettingsGroupReader{settings, kHeaders}.allArgs();
    for (const auto& arg : args)
    {
        insertOrReplaceHeader(
            &extraResponseHeaders,
            HttpHeader{arg.first.toStdString(), arg.second.toStdString()});
    }
}

} // namespace nx::network::http::server
