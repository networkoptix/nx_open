#ifndef __RESOURCE_PROPERTY_DICTIONARY_H
#define __RESOURCE_PROPERTY_DICTIONARY_H

#include "utils/common/threadsafe_item_storage.h"
#include "utils/common/singleton.h"
#include "nx_ec/data/api_fwd.h"
#include "nx_ec/impl/ec_api_impl.h"

typedef QHash<QString, QString> QnResourcePropertyList;

class QnResourcePropertyDictionary: public QObject, 
    public Singleton<QnResourcePropertyDictionary>,
    private QnThreadsafeItemStorageNotifier<QnResourcePropertyList>
{
    Q_OBJECT
public:
    void reset();

    void saveParams(const QnUuid& resourceId);
    void saveParamsAsync(const QnUuid& resourceId);
private:
    void addToUnsavedParams(const QnUuid& resourceId, const ec2::ApiResourceParamDataList& params);
    void onRequestDone( int reqID, ec2::ErrorCode errorCode );
private:
    QScopedPointer<QnThreadsafeItemStorage<QnResourcePropertyList> > m_items;
    QMap<QnUuid, QnResourcePropertyList> m_modifiedItems;
    QMap<int, ec2::ApiResourceParamListWithIdData> m_requestInProgress;
    QMutex m_mutex;
};

#define propertyDictionay QnResourcePropertyDictionary::instance()

#endif // __RESOURCE_PROPERTY_DICTIONARY_H
