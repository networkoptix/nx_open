#pragma once

#include <QtCore/QSharedMemory>

#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>

class QnClientInstanceManager: public QObject, public Singleton<QnClientInstanceManager>
{
    Q_OBJECT
public:
    QnClientInstanceManager(QObject *parent = 0);
    ~QnClientInstanceManager();

    int instanceIndex() const;

    QnUuid instanceGuid() const;
    QnUuid instanceGuidForIndex(int index) const;

    bool isOwnSettingsDirty() const;
    void markOtherSettingsDirty(bool value);
    void markOwnSettingsDirty(bool value);

    bool isValid() const;

    QList<int> runningInstancesIndices() const;
private:
    mutable QSharedMemory m_sharedMemory;
    int m_index;
    QnUuid m_instanceGuid;
};

#define qnClientInstanceManager QnClientInstanceManager::instance()
