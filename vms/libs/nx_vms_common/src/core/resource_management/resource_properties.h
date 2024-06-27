// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/common/system_context_aware.h>
#include <nx_ec/ec_api_fwd.h>
#include <utils/common/threadsafe_item_storage.h>

class NX_VMS_COMMON_API QnResourcePropertyDictionary:
    public QObject,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT
public:
    QnResourcePropertyDictionary(
        nx::vms::common::SystemContext* context,
        QObject* parent = nullptr);

    bool saveParams(const nx::Uuid& resourceId);
    int saveParamsAsync(const nx::Uuid& resourceId);
    int saveParamsAsync(const QList<nx::Uuid>& resourceId);

    QString value(const nx::Uuid& resourceId, const QString& key) const;

    /** @return Whether the stored value has been modified by this call. */
    bool setValue(
        const nx::Uuid& resourceId,
        const QString& key,
        const QString& value,
        bool markDirty = true);

    bool hasProperty(const nx::Uuid& resourceId, const QString& key) const;
    bool hasProperty(const QString& key, const QString& value) const;
    nx::vms::api::ResourceParamDataList allProperties(const nx::Uuid& resourceId) const;
    nx::vms::api::ResourceParamWithRefDataList allProperties() const;
    QMap<QString, QString> modifiedProperties(const nx::Uuid& resourceId) const;

    /**
     * Mark all params for resource as unsaved
     **/
    void markAllParamsDirty(
        const nx::Uuid& resourceId,
        nx::utils::MoveOnlyFunc<bool(const QString& paramName, const QString& paramValue)> filter = nullptr);

    QHash<nx::Uuid, QSet<QString> > allPropertyNamesByResource() const;

    void clear();
    void clear(const QVector<nx::Uuid>& idList);
public slots:
    bool on_resourceParamRemoved(const nx::Uuid& resourceId, const QString& key);
signals:
    void asyncSaveDone(int recId, ec2::ErrorCode);
    void propertyChanged(const nx::Uuid& resourceId, const QString& key);
    void propertyRemoved(const nx::Uuid& resourceId, const QString& key);
private:
    void onRequestDone(int reqID, ec2::ErrorCode errorCode);
    void fromModifiedDataToSavedData(
        const nx::Uuid& resourceId,
        nx::vms::api::ResourceParamWithRefDataList& outData);
    int saveData(const nx::vms::api::ResourceParamWithRefDataList&& data);
private:
    using QnResourcePropertyList = QMap<QString, QString>;
    QMap<nx::Uuid, QnResourcePropertyList> m_items;
    QMap<nx::Uuid, QnResourcePropertyList> m_modifiedItems;
    mutable nx::Mutex m_mutex;
};
