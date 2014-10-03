#ifndef __RESOURCE_TYPE_H
#define __RESOURCE_TYPE_H

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QSharedPointer>

#include "param.h"
#include "utils/common/id.h"

typedef QMap<QString, QnParamTypePtr> ParamTypeMap;

class QN_EXPORT QnResourceType
{
public:
    QnResourceType();
    //QnResourceType(const QString& name);
    virtual ~QnResourceType();

    void setId(const QnUuid& value) { m_id = value; }
    QnUuid getId() const { return m_id;}

    void setParentId(const QnUuid &value);
    QnUuid getParentId() const { return m_parentId;}

    void setName(const QString& value) { m_name = value; }
    QString getName() const { return m_name;}

    void setManufacture(const QString& value) { m_manufacture = value; }
    QString getManufacture() const { return m_manufacture;}

    bool isCamera() const;

    void addAdditionalParent(QnUuid parent);
    QList<QnUuid> allParentList() const;

    void addParamType(QnParamTypePtr param);
    bool hasParam(const QString& name) const;

    const ParamTypeMap& paramTypeList() const;

private:
    QnUuid m_id;
    QnUuid m_parentId;
    QString m_name;
    QString m_manufacture;
    QList<QnUuid> m_additionalParentList;

    ParamTypeMap m_paramTypeList;

    mutable QMutex m_allParamTypeListCacheMutex;
    mutable QSharedPointer<ParamTypeMap> m_allParamTypeListCache;

    mutable bool m_isCamera;
    mutable bool m_isCameraSet;
};

Q_DECLARE_METATYPE(QnResourceTypeList)

class QN_EXPORT QnResourceTypePool
{
public:
    typedef QMap<QnUuid, QnResourceTypePtr> QnResourceTypeMap;

    static QnResourceTypePool *instance();

    QnResourceTypePtr getResourceTypeByName(const QString& name) const;
    QnResourceTypePtr getResourceType(QnUuid id) const;
    void addResourceType(QnResourceTypePtr resourceType);
    void replaceResourceTypeList(const QnResourceTypeList& resourceType);

    /* exact match name */
    QnUuid getResourceTypeId(const QString& manufacture, const QString& name, bool showWarning = true) const;

    /* exact match name for fixed resourceTypes (Layout, Server, etc) */
    QnUuid getFixedResourceTypeId(const QString& name) const;

    /* match name using like operation */
    QnUuid getLikeResourceTypeId(const QString& manufacture, const QString& name) const;

    QnResourceTypeMap getResourceTypeMap() const;

    bool isEmpty() const;

    QnResourceTypePtr desktopCameraResourceType() const;
private:
    mutable QMutex m_mutex;
    QnResourceTypeMap m_resourceTypeMap;
};

#define qnResTypePool QnResourceTypePool::instance()

#endif // __RESOURCE_TYPE_H
