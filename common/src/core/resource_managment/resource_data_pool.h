#ifndef QN_RESOURCE_DATA_POOL_H
#define QN_RESOURCE_DATA_POOL_H

#include <QtCore/QObject>
#include <QtCore/QMutex>

#include <core/resource/resource_data.h>
#include <core/resource/resource_fwd.h>

class QnResourceDataPool: public QObject {
    Q_OBJECT;
public:
    QnResourceDataPool(QObject *parent = NULL);
    virtual ~QnResourceDataPool();

    QnResourceData data(const QString &key) const;
    QnResourceData data(const QnVirtualCameraResourcePtr &camera) const;
    
    bool load(const QString &fileName);

private:
    bool loadInternal(const QString &fileName);

private:
    mutable QMutex m_mutex;
    QHash<QString, QnResourceData> m_dataByKey;
};


#endif // QN_RESOURCE_DATA_POOL_H
