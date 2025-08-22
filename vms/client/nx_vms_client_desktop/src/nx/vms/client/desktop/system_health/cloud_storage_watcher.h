// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class CloudStorageWatcher:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    explicit CloudStorageWatcher(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~CloudStorageWatcher();

    int enabledCloudStorageCount() const;
    int availableCloudStorageCount() const;
private:
    void update(int availableCount, int enabledCount);

signals:
    void cloudStorageStateChanged();

private:
    int m_enabledCloudStorageCount = 0;
    int m_availableCloudStorageCount = 0;
};

} // namespace nx::vms::client::desktop
