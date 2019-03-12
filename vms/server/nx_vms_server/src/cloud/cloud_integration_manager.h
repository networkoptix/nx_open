#pragma once

#include <QtCore/QObject>

#include <nx/vms/cloud_integration/cloud_manager_group.h>

#include <cloud_integration/cloud_connector.h>
#include <nx/vms/server/server_module_aware.h>

namespace ec2 { class AbstractTransactionMessageBus; } // namespace ec2

class CloudIntegrationManager: public QObject, public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT

public:
    CloudIntegrationManager(
        QnMediaServerModule* serverModule,
        ::ec2::AbstractTransactionMessageBus* transactionMessageBus,
        nx::vms::auth::AbstractNonceProvider* defaultNonceFetcher);

    nx::vms::cloud_integration::CloudManagerGroup& cloudManagerGroup();

private:
    ::ec2::CloudConnector m_cloudConnector;
    nx::vms::cloud_integration::CloudManagerGroup m_cloudManagerGroup;

    void onCloudBindingStatusChanged(bool isBound);
};
