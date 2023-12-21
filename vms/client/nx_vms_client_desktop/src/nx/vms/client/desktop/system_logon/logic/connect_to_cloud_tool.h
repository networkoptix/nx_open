// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/system_data.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <ui/workbench/workbench_state_manager.h>

class QWidget;

namespace nx::cloud::db::api {

enum class ResultCode;
class Connection;

} // namespace nx::cloud::db::api

namespace nx::vms::client::core { class CloudConnectionFactory; }
namespace nx::vms::common { class SystemSettings; }

namespace nx::vms::client::desktop {

class OauthLoginDialog;
class SessionRefreshDialog;

class ConnectToCloudTool:
    public QObject,
    public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = QObject;

public:
    ConnectToCloudTool(QWidget* parent, nx::vms::common::SystemSettings*);
    virtual ~ConnectToCloudTool() override;

    bool start();
    void cancel();

signals:
    void finished();

protected:
    virtual bool tryClose(bool force) override;

private:
    QWidget* getTopWidget() const;
    void showSuccess(const QString& cloudUser);
    void showFailure(const QString& message);

    void requestCloudAuthData();
    void onCloudAuthDataReady();
    bool processBindResult(nx::cloud::db::api::ResultCode result);
    void onBindFinished(
        nx::cloud::db::api::ResultCode result,
        nx::cloud::db::api::SystemData systemData);
    void onBindFinished(nx::cloud::db::api::SystemData systemData);
    void requestLocalSessionToken();
    void onLocalSessionTokenReady();
    void bindSystemToCloud(
        nx::network::http::Credentials credentials,
        const QString& organizationId = QString());

    void onBindToCloudDataReady();
    void onCloudTokensReady();

private:
    QWidget* m_parent;
    QPointer<OauthLoginDialog> m_oauthLoginDialog;
    QPointer<SessionRefreshDialog> m_localLoginDialog;

    std::unique_ptr<core::CloudConnectionFactory> m_cloudConnectionFactory;
    std::unique_ptr<nx::cloud::db::api::Connection> m_cloudConnection;
    nx::vms::client::core::CloudAuthData m_cloudAuthData;
    nx::cloud::db::api::SystemData m_systemData;
    const nx::vms::common::SystemSettings* const m_systemSettings;
};

} // namespace nx::vms::client::desktop
