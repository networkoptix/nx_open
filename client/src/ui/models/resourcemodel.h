#ifndef RESOURCEMODEL_H
#define RESOURCEMODEL_H

//#define USE_OLD_RESOURCEMODEL

#include <QtCore/QAbstractItemModel>

#include "core/resource/resource.h"

#ifdef USE_OLD_RESOURCEMODEL
#include <QtGui/QStandardItemModel>

class ResourceModelPrivate;
class ResourceModel : public QStandardItemModel
#else
class ResourceModelPrivate;
class ResourceModel : public QAbstractItemModel
#endif
{
    Q_OBJECT

public:
    explicit ResourceModel(QObject *parent = 0);
    virtual ~ResourceModel();

    QnResourcePtr resource(const QModelIndex &index) const;
    QModelIndex index(const QnResourcePtr &resource) const;

#ifndef USE_OLD_RESOURCEMODEL
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex buddy(const QModelIndex &index) const;
    QModelIndex parent(const QModelIndex &index) const;
    bool hasChildren(const QModelIndex &parent) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
#endif

    QStringList mimeTypes() const;
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    bool dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent);
    Qt::DropActions supportedDropActions() const;

public Q_SLOTS:
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

class ResourceSortFilterProxyModelPrivate;
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
    Q_DISABLE_COPY(ResourceSortFilterProxyModel)
    Q_DECLARE_PRIVATE(ResourceSortFilterProxyModel)
    const QScopedPointer<ResourceSortFilterProxyModelPrivate> d_ptr;
};

#endif // RESOURCEMODEL_H
