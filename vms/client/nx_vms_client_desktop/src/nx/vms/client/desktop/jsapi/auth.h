// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <core/resource/resource.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

namespace nx::vms::client::desktop::jsapi {

class Auth: public QObject, public core::RemoteConnectionAware
{
    Q_OBJECT
public:
    using base_type = QObject;
    using UrlProvider = std::function<QUrl()>;

    Auth(UrlProvider urlProvider, const QnResourcePtr& resource, QObject* parent = nullptr);

    Q_INVOKABLE QString sessionToken() const;

private:
    bool isApproved(const QUrl& url) const;

private:
    UrlProvider m_url;
    QnResourcePtr m_resource;
};

} // namespace nx::vms::client::desktop::jsapi
