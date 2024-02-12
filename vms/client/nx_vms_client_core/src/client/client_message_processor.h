// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/common_message_processor.h>
#include <client/client_connection_status.h>
#include <core/resource/resource_fwd.h>

class NX_VMS_CLIENT_CORE_API QnClientMessageProcessor: public QnCommonMessageProcessor
{
    Q_OBJECT
    using base_type = QnCommonMessageProcessor;

public:
    enum class HoldConnectionPolicy
    {
        /** Connection is broken when peer is lost. Server is marked as offline. */
        none,

        /**
         * Server update scenario. Connection is held when peer is lost, should be restored manually
         * when a client update is installed. Server is marked as offline.
         */
        update,

        /** Session password is re-entered, connection will be restored automatically. */
        reauth,
    };
    Q_ENUM(HoldConnectionPolicy)

public:
    explicit QnClientMessageProcessor(
        nx::vms::common::SystemContext* context,
        QObject* parent = nullptr);
    virtual void init(const ec2::AbstractECConnectionPtr& connection) override;

    const QnClientConnectionStatus* connectionStatus() const;

    void holdConnection(HoldConnectionPolicy policy);

protected:
    virtual Qt::ConnectionType handlerConnectionType() const override;

    virtual void disconnectFromConnection(const ec2::AbstractECConnectionPtr& connection) override;

    virtual void onResourceStatusChanged(
        const QnResourcePtr &resource,
        nx::vms::api::ResourceStatus status,
        ec2::NotificationSource source) override;
    virtual void updateResource(const QnResourcePtr& resource, ec2::NotificationSource source) override;
    virtual void onGotInitialNotification(const nx::vms::api::FullInfoData& fullData) override;

    virtual void handleRemotePeerFound(nx::Uuid peer, nx::vms::api::PeerType peerType) override;
    virtual void handleRemotePeerLost(nx::Uuid peer, nx::vms::api::PeerType peerType) override;
    virtual void removeHardwareIdMapping(const nx::Uuid& id) override;

signals:
    void hardwareIdMappingRemoved(const nx::Uuid& id);

private:
    QnClientConnectionStatus m_status;
    bool m_connected = false;
    HoldConnectionPolicy m_policy = HoldConnectionPolicy::none;
};

#define qnClientMessageProcessor \
    static_cast<QnClientMessageProcessor*>(this->messageProcessor())
