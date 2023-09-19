// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtNetwork/QAuthenticator>

#include <core/resource/media_server_resource.h>
#include <nx/vms/api/data/discovery_data.h>
#include <nx/vms/api/data/module_information.h>

#include "client_resource_fwd.h"

/**
 * Used for DesktopClient purpose to put incompatible media servers to the resource tree.
 */
class NX_VMS_CLIENT_DESKTOP_API QnFakeMediaServerResource: public QnMediaServerResource
{
    Q_OBJECT
    using base_type = QnMediaServerResource;

public:
    QnFakeMediaServerResource();

    virtual QnUuid getOriginalGuid() const override;
    void setFakeServerModuleInformation(const nx::vms::api::DiscoveredServerData& serverData);
    virtual nx::vms::api::ModuleInformation getModuleInformation() const override;

    virtual QString getName() const override;
    virtual nx::vms::api::ResourceStatus getStatus() const override;

    virtual nx::utils::Url getApiUrl() const override;

    void setAuthenticator(const QAuthenticator& authenticator);

protected:
    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;

private:
    nx::vms::api::DiscoveredServerData m_serverData;
    QAuthenticator m_authenticator;
};
