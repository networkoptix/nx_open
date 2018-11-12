#pragma once

#include <client/client_globals.h>
#include <client_core/connection_context_aware.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>

class QnResourceRuntimeDataManager:
    public QObject,
    public Singleton<QnResourceRuntimeDataManager>,
    public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = QObject;
public:
    QnResourceRuntimeDataManager(QObject* parent = nullptr);

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
    void layoutItemDataChanged(const QnUuid& id, Qn::ItemDataRole role, const QVariant& data);

private:
    void setDataInternal(const QnUuid& id, Qn::ItemDataRole role, const QVariant& data);

private:
    using DataHash = QHash<Qn::ItemDataRole, QVariant>;
    QHash<QnUuid, DataHash> m_data;
};

#define qnResourceRuntimeDataManager QnResourceRuntimeDataManager::instance()
