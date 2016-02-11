#pragma once

#include <QtCore/QSharedMemory>

#include <nx/utils/uuid.h>

class QnClientInstanceManager : public QObject {
    Q_OBJECT
public:
    QnClientInstanceManager(QObject *parent = 0);
    ~QnClientInstanceManager();

    int instanceIndex() const;

    QnUuid instanceGuid() const;
    QnUuid instanceGuidForIndex(int index) const;

    bool isValid() const;

    QList<int> runningInstancesIndices() const;
private:
    mutable QSharedMemory m_sharedMemory;
    int m_index;
    QnUuid m_instanceGuid;
};