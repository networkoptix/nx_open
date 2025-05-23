// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "api_integration_requests_model.h"

#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/analytics/integration_request.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>

namespace nx::vms::client::desktop {

ApiIntegrationRequestsModel::ApiIntegrationRequestsModel(
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext)
{
    auto timer = new QTimer(this);
    timer->setInterval(kRefreshInterval);
    timer->callOnTimeout(this, &ApiIntegrationRequestsModel::refresh);
    connect(this, &ApiIntegrationRequestsModel::isActiveChanged, this,
        [this, timer]
        {
            refresh();
            if (m_isActive)
                timer->start();
            else
                timer->stop();
        });
}

void ApiIntegrationRequestsModel::setRequests(const QVariant& requests)
{
    if (m_requests != requests)
    {
        m_requests = requests;
        emit requestsChanged();
    }
}

void ApiIntegrationRequestsModel::refresh()
{
    if (!connection()) //< It may be null if the client just disconnected from the server.
        return;

    connectedServerApi()->getRawResult(
        "/rest/v4/analytics/integrations/*/requests",
        nx::network::rest::Params(),
        nx::utils::guarded(this,
            [this](bool success,
                rest::Handle,
                const QByteArray& data,
                const nx::network::http::HttpHeaders&)
            {
                if (!success)
                    return;

                auto [requests, ok] = nx::reflect::json::deserialize<
                    std::vector<api::analytics::IntegrationRequestData>>(data.data());

                if (!ok)
                    return;

                QJsonArray result;
                for (const api::analytics::IntegrationRequestData& request: requests)
                {
                    result.push_back(QJsonObject{
                        // requestId is guaranteed to be non-null.
                        {"requestId", request.requestId.toString(QUuid::WithBraces)},
                        {"pinCode", request.pinCode},
                        {"name", request.integrationManifest.name},
                        {"description", request.integrationManifest.description},
                        {"vendor", request.integrationManifest.vendor},
                        {"version", request.integrationManifest.version},
                        {"permission", nx::vms::common::PredefinedUserGroups::find(
                            nx::vms::api::kPowerUsersGroupId).value_or(api::UserGroupData{}).name}
                    });
                }

                setRequests(result);
            }),
        thread());
}

void ApiIntegrationRequestsModel::reject(const QString& id)
{
    if (!connection()) //< It may be null if the client just disconnected from the server.
        return;

    connectedServerApi()->sendRequest<rest::ServerConnection::ErrorOrEmpty>(
        /*helper*/ nullptr,
        nx::network::http::Method::delete_,
        nx::format("/rest/v4/analytics/integrations/*/requests/%1", id),
        nx::network::rest::Params(),
        /*body*/ {},
        nx::utils::guarded(this,
            [this](bool, rest::Handle, const rest::ServerConnection::ErrorOrEmpty&)
            {
                refresh();
            }),
        thread());
}

void ApiIntegrationRequestsModel::approve(const QString& id)
{
    if (!connection()) //< It may be null if the client just disconnected from the server.
        return;

    connectedServerApi()->sendRequest<rest::ServerConnection::ErrorOrEmpty>(
        systemContext()->restApiHelper()->getSessionTokenHelper(),
        nx::network::http::Method::post,
        nx::format("/rest/v4/analytics/integrations/*/requests/%1/approve", id),
        nx::network::rest::Params(),
        /*body*/ QByteArray(),
        nx::utils::guarded(this,
            [this](bool, rest::Handle, const rest::ServerConnection::ErrorOrEmpty&)
            {
                refresh();
            }),
        thread());
}

} // namespace nx::vms::client::desktop
