#ifndef __RUNTIME_INFO_MANAGER_H_
#define __RUNTIME_INFO_MANAGER_H_

#include <QtCore/QMap>
#include <QtCore/QList>

#include <nx_ec/data/api_fwd.h>
#include <nx_ec/data/api_runtime_data.h>

#include <utils/common/singleton.h>
#include <utils/common/threadsafe_item_storage.h>

struct QnPeerRuntimeInfo {
    QnPeerRuntimeInfo(){}
    QnPeerRuntimeInfo(const ec2::ApiRuntimeData& runtimeData):
        uuid(runtimeData.peer.id),
        data(runtimeData){}

    QnUuid uuid;
    ec2::ApiRuntimeData data;

    bool operator==(const QnPeerRuntimeInfo& other) const {
        return uuid == other.uuid &&
            data == other.data;
    }

    bool isNull() const {
        return uuid.isNull();
    }
};


Q_DECLARE_METATYPE(QnPeerRuntimeInfo)
Q_DECLARE_TYPEINFO(QnPeerRuntimeInfo, Q_MOVABLE_TYPE);

typedef QList<QnPeerRuntimeInfo> QnPeerRuntimeInfoList;
typedef QHash<QnUuid, QnPeerRuntimeInfo> QnPeerRuntimeInfoMap;

Q_DECLARE_METATYPE(QnPeerRuntimeInfoList)
Q_DECLARE_METATYPE(QnPeerRuntimeInfoMap)


class QnRuntimeInfoManager: public QObject, 
    public Singleton<QnRuntimeInfoManager>,
    private QnThreadsafeItemStorageNotifier<QnPeerRuntimeInfo>
{
    Q_OBJECT
public:
    QnRuntimeInfoManager(QObject* parent = NULL);

    const QnThreadsafeItemStorage<QnPeerRuntimeInfo> *items() const;

    QnPeerRuntimeInfo localInfo() const;
    QnPeerRuntimeInfo remoteInfo() const;
    bool hasItem(const QnUuid& id);
    QnPeerRuntimeInfo item(const QnUuid& id) const;

    void updateLocalItem(const QnPeerRuntimeInfo& value);
signals:
    void runtimeInfoAdded(const QnPeerRuntimeInfo &data);
    void runtimeInfoChanged(const QnPeerRuntimeInfo &data);
    void runtimeInfoRemoved(const QnPeerRuntimeInfo &data);
private:
    virtual void storedItemAdded(const QnPeerRuntimeInfo &item) override;
    virtual void storedItemRemoved(const QnPeerRuntimeInfo &item) override;
    virtual void storedItemChanged(const QnPeerRuntimeInfo &item) override;
private:
    /** Mutex that is to be used when accessing items. */
    mutable QnMutex m_mutex;
    mutable QnMutex m_updateMutex;
    QScopedPointer<QnThreadsafeItemStorage<QnPeerRuntimeInfo> > m_items;
};

#endif
