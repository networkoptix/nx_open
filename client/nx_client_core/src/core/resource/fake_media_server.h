#pragma once

#include <QtNetwork/QAuthenticator>

#include <core/resource/media_server_resource.h>
#include <core/resource/client_core_resource_fwd.h>

#include <nx/vms/api/data/discovery_data.h>
#include <nx/vms/api/data/module_information.h>

/**
 * Used for DesktopClient purpose to put incompatible media servers to the resource tree.
 */
class QnFakeMediaServerResource: public QnMediaServerResource
{
    Q_OBJECT
    using base_type = QnMediaServerResource;

public:
    QnFakeMediaServerResource(QnCommonModule* commonModule);

    virtual QnUuid getOriginalGuid() const override;
    void setFakeServerModuleInformation(const nx::vms::api::DiscoveredServerData& serverData);
    virtual nx::vms::api::ModuleInformation getModuleInformation() const override;

    virtual QString getName() const override;
    virtual Qn::ResourceStatus getStatus() const override;

    virtual nx::utils::Url getApiUrl() const override;

    void setAuthenticator(const QAuthenticator& authenticator);

protected:
    virtual void updateInternal(const QnResourcePtr& other, Qn::NotifierList& notifiers) override;

signals:
    void moduleInformationChanged(const QnFakeMediaServerResourcePtr& resource);

private:
    nx::vms::api::DiscoveredServerData m_serverData;
    QAuthenticator m_authenticator;
};

