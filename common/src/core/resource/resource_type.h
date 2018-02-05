#ifndef __RESOURCE_TYPE_H
#define __RESOURCE_TYPE_H

#include <QtCore/QList>
#include <QtCore/QMap>
#include <nx/utils/thread/mutex.h>
#include <QtCore/QSharedPointer>

#include "param.h"
#include "utils/common/id.h"
#include "core/resource/resource_fwd.h"

typedef QMap<QString, QString> ParamTypeMap;

class QnResourceType
{
public:
    QnResourceType();
    //QnResourceType(const QString& name);
    virtual ~QnResourceType();

    void setId(const QnUuid& value) { m_id = value; }
    const QnUuid& getId() const { return m_id;}

    void setParentId(const QnUuid &value);
    const QnUuid& getParentId() const { return m_parentId;}

    void setName(const QString& value) { m_name = value; }
    const QString& getName() const { return m_name;}

    void setManufacture(const QString& value) { m_manufacture = value; }
    const QString& getManufacture() const { return m_manufacture;}

    bool isCamera() const;

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

    mutable QnMutex m_allParamTypeListCacheMutex;
    mutable QSharedPointer<ParamTypeMap> m_allParamTypeListCache;

    mutable bool m_isCamera;
    mutable bool m_isCameraSet;
};

Q_DECLARE_METATYPE(QnResourceTypeList)

class QN_EXPORT QnResourceTypePool
{
public:
    typedef QMap<QnUuid, QnResourceTypePtr> QnResourceTypeMap;

    static const QString kLayoutTypeId;
    static const QString kServerTypeId;
    static const QString kVideoWallTypeId;
    static const QString kWebPageTypeId;
    static const QString kStorageTypeId;
    static const QString kUserTypeId;

    static const QnUuid kUserTypeUuid;
    static const QnUuid kServerTypeUuid;
    static const QnUuid kStorageTypeUuid;
    static const QnUuid kLayoutTypeUuid;
    static const QnUuid kDesktopCameraTypeUuid;
    static const QnUuid kWearableCameraTypeUuid;

    static QnResourceTypePool *instance();

    QnResourceTypePtr getResourceTypeByName(const QString& name) const;
    QnResourceTypePtr getResourceType(QnUuid id) const;
    void addResourceType(QnResourceTypePtr resourceType);
    void replaceResourceTypeList(const QnResourceTypeList& resourceType);

    /* exact match name */
    QnUuid getResourceTypeId(const QString& manufacture, const QString& name, bool showWarning = true) const;

    /* exact match name for fixed resourceTypes (Layout, Server, etc) */
    static QnUuid getFixedResourceTypeId(const QString& name);

    /* match name using like operation */
    QnUuid getLikeResourceTypeId(const QString& manufacture, const QString& name) const;

    QnResourceTypeMap getResourceTypeMap() const;

    bool isEmpty() const;

private:
    mutable QnMutex m_mutex;
    QnResourceTypeMap m_resourceTypeMap;
    mutable QnResourceTypePtr m_desktopCamResourceType;
};

#define qnResTypePool QnResourceTypePool::instance()

#endif // __RESOURCE_TYPE_H
