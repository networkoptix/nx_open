#ifndef RESOURCEMODEL_P_H
#define RESOURCEMODEL_P_H

#include "resourcemodel.h"

class ResourceModelPrivate
{
    Q_DECLARE_PUBLIC(ResourceModel)

public:
    ResourceModelPrivate() : q_ptr(0) {}

    void init();

    inline QnResourcePtr resourceFromItem(QStandardItem *item) const
    {
        Q_Q(const ResourceModel);
        return q->resourceFromIndex(item->index());
    }
    inline QStandardItem *itemFromResource(const QnResourcePtr &resource) const
    {
        Q_Q(const ResourceModel);
        return q->itemFromIndex(q->indexFromResource(resource));
    }
    inline QStandardItem *itemFromResourceId(uint id) const
    {
        Q_Q(const ResourceModel);
        return q->itemFromIndex(q->indexFromResourceId(id));
    }

    inline bool isIndexValid(const QModelIndex &index) const
    {
        Q_Q(const ResourceModel);
        return (index.isValid() && index.model() == q
                && index.row() < q->rowCount(index.parent())
                && index.column() < q->columnCount(index.parent()));
    }

    void _q_addResource(const QnResourcePtr &resource);
    void _q_removeResource(const QnResourcePtr &resource);
    void _q_resourceChanged(const QnResourcePtr &resource);

    ResourceModel *q_ptr;
};

#endif // RESOURCEMODEL_P_H
