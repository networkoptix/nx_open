#ifndef __RESOURCE_TYPE_H
#define __RESOURCE_TYPE_H

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QSharedPointer>

#include "param.h"
#include "utils/common/id.h"

class QnResourceType;
typedef QSharedPointer<QnResourceType> QnResourceTypePtr;
typedef QList<QnResourceTypePtr> QnResourceTypeList;

Q_DECLARE_METATYPE(QnResourceTypeList)

class QN_EXPORT QnResourceType
{
public:
    QnResourceType();
    //QnResourceType(const QString& name);
    virtual ~QnResourceType();

    void setId(const QnId& value) { m_id = value; }
    QnId getId() const { return m_id;}

    void setParentId(const QnId &value);
    QnId getParentId() const { return m_parentId;}

    void setName(const QString& value) { m_name = value; }
    QString getName() const { return m_name;}

    void setManufacture(const QString& value) { m_manufacture = value; }
    QString getManufacture() const { return m_manufacture;}

    bool isCamera() const;

    void addAdditionalParent(QnId parent);
    QList<QnId> allParentList() const;

    void addParamType(QnParamTypePtr param);

    const QList<QnParamTypePtr>& paramTypeList() const;

private:
    QnId m_id;
    QnId m_parentId;
    QString m_name;
    QString m_manufacture;
    QList<QnId> m_additionalParentList;

    typedef QList<QnParamTypePtr> ParamTypeList;
    ParamTypeList m_paramTypeList;

    mutable QMutex m_allParamTypeListCacheMutex;
    mutable QSharedPointer<ParamTypeList> m_allParamTypeListCache;

    mutable bool m_isCamera;
    mutable bool m_isCameraSet;
};


class QN_EXPORT QnResourceTypePool
{
public:
    typedef QMap<QnId, QnResourceTypePtr> QnResourceTypeMap;

    static QnResourceTypePool *instance();

    QnResourceTypePtr getResourceTypeByName(const QString& name) const;
    QnResourceTypePtr getResourceType(QnId id) const;
    void addResourceType(QnResourceTypePtr resourceType);
    void addResourceTypeList(const QList<QnResourceTypePtr>& resourceType);
    void replaceResourceTypeList(const QList<QnResourceTypePtr>& resourceType);

    /* exact match name */
    QnId getResourceTypeId(const QString& manufacture, const QString& name, bool showWarning = true) const;

    /* match name using like operation */
    QnId getLikeResourceTypeId(const QString& manufacture, const QString& name) const;

    QnResourceTypeMap getResourceTypeMap() const;

    bool isEmpty() const;
private:
    mutable QMutex m_mutex;
    QnResourceTypeMap m_resourceTypeMap;
};

#define qnResTypePool QnResourceTypePool::instance()

#endif // __RESOURCE_TYPE_H
