#ifndef RESOURCEMODEL_H
#define RESOURCEMODEL_H

#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"

class ResourceModel : public QStandardItemModel
{
    Q_OBJECT

public:
    explicit ResourceModel(QObject *parent = 0);
    ~ResourceModel();

    QStringList mimeTypes() const;
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    bool dropMimeData (const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);

protected:
    QnResourcePtr resourceFromIndex(const QModelIndex &index) const;
    QnResourcePtr resourceFromItem(QStandardItem *item) const;
    QStandardItem *itemFromResource(QnResourcePtr resource) const;
    QStandardItem *itemFromResourceId(uint id) const;

    void addResource(QnResourcePtr resource);
    void removeResource(QnResourcePtr resource);

private Q_SLOTS:
    void _q_addResource(QnResourcePtr resource);
    void _q_removeResource(QnResourcePtr resource);
};

#endif // RESOURCEMODEL_H
