#pragma once

#include <QtCore/QSharedMemory>

#include <utils/common/uuid.h>

class QnClientInstanceManager : public QObject {
    Q_OBJECT
public:
    QnClientInstanceManager(QObject *parent = 0);
    ~QnClientInstanceManager();

    int instanceIndex() const;
    QnUuid instanceGuid() const;
    bool isValid() const;
private:
    mutable QSharedMemory m_sharedMemory;
    int m_index;
    QnUuid m_instanceGuid;
};