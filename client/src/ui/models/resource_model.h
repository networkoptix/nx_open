#ifndef RESOURCEMODEL_H
#define RESOURCEMODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtGui/QSortFilterProxyModel>
#include <core/resource/resource.h>

class QnResourceModelPrivate;

class QnResourceModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit QnResourceModel(QObject *parent = 0);
    virtual ~QnResourceModel();

    QnResourcePtr resource(const QModelIndex &index) const;
    QModelIndex index(const QnResourcePtr &resource) const;

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

    QStringList mimeTypes() const;
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    bool dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent);
    Qt::DropActions supportedDropActions() const;

public slots:
    void addResource(const QnResourcePtr &resource);
    void removeResource(const QnResourcePtr &resource);

private slots:
    void at_resPool_resourceAdded(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);
    void at_resPool_resourceChanged(const QnResourcePtr &resource);

private:
    friend class QnResourceSearchProxyModel;

    Q_DISABLE_COPY(QnResourceModel)
    Q_DECLARE_PRIVATE(QnResourceModel)
    
    const QScopedPointer<QnResourceModelPrivate> d_ptr;
};


#endif // RESOURCEMODEL_H
