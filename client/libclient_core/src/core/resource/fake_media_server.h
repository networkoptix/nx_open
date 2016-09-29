#pragma once

#include <core/resource/media_server_resource.h>
#include <nx_ec/data/api_discovery_data.h>

class QnFakeMediaServerResource: public QnMediaServerResource
{
    Q_OBJECT

    typedef QnMediaServerResource base_type;
public:
    QnFakeMediaServerResource() {}

    void setFakeServerModuleInformation(const ec2::ApiDiscoveredServerData& serverData);
    virtual QnModuleInformation getModuleInformation() const override;
signals:
    void moduleInformationChanged(const QnResourcePtr &resource);
private:
    ec2::ApiDiscoveredServerData m_serverData;
    mutable QnMutex m_mutex;
};

typedef QnSharedResourcePointer<QnFakeMediaServerResource> QnFakeMediaServerResourcePtr;
