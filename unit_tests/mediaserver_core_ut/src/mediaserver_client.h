
#pragma once

#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>

#include <api/model/cloud_credentials_data.h>
#include <rest/server/json_rest_result.h>


class MediaServerClient
{
public:
    MediaServerClient(const SocketAddress& mediaServerEndpoint);

    void setUserName(const QString& userName);
    void setPassword(const QString& password);

    void saveCloudSystemCredentials(
        const CloudCredentialsData& inputData,
        std::function<void(QnJsonRestResult)> completionHandler);

private:
    const SocketAddress m_mediaServerEndpoint;
    QString m_userName;
    QString m_password;
};
