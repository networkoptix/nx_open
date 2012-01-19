#ifndef RESOURCEMODEL_P_H
#define RESOURCEMODEL_P_H

#include "resourcemodel.h"

#ifndef USE_OLD_RESOURCEMODEL
#include <QtCore/QHash>
#include <QtCore/QTimer>
#include <QtCore/QVector>

class Node
{
    friend class ResourceModelPrivate;

public:
    inline Node() : m_id(0), m_parentId(0), m_flags(0), m_status(QnResource::Offline) {}

    inline bool operator==(const Node &other) const
    {
        return m_id == other.m_id && m_parentId == other.m_parentId && m_flags == other.m_flags
               && m_status == other.m_status && m_name == other.m_name/* && m_searchString == other.m_searchString*/;
    }

    inline uint id() const { return m_id; }
    inline uint parentId() const { return m_parentId; }
    inline quint32 flags() const { return m_flags; }
    inline QnResource::Status status() const { return m_status; }
    inline QString name() const { return m_name; }
    inline QString searchString() const { return m_searchString; }
    QIcon icon() const;

private:
    uint m_id;
    uint m_parentId;
    quint32 m_flags;
    QnResource::Status m_status;
    QString m_name;
    QString m_searchString;
};

Q_DECLARE_TYPEINFO(Node, Q_MOVABLE_TYPE);
#endif


class ResourceModelPrivate
{
    Q_DECLARE_PUBLIC(ResourceModel)

public:
    ResourceModelPrivate();
    ~ResourceModelPrivate();

    void init();

#ifdef USE_OLD_RESOURCEMODEL
    inline QStandardItem *item(const QnId &id) const
    { Q_Q(const ResourceModel); return q->itemFromIndex(index(id)); }
    inline QStandardItem *item(const QnResourcePtr &resource) const
    { Q_Q(const ResourceModel); return q->itemFromIndex(q->index(resource)); }
    inline QnResourcePtr resource(QStandardItem *item) const
    { Q_Q(const ResourceModel); return q->resource(item->index()); }
#else
    static Node *createNode(const QnResourcePtr &resource);

    Node *node(const QModelIndex &index) const;
    Node *node(const QnId &id) const;
    inline Node *node(const QnResourcePtr &resource) const
    { return node(resource->getId()); }
    QModelIndex index(Node *node, int column = 0) const;
#endif
    QModelIndex index(const QnId &id, int column = 0) const;
    inline QModelIndex index(const QnResourcePtr &resource, int column = 0) const
    { return index(resource->getId(), column); }

#ifndef USE_OLD_RESOURCEMODEL
    inline bool isIndexValid(const QModelIndex &index) const
    {
        Q_Q(const ResourceModel);
        return (index.isValid() && index.model() == q
                && index.row() < q->rowCount(index.parent())
                && index.column() < q->columnCount(index.parent()));
    }
#endif

    void _q_addResource(const QnResourcePtr &resource);
    void _q_removeResource(const QnResourcePtr &resource);
    void _q_resourceChanged(const QnResourcePtr &resource);

    ResourceModel *q_ptr;

#ifndef USE_OLD_RESOURCEMODEL
    Node root;
    QHash<Node *, QVector<Node *> > nodes;
#endif
};

#endif // RESOURCEMODEL_P_H
