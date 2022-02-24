// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>
#include <common/common_globals.h>
#include <common/common_module_aware.h>

class NX_VMS_COMMON_API QnResourceStatusDictionary:
    public QObject,
    public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT
public:
    QnResourceStatusDictionary(QObject* parent);

    nx::vms::api::ResourceStatus value(const QnUuid& resourceId) const;
    void setValue(const QnUuid& resourceId, nx::vms::api::ResourceStatus status);
    void clear();
    void clear(const QVector<QnUuid>& idList);
    void remove(const QnUuid& id);
    QMap<QnUuid, nx::vms::api::ResourceStatus> values() const;
private:
    QMap<QnUuid, nx::vms::api::ResourceStatus> m_items;
    mutable nx::Mutex m_mutex;
};
