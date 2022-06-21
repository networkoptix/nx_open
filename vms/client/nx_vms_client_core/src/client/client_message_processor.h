// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/common_message_processor.h>
#include <client/client_connection_status.h>
#include <core/resource/resource_fwd.h>

class NX_VMS_CLIENT_CORE_API QnClientMessageProcessor: public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;

public:
    explicit QnClientMessageProcessor(
        nx::vms::common::SystemContext* context,
        QObject* parent = nullptr);
    virtual void init(const ec2::AbstractECConnectionPtr& connection) override;

    const QnClientConnectionStatus* connectionStatus() const;

    void setHoldConnection(bool holdConnection);
    bool isConnectionHeld() const;

protected:
    virtual Qt::ConnectionType handlerConnectionType() const override;

    virtual void disconnectFromConnection(const ec2::AbstractECConnectionPtr& connection) override;

    virtual void handleTourAddedOrUpdated(const nx::vms::api::LayoutTourData& tour) override;

    virtual void onResourceStatusChanged(
        const QnResourcePtr &resource,
        nx::vms::api::ResourceStatus status,
        ec2::NotificationSource source) override;
    virtual void updateResource(const QnResourcePtr& resource, ec2::NotificationSource source) override;
    virtual void onGotInitialNotification(const nx::vms::api::FullInfoData& fullData) override;

    virtual void handleRemotePeerFound(QnUuid peer, nx::vms::api::PeerType peerType) override;
    virtual void handleRemotePeerLost(QnUuid peer, nx::vms::api::PeerType peerType) override;
    virtual void removeHardwareIdMapping(const QnUuid& id) override;

signals:
    void hardwareIdMappingRemoved(const QnUuid& id);

private:
    QnClientConnectionStatus m_status;
    bool m_connected = false;
    bool m_holdConnection = false;
};

#define qnClientMessageProcessor \
    static_cast<QnClientMessageProcessor*>(this->messageProcessor())
