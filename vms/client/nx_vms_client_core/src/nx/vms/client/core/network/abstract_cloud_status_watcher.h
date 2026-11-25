// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <network/cloud_system_data.h>

namespace nx::network::http { class Credentials; }

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API AbstractCloudStatusWatcher: public QObject
{
    Q_OBJECT

public:
    enum Status
    {
        LoggedOut,
        Online,
        Offline, //< User is logged in with "stay connected" checked but internet connection is lost.
        UpdatingCredentials, //< User credentials are being updated without requiring logout.
    };
    Q_ENUM(Status)

    Q_PROPERTY(Status status READ status NOTIFY statusChanged)

    explicit AbstractCloudStatusWatcher(QObject* parent = nullptr);
    virtual ~AbstractCloudStatusWatcher() override;

    virtual Status status() const = 0;
    virtual QnCloudSystemList cloudSystems() const = 0;

    /** User name and access token with global scope. */
    virtual nx::network::http::Credentials credentials() const = 0;

signals:
    void statusChanged(Status status);
    void credentialsChanged();
    void cloudLoginChanged();
    void cloudSystemsChanged(const QnCloudSystemList& currentCloudSystems);
};

} // namespace nx::vms::client::core
