#pragma once

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

#include <nx/utils/singleton.h>

class QnResourceRuntimeDataManager: public QObject, public Singleton<QnResourceRuntimeDataManager>
{
    Q_OBJECT
    using base_type = QObject;
public:
    QnResourceRuntimeDataManager(QObject* parent = nullptr);

    QVariant resourceData(const QnResourcePtr& resource, Qn::ItemDataRole role) const;
    void setResourceData(const QnResourcePtr& resource, Qn::ItemDataRole role, QVariant data);

    QVariant layoutItemData(const QnUuid& id, Qn::ItemDataRole role) const;
    void setLayoutItemData(const QnUuid& id, Qn::ItemDataRole role, QVariant data);

    template<class T>
    void setLayoutItemData(const QnUuid& id, Qn::ItemDataRole role, const T &value)
    {
        setLayoutItemData(id, role, QVariant::fromValue<T>(value));
    }

private:
    void setDataInternal(const QnUuid& id, Qn::ItemDataRole role, QVariant data);

private:
    using DataHash = QHash<Qn::ItemDataRole, QVariant>;
    QHash<QnUuid, DataHash> m_data;
};

#define qnResourceRuntimeDataManager QnResourceRuntimeDataManager::instance()