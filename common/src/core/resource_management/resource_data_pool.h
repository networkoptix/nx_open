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
     * \param camera                    resource to get data for.
     * \param useWildcard               do general wildcard search. It more slow method but allow to find values with '*' in the middle of a key value
     * \returns                         Resource data for the given camera.
     */
    QnResourceData data(const QnVirtualCameraResourcePtr &camera, bool useWildcard = false) const;
    
    bool load(const QString &fileName);

private:
    bool loadInternal(const QString &fileName);

private:
    QHash<QString, QnResourceData> m_dataByKey;
    QHash<QString, QString> m_shortVendorByName;
};


#endif // QN_RESOURCE_DATA_POOL_H
