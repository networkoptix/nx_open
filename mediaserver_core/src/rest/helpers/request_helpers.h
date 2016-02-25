#pragma once

#include <network/router.h>
#include <http/custom_headers.h>
#include <nx/network/http/asynchttpclient.h>
#include <utils/common/systemerror.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

template<typename Context, typename RequestFunc>
void runMultiserverRequest(QUrl url
    , const RequestFunc &request
    , const QnMediaServerResourcePtr &server
    , Context *context)
{
    nx_http::HttpHeaders headers;
    headers.emplace(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    const QnRoute route = QnRouter::instance()->routeTo(server->getId());
    if (route.reverseConnect)
    {
        static const auto kLocalHost = "127.0.0.1";

        // Proxy via himself to activate backward connection logic
        url.setHost(kLocalHost);
        url.setPort(context->ownerPort());
    }
    else if (!route.addr.isNull())
    {
        url.setHost(route.addr.address.toString());
        url.setPort(route.addr.port);
    }

    if (const QnUserResourcePtr admin = qnResPool->getAdministrator()) {
        url.setUserName(admin->getName());
        url.setPassword(QString::fromUtf8(admin->getDigest()));
    }

    request(url, headers, context);
}

template<typename Context, typename CompletionFunc>
void runMultiserverDownloadRequest(QUrl url
    , const QnMediaServerResourcePtr &server
    , const CompletionFunc &requestCompletionFunc
    , Context *context)
{
    const auto downloadRequest = [requestCompletionFunc]
        (const QUrl &url, const nx_http::HttpHeaders &headers, Context *context)
    {
        context->executeGuarded([url, requestCompletionFunc, headers, context]()
        {
            nx_http::downloadFileAsync(url, requestCompletionFunc, headers,
                                       nx_http::AsyncHttpClient::authDigestWithPasswordHash);
            context->incRequestsCount();
        });
    };

    runMultiserverRequest(url, downloadRequest, server, context);
}

template<typename Context, typename CompletionFunc>
void runMultiserverUploadRequest(QUrl url
    , const QByteArray &data
    , const QByteArray &contentType
    , const QString &user
    , const QString &password
    , const QnMediaServerResourcePtr &server
    , const CompletionFunc &completionFunc
    , Context *context)
{
    const auto downloadRequest = [completionFunc, data, contentType, user, password]
        (const QUrl &url, const nx_http::HttpHeaders &headers, Context *context)
    {
        context->executeGuarded([url, data, completionFunc, headers, user, password, contentType, context]()
        {
            nx_http::uploadDataAsync(url, data, contentType, user, password, headers, completionFunc);
            context->incRequestsCount();
        });
    };

    runMultiserverRequest(url, downloadRequest, server, context);
}
