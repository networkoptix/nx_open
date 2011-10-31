#ifndef __RESOURCE_TYPE_H
#define __RESOURCE_TYPE_H

#include <QList>
#include "param.h"
#include "utils/common/qnid.h"

class QN_EXPORT QnResourceType
{
public:
    QnResourceType();
    //QnResourceType(const QString& name);
    virtual ~QnResourceType();

    void setId(const QnId& value) { m_id = value; }
    QnId getId() const { return m_id;}

    void setParentId(const QnId& value) { m_parentId = value; }
    QnId getParentId() const { return m_parentId;}

    void setName(const QString& value) { m_name = value; }
    QString getName() const { return m_name;}

    void setManufacture(const QString& value) { m_manufacture = value; }
    QString getManufacture() const { return m_manufacture;}


    void addAdditionalParent(const QnId& parent);
    QList<QnId> allParentList() const;

    void addParamType(QnParamTypePtr param);

    const QList<QnParamTypePtr>& paramTypeList() const { return m_paramTypeList; }
private:
    QnId m_id;
    QnId m_parentId;
    QString m_name;
    QString m_manufacture;
    QList<QnId> m_additionalParentList;
    QList<QnParamTypePtr> m_paramTypeList;
};

typedef QSharedPointer<QnResourceType> QnResourceTypePtr;

class QN_EXPORT QnResourceTypePool
{
public:
    static QnResourceTypePool* instance();

    QnResourceTypePtr getResourceType(const QnId& id) const;
    void addResourceType(QnResourceTypePtr resourceType);

    QnId getResourceTypeId(const QString& manufacture, const QString& name) const;

private:
    mutable QMutex m_mutex;
    typedef QMap<QnId, QnResourceTypePtr> QnResourceTypeMap;
    QnResourceTypeMap m_resourceTypeMap;
};

#define qnResTypePool QnResourceTypePool::instance()

#endif // __RESOURCE_TYPE_H
