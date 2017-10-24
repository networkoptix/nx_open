#pragma once

#include <QtCore/QObject>

#include <nx/vms/cloud_integration/cloud_manager_group.h>

class CloudIntegrationManager:
    public QObject
{
    Q_OBJECT

public:
    nx::vms::cloud_integration::CloudManagerGroup cloudManagerGroup;

    CloudIntegrationManager(
        QnCommonModule* commonModule,
        nx::vms::auth::AbstractNonceProvider* defaultNonceFetcher);

private slots:
    void onCloudBindingStatusChanged(bool isBound);
};
