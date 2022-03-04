// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtNetwork/QNetworkProxy>

#include <core/resource/resource_fwd.h>
#include <nx/utils/url.h>

class NX_VMS_COMMON_API QnNetworkProxyFactory
{
public:
    static nx::utils::Url urlToResource(
        const nx::utils::Url& baseUrl,
        const QnResourcePtr& resource,
        const QString& proxyQueryParameterName = QString());

    /**
     * @param via In not NULL filled with server, request is to be sent to.
     */
    static QNetworkProxy proxyToResource(
        const QnResourcePtr& resource,
        QnMediaServerResourcePtr* const via = nullptr);
};
