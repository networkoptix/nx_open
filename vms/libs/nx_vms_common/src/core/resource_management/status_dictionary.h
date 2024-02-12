// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/resource_types.h>

class NX_VMS_COMMON_API QnResourceStatusDictionary
{
public:
    nx::vms::api::ResourceStatus value(const nx::Uuid& resourceId) const;
    void setValue(const nx::Uuid& resourceId, nx::vms::api::ResourceStatus status);
    void clear();
    void clear(const QVector<nx::Uuid>& idList);
    void remove(const nx::Uuid& id);
    QMap<nx::Uuid, nx::vms::api::ResourceStatus> values() const;
private:
    QMap<nx::Uuid, nx::vms::api::ResourceStatus> m_items;
    mutable nx::Mutex m_mutex;
};
