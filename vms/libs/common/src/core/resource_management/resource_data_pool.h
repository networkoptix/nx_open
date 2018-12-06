#ifndef QN_RESOURCE_DATA_POOL_H
#define QN_RESOURCE_DATA_POOL_H

#include <QtCore/QObject>

#include <nx/utils/thread/mutex.h>
#include <core/resource/resource_data.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/software_version.h>

class QnResourceDataPool: public QObject
{
    Q_OBJECT;
public:
    using QnConstSecurityCamResourcePtr = QnSharedResourcePointer<const QnSecurityCamResource>;

    QnResourceDataPool(QObject *parent = NULL);
    virtual ~QnResourceDataPool();

    /**
     * \param key                       Key to get data for. Note that keys are case-insensitive.
     * \returns                         Resource data for the given key.
     */
    QnResourceData data(const QString &key) const;

    /**
     * \param camera                    resource to get data for.
     * \returns                         Resource data for the given camera.
     */
    QnResourceData data(const QnConstSecurityCamResourcePtr &camera) const;
    QnResourceData data(const QString& _vendor, const QString& model, const QString& firmware = QString()) const;

    static nx::utils::SoftwareVersion getVersion(const QByteArray& data);
    bool validateData(const QByteArray& data) const;
    bool loadFile(const QString &fileName);
    bool loadData(const QByteArray& data);
    void clear();
signals:
    void changed();
private:
    bool loadInternal(const QString &fileName);
private:
    QHash<QString, QnResourceData> m_dataByKey;
    QHash<QString, QString> m_shortVendorByName;

    /** Cache of the search results to avoid using too much regexps. */
    mutable QHash<QString, QnResourceData> m_cachedResultByKey;
    mutable QnMutex m_mutex;
};

#endif // QN_RESOURCE_DATA_POOL_H
