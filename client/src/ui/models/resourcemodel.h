#ifndef RESOURCEMODEL_H
#define RESOURCEMODEL_H

#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"

class ResourceModelPrivate;
class ResourceModel : public QStandardItemModel
{
    Q_OBJECT

public:
    explicit ResourceModel(QObject *parent = 0);
    ~ResourceModel();

    QStringList mimeTypes() const;
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    bool dropMimeData (const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);

    QnResourcePtr resourceFromIndex(const QModelIndex &index) const;
    QModelIndex indexFromResource(const QnResourcePtr &resource) const;
    QModelIndex indexFromResourceId(uint id) const; // ### remove; use use indexFromResource() instead

protected Q_SLOTS:
    void addResource(const QnResourcePtr &resource);
    void removeResource(const QnResourcePtr &resource);

private:
    Q_DISABLE_COPY(ResourceModel)
    Q_DECLARE_PRIVATE(ResourceModel)
    const QScopedPointer<ResourceModelPrivate> d_ptr;
    Q_PRIVATE_SLOT(d_func(), void _q_addResource(const QnResourcePtr &resource))
    Q_PRIVATE_SLOT(d_func(), void _q_removeResource(const QnResourcePtr &resource))
    Q_PRIVATE_SLOT(d_func(), void _q_resourceChanged(const QnResourcePtr &resource))
};


#include <QtGui/QSortFilterProxyModel>

class ResourceSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ResourceSortFilterProxyModel(QObject *parent = 0);

    QnResourcePtr resourceFromIndex(const QModelIndex &index) const;
    QModelIndex indexFromResource(const QnResourcePtr &resource) const;

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;

private:
    bool matchesFilters(const QRegExp filters[], const QnResourcePtr &resource,
                        QAbstractItemModel *sourceModel, int source_row, const QModelIndex &source_parent) const;
    void parseFilterString();
    void buildFilters(const QSet<QString> parts[], QRegExp *filters);

private:
    Q_DISABLE_COPY(ResourceSortFilterProxyModel)

    QString m_parsedFilterString;
    uint m_flagsFilter;

    enum FilterCategory {
        Text, Name, Tags, Id,
        NumFilterCategories
    };
    QRegExp m_negfilters[NumFilterCategories];
    QRegExp m_filters[NumFilterCategories];
};

#endif // RESOURCEMODEL_H
