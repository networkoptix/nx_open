#ifndef __RESOURCE_TYPE_H
#define __RESOURCE_TYPE_H

#include <QList>
#include "base/qnid.h"
#include "param.h"

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

    void addAdditionalParent(const QnId& parent);
    QList<QnId> allParentList() const;

    void addParamType(QnParamTypePtr param);

    const QList<QnParamTypePtr>& paramTypeList() const { return m_paramTypeList; }
private:
    QnId m_id;
    QnId m_parentId;
    QString m_name;
    QList<QnId> m_additionalParentList;
    QList<QnParamTypePtr> m_paramTypeList;
};

typedef QSharedPointer<QnResourceType> QnResourceTypePtr;

class QnResourceTypePool
{
public:
    static QnResourceTypePool* instance();

    QnResourceTypePtr getResourceType(const QnId& id);
    void addResourceType(QnResourceTypePtr resourceType);
private:
    QMutex m_mutex;
    typedef QMap<QnId, QnResourceTypePtr> QnResourceTypeMap;
    QnResourceTypeMap m_resourceTypeMap;
};

#define qnResTypePool QnResourceTypePool::instance()

#endif // __RESOURCE_TYPE_H
