#pragma once

#include <QtNetwork/QAuthenticator>

#include <core/resource/media_server_resource.h>
#include <nx_ec/data/api_discovery_data.h>

class QnFakeMediaServerResource;
typedef QnSharedResourcePointer<QnFakeMediaServerResource> QnFakeMediaServerResourcePtr;

/**
 * Used for DesktopClient purpose to put incompatible media servers to the resource tree.
 */
class QnFakeMediaServerResource: public QnMediaServerResource
{
    Q_OBJECT

    using base_type = QnMediaServerResource;

public:
    QnFakeMediaServerResource();
    virtual QnUuid getOriginalGuid() const override;
    void setFakeServerModuleInformation(const ec2::ApiDiscoveredServerData& serverData);
    virtual QnModuleInformation getModuleInformation() const override;
    virtual void setStatus(Qn::ResourceStatus newStatus, Qn::StatusChangeReason reason) override;

    virtual QUrl getApiUrl() const override;

    void setAuthenticator(const QAuthenticator& authenticator);

protected:
    virtual void updateInternal(const QnResourcePtr& other, Qn::NotifierList& notifiers) override;

signals:
    void moduleInformationChanged(const QnFakeMediaServerResourcePtr& resource);

private:
    ec2::ApiDiscoveredServerData m_serverData;
    QAuthenticator m_authenticator;
};

