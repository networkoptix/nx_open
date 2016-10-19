#pragma once

#include <core/resource/media_server_resource.h>
#include <nx_ec/data/api_discovery_data.h>

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
    virtual void setStatus(Qn::ResourceStatus newStatus, bool silenceMode) override;

    virtual QUrl getApiUrl() const override;

    void setUserCredentials(const QString& user, const QString& password);

protected:
    virtual void updateInternal(const QnResourcePtr& other, Qn::NotifierList& notifiers) override;

signals:
    void moduleInformationChanged(const QnResourcePtr &resource);

private:
    ec2::ApiDiscoveredServerData m_serverData;
    QString m_user;
    QString m_password;
};

typedef QnSharedResourcePointer<QnFakeMediaServerResource> QnFakeMediaServerResourcePtr;
