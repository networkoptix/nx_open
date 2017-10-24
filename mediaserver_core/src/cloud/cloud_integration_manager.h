#pragma once

#include <QtCore/QObject>

#include "cloud_manager_group.h"

class CloudIntegrationManager:
    public QObject
{
    Q_OBJECT

public:
    CloudManagerGroup cloudManagerGroup;

    CloudIntegrationManager(
        QnCommonModule* commonModule,
        AbstractNonceProvider* defaultNonceFetcher);

private slots:
    void onCloudBindingStatusChanged(bool isBound);
};
