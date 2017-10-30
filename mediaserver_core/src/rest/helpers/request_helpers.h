#pragma once

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h>
#include <network/router.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/system_error.h>

template<typename Context, typename RequestFunc>
void runMultiserverRequest(
    QnRouter* router,
    QUrl url,
    const RequestFunc &request,
    const QnMediaServerResourcePtr &server,
    Context *context)
{
    nx_http::HttpHeaders headers;
    headers.emplace(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    const QnRoute route = router->routeTo(server->getId());
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

    url.setUserName(server->getId().toByteArray());
    url.setPassword(server->getAuthKey());

    request(url, headers, context);
}

template<typename Context, typename CompletionFunc>
void runMultiserverDownloadRequest(
    QnRouter* router,
    QUrl url,
    const QnMediaServerResourcePtr &server,
    const CompletionFunc &requestCompletionFunc,
    Context *context)
{
    const auto downloadRequest = [requestCompletionFunc]
        (const QUrl &url, const nx_http::HttpHeaders &headers, Context *context)
    {
        context->executeGuarded([url, requestCompletionFunc, headers, context]()
        {
            nx_http::downloadFileAsync(url, requestCompletionFunc, headers);
            context->incRequestsCount();
        });
    };

    runMultiserverRequest(router, url, downloadRequest, server, context);
}

template<typename Context, typename CompletionFunc>
void runMultiserverUploadRequest(
    QnRouter* router,
    QUrl url,
    const QByteArray &data,
    const QByteArray &contentType,
    const QString &user,
    const QString &password,
    const QnMediaServerResourcePtr &server,
    const CompletionFunc &completionFunc,
    Context *context)
{
    const auto downloadRequest = [completionFunc, data, contentType, user, password]
        (const QUrl &url, const nx_http::HttpHeaders &headers, Context *context)
    {
        context->executeGuarded([url, data, completionFunc, headers
            , contentType, context, user, password]()
        {
            nx_http::uploadDataAsync(url, data, contentType, headers, completionFunc);

            context->incRequestsCount();
        });
    };

    runMultiserverRequest(router, url, downloadRequest, server, context);
}

inline
bool verifyRelativePath(const QString& path)
{
    return !path.contains("..");
}
