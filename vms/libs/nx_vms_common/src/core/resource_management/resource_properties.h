// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "utils/common/threadsafe_item_storage.h"
#include <common/common_module_aware.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/utils/move_only_func.h>
#include <nx_ec/ec_api_fwd.h>

class NX_VMS_COMMON_API QnResourcePropertyDictionary:
    public QObject,
    public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT
public:
    QnResourcePropertyDictionary(QObject* parent);

    bool saveParams(const QnUuid& resourceId);
    int saveParamsAsync(const QnUuid& resourceId);
    int saveParamsAsync(const QList<QnUuid>& resourceId);

    QString value(const QnUuid& resourceId, const QString& key) const;

    /** @return Whether the stored value has been modified by this call. */
    bool setValue(
        const QnUuid& resourceId,
        const QString& key,
        const QString& value,
        bool markDirty = true,
        bool replaceIfExists = true);

    bool hasProperty(const QnUuid& resourceId, const QString& key) const;
    bool hasProperty(const QString& key, const QString& value) const;
    nx::vms::api::ResourceParamDataList allProperties(const QnUuid& resourceId) const;

    /**
     * Mark all params for resource as unsaved
     **/
    void markAllParamsDirty(
        const QnUuid& resourceId,
        nx::utils::MoveOnlyFunc<bool(const QString& paramName, const QString& paramValue)> filter = nullptr);

    QHash<QnUuid, QSet<QString> > allPropertyNamesByResource() const;

    void clear();
    void clear(const QVector<QnUuid>& idList);
public slots:
    bool on_resourceParamRemoved(const QnUuid& resourceId, const QString& key);
signals:
    void asyncSaveDone(int recId, ec2::ErrorCode);
    void propertyChanged(const QnUuid& resourceId, const QString& key);
    void propertyRemoved(const QnUuid& resourceId, const QString& key);
private:
    void addToUnsavedParams(const nx::vms::api::ResourceParamWithRefDataList& params);
    void onRequestDone(int reqID, ec2::ErrorCode errorCode);
    void fromModifiedDataToSavedData(
        const QnUuid& resourceId,
        nx::vms::api::ResourceParamWithRefDataList& outData);
    int saveData(const nx::vms::api::ResourceParamWithRefDataList&& data);
    /**
     * Removes those elements from \a m_requestInProgress for which
     * comp(nx::vms::api::ResourceParamWithRefData) returns \a true
     */
    template<class Pred> void cancelOngoingRequest(const Pred& pred);
private:
    using QnResourcePropertyList = QMap<QString, QString>;
    QMap<QnUuid, QnResourcePropertyList> m_items;
    QMap<QnUuid, QnResourcePropertyList> m_modifiedItems;
    //!Used to mark value as unsaved in case of ec2 request failure
    QMap<int, nx::vms::api::ResourceParamWithRefDataList> m_requestInProgress;
    mutable nx::Mutex m_mutex;
    mutable nx::Mutex m_requestMutex;
};
