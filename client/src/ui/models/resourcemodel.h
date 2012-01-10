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
    QModelIndex indexFromResource(QnResourcePtr resource) const;
    QModelIndex indexFromResourceId(uint id) const; // ### remove; use use indexFromResource() instead
    inline QnResourcePtr resourceFromItem(QStandardItem *item) const
    { return resourceFromIndex(item->index()); }
    inline QStandardItem *itemFromResource(QnResourcePtr resource) const
    { return itemFromIndex(indexFromResource(resource)); }
    inline QStandardItem *itemFromResourceId(uint id) const
    { return itemFromIndex(indexFromResourceId(id)); }

    void addResource(QnResourcePtr resource);
    void removeResource(QnResourcePtr resource);

private Q_SLOTS:
    void _q_addResource(QnResourcePtr resource);
    void _q_removeResource(QnResourcePtr resource);

private:
    Q_DISABLE_COPY(ResourceModel)
};


#include <QtGui/QSortFilterProxyModel>

class ResourceSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ResourceSortFilterProxyModel(QObject *parent = 0);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;

private:
    void parseFilterString();

private:
    Q_DISABLE_COPY(ResourceSortFilterProxyModel)

    QString m_parsedFilterString;
    uint m_flagsFilter;

    enum FilterCategory {
        Text, Id, Name, Tags,
        NumFilterCategories
    };
    QList<QRegExp> m_filters[NumFilterCategories];
};

#endif // RESOURCEMODEL_H
