// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <utils/common/functional.h>

class NX_VMS_CLIENT_DESKTOP_API QnResourceRuntimeDataManager:
    public QObject,
    public nx::vms::client::desktop::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;
public:
    QnResourceRuntimeDataManager(
        nx::vms::client::desktop::SystemContext* systemContext,
        QObject* parent = nullptr);

    QVariant resourceData(const QnResourcePtr& resource, Qn::ItemDataRole role) const;
    void setResourceData(const QnResourcePtr& resource, Qn::ItemDataRole role, const QVariant& data);
    void cleanupResourceData(const QnResourcePtr& resource, Qn::ItemDataRole role);
    void cleanupResourceData(const QnResourcePtr& resource);

    QVariant layoutItemData(const QnUuid& id, Qn::ItemDataRole role) const;
    void setLayoutItemData(const QnUuid& id, Qn::ItemDataRole role, const QVariant& data);

    void cleanupData(const QnUuid& id, Qn::ItemDataRole role);
    void cleanupData(const QnUuid& id);

    template<class T>
    void setLayoutItemData(const QnUuid& id, Qn::ItemDataRole role, const T &value)
    {
        setLayoutItemData(id, role, QVariant::fromValue<T>(value));
    }

signals:
    void resourceDataChanged(const QnUuid& id, Qn::ItemDataRole role, const QVariant& data);
    void layoutItemDataChanged(const QnUuid& id, Qn::ItemDataRole role, const QVariant& data);

private:
    Qn::Notifier setDataInternal(const QnUuid& id, Qn::ItemDataRole role, const QVariant& data);

private:
    mutable nx::Mutex m_mutex;
    using DataHash = QHash<Qn::ItemDataRole, QVariant>;
    QHash<QnUuid, DataHash> m_data;
};
