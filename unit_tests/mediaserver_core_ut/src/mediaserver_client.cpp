
#include "mediaserver_client.h"

#include <nx/network/http/fusion_data_http_client.h>


MediaServerClient::MediaServerClient(const SocketAddress& mediaServerEndpoint)
:
    m_mediaServerEndpoint(mediaServerEndpoint)
{
}

void MediaServerClient::setUserName(const QString& userName)
{
    m_userName = userName;
}

void MediaServerClient::setPassword(const QString& password)
{
    m_password = password;
}

void MediaServerClient::saveCloudSystemCredentials(
    const CloudCredentialsData& inputData,
    std::function<void(QnJsonRestResult)> completionHandler)
{
    QUrl url(lit("http://%1/api/saveCloudSystemCredentials")
        .arg(m_mediaServerEndpoint.toString()));
    url.setUserName(m_userName);
    url.setPassword(m_password);
    auto fusionClient = 
        std::make_shared<nx_http::FusionDataHttpClient<
            CloudCredentialsData, QnJsonRestResult>>(url, inputData);
    auto fusionClientPtr = fusionClient.get();
    fusionClientPtr->execute(
        [fusionClient = std::move(fusionClient),
            completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode errorCode,
                const nx_http::Response* response,
                QnJsonRestResult outData)
        {
            if (errorCode != SystemError::noError || response == nullptr)
            {
                QnJsonRestResult result;
                result.error = QnJsonRestResult::CantProcessRequest;
                return completionHandler(std::move(result));
            }

            completionHandler(std::move(outData));
        });
    //TODO #ak
}
