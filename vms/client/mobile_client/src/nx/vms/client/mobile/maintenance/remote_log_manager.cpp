// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_log_manager.h"

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtQml/QtQml>

#include <mobile_client/mobile_client_settings.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/http_types.h>
#include <nx/network/maintenance/log/collector_api_paths.h>
#include <nx/network/maintenance/log/uploader.h>
#include <nx/network/url/url_builder.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/mobile/push_notification/details/push_ipc_data.h>
#include <utils/common/synctime.h>

#include "remote_log_session_data.h"

namespace nx::vms::client::mobile {

using network::maintenance::log::UploaderManager;

namespace {

QString startLogSession(
    const std::unique_ptr<UploaderManager>& uploader,
    std::chrono::milliseconds duration,
    const std::string& forceSessionId = {})
{
    using namespace std::chrono;
    const auto sessionId = uploader->start(duration, forceSessionId);
    if (sessionId.empty())
        return {};

    const qint64 sessionEndTimeMs = qnSyncTime->currentMSecsSinceEpoch() + duration.count();
    qnSettings->setRemoteLogSessionData({sessionEndTimeMs, sessionId});
    details::PushIpcData::setCloudLoggerOptions(sessionId + "_pn", milliseconds(sessionEndTimeMs));

    return QString::fromStdString(sessionId);
}

} // namespace

void RemoteLogManager::registerQmlType()
{
    qmlRegisterType<RemoteLogManager>("nx.vms.client.mobile", 1, 0, "LogManager");
}

RemoteLogManager::RemoteLogManager(QObject* parent):
    base_type(parent),
    m_uploader(
        []()
        {
            nx::log::LevelSettings logUploadFilter;
            logUploadFilter.primary = nx::log::Level::verbose;
            logUploadFilter.filters.emplace(std::string("nx"), nx::log::Level::verbose);
            logUploadFilter.filters.emplace(std::string("Qn"), nx::log::Level::verbose);

            const auto url = nx::network::url::Builder()
                .setScheme(nx::network::http::kUrlSchemeName)
                .setHost(nx::network::SocketGlobals::cloud().cloudHost())
                .setPath(nx::network::maintenance::log::kLogCollectorPathPrefix);
            return std::make_unique<UploaderManager>(url, logUploadFilter);

        }())
{
    const auto lastSession = qnSettings->remoteLogSessionData();
    const qint64 remainingTimeMs = lastSession.endTimeMs - qnSyncTime->currentMSecsSinceEpoch();
    if (remainingTimeMs <= 0)
        return;

    startLogSession(
        m_uploader,
        std::chrono::milliseconds(remainingTimeMs),
        lastSession.sessionId);
}

RemoteLogManager::~RemoteLogManager()
{
}

QString RemoteLogManager::startRemoteLogSession(int durationMinutes)
{
    const auto result = startLogSession(m_uploader, std::chrono::minutes(durationMinutes));
    if (!result.isEmpty())
        qApp->clipboard()->setText(result);
    return result;
}

QString RemoteLogManager::remoteLogSessionId() const
{
    return QString::fromStdString(m_uploader->currentSessionId());
}

} // namespace nx::vms::client::mobile
