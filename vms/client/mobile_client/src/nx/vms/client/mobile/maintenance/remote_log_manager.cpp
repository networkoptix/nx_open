// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_log_manager.h"

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtQml/QtQml>

#include <mobile_client/mobile_client_module.h>
#include <mobile_client/mobile_client_settings.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/mobile/push_notification/details/push_ipc_data.h>
#include <utils/common/synctime.h>

#include "remote_log_session_data.h"

namespace nx::vms::client::mobile {

namespace {

QString startLogSession(
    std::chrono::milliseconds duration,
    const std::string& forceSessionId = {})
{
    using namespace std::chrono;
    const auto uploader = qnMobileClientModule->remoteLogsUploader();
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
    qmlRegisterSingletonType<RemoteLogManager>("nx.vms.client.mobile", 1, 0, "LogManager",
        [](QQmlEngine* /*engine*/, QJSEngine* /*jsEngine*/)
        {
            return core::withCppOwnership(instance());
        });
}

RemoteLogManager* RemoteLogManager::instance()
{
    static RemoteLogManager instance;
    return &instance;
}

RemoteLogManager::RemoteLogManager(QObject* parent):
    base_type(parent)
{
    const auto lastSession = qnSettings->remoteLogSessionData();
    const qint64 remainingTimeMs = lastSession.endTimeMs - qnSyncTime->currentMSecsSinceEpoch();
    if (remainingTimeMs > 0)
        startLogSession(std::chrono::milliseconds(remainingTimeMs), lastSession.sessionId);
}

RemoteLogManager::~RemoteLogManager()
{
}

QString RemoteLogManager::startRemoteLogSession(int durationMinutes)
{
    const auto result = startLogSession(std::chrono::minutes(durationMinutes));
    if (!result.isEmpty())
        qApp->clipboard()->setText(result);
    return result;
}

QString RemoteLogManager::remoteLogSessionId() const
{
    return QString::fromStdString(qnMobileClientModule->remoteLogsUploader()->currentSessionId());
}

} // namespace nx::vms::client::mobile
