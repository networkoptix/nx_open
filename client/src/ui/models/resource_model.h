#ifndef QN_RESOURCE_MODEL_H
#define QN_RESOURCE_MODEL_H

#include <QAbstractItemModel>
#include <QScopedPointer>
#include <core/resource/resource_fwd.h>

class QnResourceModelPrivate;
class QnResourcePool;

class QnResourceModel : public QAbstractItemModel {
    Q_OBJECT;
    Q_ENUMS(ItemDataRole);

public:
    enum ItemDataRole {
        ResourceRole        = Qt::UserRole + 1, /**< Role for QnResourcePtr. */
        IdRole              = Qt::UserRole + 2, /**< Role for resource's QnId. */
        SearchStringRole    = Qt::UserRole + 3, /**< Role for search string. */
        StatusRole          = Qt::UserRole + 4, /**< Role for resource's status. */
    };

    explicit QnResourceModel(QObject *parent = 0);
    virtual ~QnResourceModel();

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex buddy(const QModelIndex &index) const override;
    virtual QModelIndex parent(const QModelIndex &index) const override;
    virtual bool hasChildren(const QModelIndex &parent) const override;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    virtual QStringList mimeTypes() const override;
    virtual QMimeData *mimeData(const QModelIndexList &indexes) const override;
    virtual bool dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    virtual Qt::DropActions supportedDropActions() const override;

    void setResourcePool(QnResourcePool *resourcePool);
    QnResourcePool *resourcePool() const;

protected:
    void start();
    void stop();

private slots:
    void addResource(const QnResourcePtr &resource);
    void removeResource(const QnResourcePtr &resource);
    void at_resPool_resourceAdded(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);
    void at_resPool_resourceChanged(const QnResourcePtr &resource);

private:
    QnResourcePool *m_resourcePool;
    Node *m_root;
    QHash<QnId, Node *> m_nodeById;

    //Node *m_root;
    //QHash<uint, Node *> m_nodes;
    //QHash<Node *, QVector<Node *> > m_childrenByNode;
};


#endif // QN_RESOURCE_MODEL_H
