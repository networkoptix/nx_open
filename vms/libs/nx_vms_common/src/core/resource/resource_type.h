// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QSharedPointer>

#include <core/resource/resource_fwd.h>
#include <nx/utils/thread/mutex.h>

#include "resource_property_key.h"

class NX_VMS_COMMON_API QnResourceType
{
public:
    using ParamTypeMap = QMap<QString, QString>;

    virtual ~QnResourceType();

    void setId(const QnUuid& value) { m_id = value; }
    const QnUuid& getId() const { return m_id;}

    void setParentId(const QnUuid &value);
    const QnUuid& getParentId() const { return m_parentId;}

    void setName(const QString& value) { m_name = value; }
    const QString& getName() const { return m_name;}

    void setManufacture(const QString& value) { m_manufacture = value; }
    const QString& getManufacturer() const { return m_manufacture;}

    bool isCamera() const;
    void setIsCamera(); //< For tests.

    void addAdditionalParent(const QnUuid& parent);
    QList<QnUuid> allParentList() const;

    void addParamType(const QString& name, const QString& defaultValue);
    bool hasParam(const QString& name) const;

    const ParamTypeMap& paramTypeListUnsafe() const;
    const ParamTypeMap paramTypeList() const;

    QString defaultValue(const QString& key) const;

private:
    QnUuid m_id;
    QnUuid m_parentId;
    QString m_name;
    QString m_manufacture;
    QList<QnUuid> m_additionalParentList;

    ParamTypeMap m_paramTypeList;

    mutable nx::Mutex m_allParamTypeListCacheMutex;
    mutable QSharedPointer<ParamTypeMap> m_allParamTypeListCache;

    mutable bool m_isCamera = false;
    mutable bool m_isCameraSet = false;
};

class NX_VMS_COMMON_API QnResourceTypePool
{
public:
    typedef QMap<QnUuid, QnResourceTypePtr> QnResourceTypeMap;

    static const QString kC2pCameraTypeId;

    static QnResourceTypePool *instance();

    QnResourceTypePool();
    QnResourceTypePtr getResourceTypeByName(const QString& name) const;
    QnResourceTypePtr getResourceType(QnUuid id) const;
    void addResourceType(QnResourceTypePtr resourceType);
    void replaceResourceTypeList(const QnResourceTypeList& resourceType);

    /* exact match name */
    QnUuid getResourceTypeId(const QString& manufacturer, const QString& name, bool showWarning = true) const;

    /* match name using like operation */
    QnUuid getLikeResourceTypeId(const QString& manufacturer, const QString& name) const;

    QnResourceTypeMap getResourceTypeMap() const;

    bool isEmpty() const;

private:
    mutable nx::Mutex m_mutex;
    QnResourceTypeMap m_resourceTypeMap;
    mutable QnResourceTypePtr m_desktopCamResourceType;
};

#define qnResTypePool QnResourceTypePool::instance()
