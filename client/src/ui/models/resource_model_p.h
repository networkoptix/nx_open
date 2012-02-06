#ifndef RESOURCEMODEL_P_H
#define RESOURCEMODEL_P_H

#include "resource_model.h"

#include <QtCore/QHash>
#include <QtCore/QVector>

class Node
{
public:
    inline Node() : m_id(0), m_parentId(0), m_flags(0), m_status(QnResource::Offline) {}
    Node(const QnResourcePtr &resource);

    inline bool operator==(const Node &other) const;
    inline bool operator!=(const Node &other) const
    { return !operator==(other); }

    void updateFromResource(const QnResourcePtr &resource);

    QnResourcePtr resource() const;

    inline uint id() const { return m_id; }
    inline uint parentId() const { return m_parentId; }
    inline quint32 flags() const { return m_flags; }
    inline QnResource::Status status() const { return m_status; }
    inline QString name() const { return m_name; }
    inline QString searchString() const { return m_searchString; }

    QIcon icon() const;

private:
    Q_DISABLE_COPY(Node)

    uint m_id;
    uint m_parentId;
    quint32 m_flags;
    QnResource::Status m_status;
    QString m_name;
    QString m_searchString;
};

Q_DECLARE_TYPEINFO(Node, Q_MOVABLE_TYPE);

bool Node::operator==(const Node &other) const
{
    return m_id == other.m_id && m_parentId == other.m_parentId && m_flags == other.m_flags
           && m_status == other.m_status && m_name == other.m_name/* && m_searchString == other.m_searchString*/;
}


class QnResourceModelPrivate
{
    Q_DECLARE_PUBLIC(QnResourceModel)

public:
    QnResourceModelPrivate();
    ~QnResourceModelPrivate();

    void init();

    void insertNode(Node *parentNode, Node *node, int row);
    void removeNodes(Node *parentNode, int row, int count);

    Node *node(const QModelIndex &index) const;
    Node *node(QnId id) const;
    inline Node *node(const QnResourcePtr &resource) const
    { return node(resource->getId()); }
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QModelIndex index(Node *node, int column = 0) const;
    QModelIndex index(QnId id, int column = 0) const;
    inline QModelIndex index(const QnResourcePtr &resource, int column = 0) const
    { return index(resource->getId(), column); }

    inline bool isIndexValid(const QModelIndex &index) const
    {
        Q_Q(const QnResourceModel);
        return (index.isValid() && index.model() == q
                && index.row() < q->rowCount(index.parent())
                && index.column() < q->columnCount(index.parent()));
    }

    void at_resPool_resourceAdded(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);
    void at_resPool_resourceChanged(const QnResourcePtr &resource);

private:
    QnResourceModel *q_ptr;

    Node root;
    QHash<uint, Node *> nodes;
    QHash<Node *, QVector<Node *> > nodeTree;
};


#endif // RESOURCEMODEL_P_H
