#pragma once

#include <network/router.h>
#include <http/custom_headers.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/common/systemerror.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

template<typename Context, typename CompletionFunc>
void runMultiserverDownloadRequest(QUrl url
    , const QnMediaServerResourcePtr &server
    , const CompletionFunc &requestCompletionFunc
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

    context->executeGuarded([url, requestCompletionFunc, headers, context]()
    {
        if (nx_http::downloadFileAsync(url, requestCompletionFunc, headers,
            nx_http::AsyncHttpClient::authDigestWithPasswordHash))
        {
            context->incRequestsCount();
        }
    });
}
