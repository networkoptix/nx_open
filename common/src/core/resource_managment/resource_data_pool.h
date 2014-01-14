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

    /**
     * \param key                       Key to get data for. Note that keys are case-insensitive.
     * \returns                         Resource data for the given key.
     */
    QnResourceData data(const QString &key) const;

    /**
     * \param                           Camera resource to get data for.
     * \returns                         Resource data for the given camera.
     */
    QnResourceData data(const QnVirtualCameraResourcePtr &camera) const;
    
    bool load(const QString &fileName);

private:
    bool loadInternal(const QString &fileName);

private:
    mutable QMutex m_mutex;
    QHash<QString, QnResourceData> m_dataByKey;
};


#endif // QN_RESOURCE_DATA_POOL_H
