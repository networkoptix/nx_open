#ifndef __RESOURCE_PROPERTY_DICTIONARY_H
#define __RESOURCE_PROPERTY_DICTIONARY_H

#include "utils/common/threadsafe_item_storage.h"
#include "utils/common/singleton.h"
#include "nx_ec/data/api_fwd.h"
#include "nx_ec/impl/ec_api_impl.h"

typedef QHash<QString, QString> QnResourcePropertyList;

class QnResourcePropertyDictionary: public QObject, 
    public Singleton<QnResourcePropertyDictionary>
{
    Q_OBJECT
public:
    void reset();

    void saveParams(const QnUuid& resourceId);
    int saveParamsAsync(const QnUuid& resourceId);
    int saveParamsAsync(const QList<QnUuid>& resourceId);
    
    QString value(const QnUuid& resourceId, const QString& key) const;
    bool setValue(const QnUuid& resourceId, const QString& key, const QString& value, bool markDirty = true);
    bool hasProperty(const QnUuid& resourceId, const QString& key) const;
    ec2::ApiResourceParamDataList allProperties(const QnUuid& resourceId) const;
    void clear();
signals:
    void asyncSaveDone(int recId, ec2::ErrorCode);
private:
    void addToUnsavedParams(const ec2::ApiResourceParamWithRefDataList& params);
    void onRequestDone( int reqID, ec2::ErrorCode errorCode );
    void fromModifiedDataToSavedData(const QnUuid& resourceId, ec2::ApiResourceParamWithRefDataList& outData);
    int saveData(const ec2::ApiResourceParamWithRefDataList& data);
private:
    QMap<QnUuid, QnResourcePropertyList> m_items;
    QMap<QnUuid, QnResourcePropertyList> m_modifiedItems;
    QMap<int, ec2::ApiResourceParamWithRefDataList> m_requestInProgress;
    mutable QMutex m_mutex;
};

#define propertyDictionay QnResourcePropertyDictionary::instance()

#endif // __RESOURCE_PROPERTY_DICTIONARY_H
