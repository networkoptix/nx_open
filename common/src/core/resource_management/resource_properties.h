#pragma once

#include "utils/common/threadsafe_item_storage.h"
#include <nx/utils/singleton.h>
#include "nx_ec/data/api_fwd.h"
#include "nx_ec/data/api_resource_data.h"
#include "nx_ec/impl/ec_api_impl.h"

typedef QMap<QString, QString> QnResourcePropertyList;

class QnResourcePropertyDictionary: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
public:
    QnResourcePropertyDictionary(QObject* parent);

    bool saveParams(const QnUuid& resourceId);
    int saveParamsAsync(const QnUuid& resourceId);
    int saveParamsAsync(const QList<QnUuid>& resourceId);

    QString value(const QnUuid& resourceId, const QString& key) const;
    bool setValue(const QnUuid& resourceId, const QString& key, const QString& value, bool markDirty = true, bool replaceIfExists = true);
    bool hasProperty(const QnUuid& resourceId, const QString& key) const;
    bool removeProperty(const QnUuid& resourceId, const QString& key);
    ec2::ApiResourceParamDataList allProperties(const QnUuid& resourceId) const;

    /**
     * Mark all params for resource as unsaved
     **/
    void markAllParamsDirty(const QnUuid& resourceId);

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
    mutable QnMutex m_mutex;
    mutable QnMutex m_requestMutex;
};
