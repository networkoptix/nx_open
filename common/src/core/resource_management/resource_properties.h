#ifndef __RESOURCE_PROPERTY_DICTIONARY_H
#define __RESOURCE_PROPERTY_DICTIONARY_H

#include "utils/common/threadsafe_item_storage.h"
#include "utils/common/singleton.h"
#include "nx_ec/data/api_fwd.h"
#include "nx_ec/data/api_resource_data.h"
#include "nx_ec/impl/ec_api_impl.h"

typedef QHash<QString, QString> QnResourcePropertyList;

class QnResourcePropertyDictionary: public QObject, 
    public Singleton<QnResourcePropertyDictionary>
{
    Q_OBJECT
public:
    void saveParams(const QnUuid& resourceId);
    int saveParamsAsync(const QnUuid& resourceId);
    int saveParamsAsync(const QList<QnUuid>& resourceId);
    
    QString value(const QnUuid& resourceId, const QString& key) const;
    bool setValue(const QnUuid& resourceId, const QString& key, const QString& value, bool markDirty = true, bool replaceIfExists = true);
    bool hasProperty(const QnUuid& resourceId, const QString& key) const;
    bool removeProperty(const QnUuid& resourceId, const QString& key);
    ec2::ApiResourceParamDataList allProperties(const QnUuid& resourceId) const;
    
    QHash<QnUuid, QSet<QString> > allPropertyNamesByResource() const;

    void clear();
    void clear(const QVector<QnUuid>& idList);
signals:
    void asyncSaveDone(int recId, ec2::ErrorCode);
private:
    void addToUnsavedParams(const ec2::ApiResourceParamWithRefDataList& params);
    void onRequestDone( int reqID, ec2::ErrorCode errorCode );
    void fromModifiedDataToSavedData(const QnUuid& resourceId, ec2::ApiResourceParamWithRefDataList& outData);
    int saveData(const ec2::ApiResourceParamWithRefDataList&& data);
    //!Removes those elements from \a m_requestInProgress for which comp(ec2::ApiResourceParamWithRefData) returns \a true
    template<class Pred> void cancelOngoingRequest(const Pred& pred);
private:
    QMap<QnUuid, QnResourcePropertyList> m_items;
    QMap<QnUuid, QnResourcePropertyList> m_modifiedItems;
    //!Used to mark value as unsaved in case of ec2 request failure
    QMap<int, ec2::ApiResourceParamWithRefDataList> m_requestInProgress;
    mutable QMutex m_mutex;
    mutable QMutex m_requestMutex;
};

#define propertyDictionary QnResourcePropertyDictionary::instance()

#endif // __RESOURCE_PROPERTY_DICTIONARY_H
