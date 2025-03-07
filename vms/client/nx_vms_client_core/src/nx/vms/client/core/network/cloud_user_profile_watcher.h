// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/utils/coro/task.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/network/cloud_api.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>

namespace nx::vms::client::core {

// Provides access to the current user profile data loaded from the cloud.
class CloudUserProfileWatcher: public QObject
{
    Q_OBJECT

    using base_type = QObject;

    Q_PROPERTY(CloudStatusWatcher* statusWatcher MEMBER m_statusWatcher NOTIFY statusWatcherChanged);

    Q_PROPERTY(QString fullName READ fullName NOTIFY fullNameChanged)
    Q_PROPERTY(QVariantList channelPartnerList
        READ channelPartnerList
        NOTIFY channelPartnerListChanged)
    Q_PROPERTY(nx::Uuid channelPartner
        READ channelPartner
        WRITE setChannelPartner
        NOTIFY channelPartnerChanged)

    Q_PROPERTY(QUrl avatarUrl READ avatarUrl NOTIFY avatarUrlChanged)

public:
    CloudUserProfileWatcher(QObject* parent = nullptr);
    virtual ~CloudUserProfileWatcher() override;

    QString fullName() const;

    QVariantList channelPartnerList() const;

    nx::Uuid channelPartner() const;
    void setChannelPartner(nx::Uuid channelPartner);

    QUrl avatarUrl() const;

protected:
    coro::FireAndForget run();

signals:
    void fullNameChanged();
    void channelPartnerListChanged();
    void channelPartnerChanged();
    void avatarUrlChanged();
    void statusWatcherChanged();

private:
    QPointer<CloudStatusWatcher> m_statusWatcher;
    QString m_fullName;
    ChannelPartnerList m_channelPartnerList;
    nx::Uuid m_channelPartner; //< Selected channel partner. TODO: move to persistent storage.
};

} // nx::vms::client::core
