// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QList>

#include <nx/utils/singleton.h>
#include <nx/vms/api/data/runtime_data.h>
#include <utils/common/threadsafe_item_storage.h>

class QnCommonMessageProcessor;

struct QnPeerRuntimeInfo
{
    QnPeerRuntimeInfo(){}
    QnPeerRuntimeInfo(const nx::vms::api::RuntimeData& runtimeData):
        uuid(runtimeData.peer.id),
        data(runtimeData){}

    QnUuid uuid;
    nx::vms::api::RuntimeData data;

    bool operator==(const QnPeerRuntimeInfo& other) const
    {
        return uuid == other.uuid &&
            data == other.data;
    }

    bool isNull() const
    {
        return uuid.isNull();
    }
};


Q_DECLARE_METATYPE(QnPeerRuntimeInfo)
Q_DECLARE_TYPEINFO(QnPeerRuntimeInfo, Q_MOVABLE_TYPE);

typedef QList<QnPeerRuntimeInfo> QnPeerRuntimeInfoList;
typedef QHash<QnUuid, QnPeerRuntimeInfo> QnPeerRuntimeInfoMap;

Q_DECLARE_METATYPE(QnPeerRuntimeInfoList)
Q_DECLARE_METATYPE(QnPeerRuntimeInfoMap)


class NX_VMS_COMMON_API QnRuntimeInfoManager:
    public QObject,
    private QnThreadsafeItemStorageNotifier<QnPeerRuntimeInfo>
{
    Q_OBJECT

public:
    QnRuntimeInfoManager(QObject* parent = nullptr);

    const QnThreadsafeItemStorage<QnPeerRuntimeInfo>* items() const;

    QnPeerRuntimeInfo localInfo() const;
    bool hasItem(const QnUuid& id);
    QnPeerRuntimeInfo item(const QnUuid& id) const;

    void setMessageProcessor(QnCommonMessageProcessor* messageProcessor);
    void updateLocalItem(const QnPeerRuntimeInfo& value);
    void updateRemoteItem(const QnPeerRuntimeInfo& value);

signals:
    void runtimeInfoAdded(const QnPeerRuntimeInfo& data);
    void runtimeInfoChanged(const QnPeerRuntimeInfo& data);
    void runtimeInfoRemoved(const QnPeerRuntimeInfo& data);

private:
    virtual Qn::Notifier storedItemAdded(const QnPeerRuntimeInfo& item) override;
    virtual Qn::Notifier storedItemRemoved(const QnPeerRuntimeInfo& item) override;
    virtual Qn::Notifier storedItemChanged(const QnPeerRuntimeInfo& item) override;

    void updateItem(const QnPeerRuntimeInfo& value);

private:
    /** Mutex that is to be used when accessing items. */
    mutable nx::Mutex m_mutex;
    mutable nx::Mutex m_updateMutex;
    QnPeerRuntimeInfo m_localInfo;
    QScopedPointer<QnThreadsafeItemStorage<QnPeerRuntimeInfo>> m_items;
    QnCommonMessageProcessor* m_messageProcessor = nullptr;
};

