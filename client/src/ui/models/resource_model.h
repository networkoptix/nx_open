#ifndef QN_RESOURCE_MODEL_H
#define QN_RESOURCE_MODEL_H

#include <QAbstractItemModel>
#include <QScopedPointer>
#include <QUuid>
#include <core/resource/resource_fwd.h>
#include <utils/common/qnid.h>

class QnResourceModelPrivate;
class QnResourcePool;
class QnLayoutItemData;

class QnResourceModel : public QAbstractItemModel {
    Q_OBJECT;
    Q_ENUMS(ItemDataRole);

public:
    enum ItemDataRole {
        ResourceRole        = Qt::UserRole + 1, /**< Role for QnResourcePtr. */
        ResourceFlagsRole   = Qt::UserRole + 2, /**< Role for resource flags. */
        IdRole              = Qt::UserRole + 3, /**< Role for resource's QnId. */
        UuidRole            = Qt::UserRole + 4, /**< Role for layout item's UUID. */
        SearchStringRole    = Qt::UserRole + 5, /**< Role for search string. */
        StatusRole          = Qt::UserRole + 6, /**< Role for resource's status. */
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

    QnResourcePtr resource(const QModelIndex &index) const;

private:
    class Node;

    void start();
    void stop();

    Node *node(const QnId &id);
    Node *node(const QUuid &uuid);
    Node *node(const QModelIndex &index) const;

private slots:
    void at_resPool_resourceAdded(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);
    void at_resPool_aboutToBeDestroyed();
    void at_resource_parentIdChanged(const QnResourcePtr &resource);
    void at_resource_parentIdChanged();
    void at_resource_resourceChanged();
    void at_resource_itemAdded(const QnLayoutResourcePtr &layout, const QnLayoutItemData &item);
    void at_resource_itemAdded(const QnLayoutItemData &item);
    void at_resource_itemRemoved(const QnLayoutResourcePtr &layout, const QnLayoutItemData &item);
    void at_resource_itemRemoved(const QnLayoutItemData &item);

private:
    /** Associated resource pool. */
    QnResourcePool *m_resourcePool;

    /** Root node. Considered a resource node with id = 0.  */
    Node *m_root;

    /** Mapping for resource nodes, by resource id. */
    QHash<QnId, Node *> m_resourceNodeById;

    /** Mapping for item nodes, by item id. */
    QHash<QUuid, Node *> m_itemNodeByUuid;

    /** Mapping for item nodes, by resource id. Is managed by nodes. */
    QHash<QnId, QList<Node *> > m_itemNodesById;
};

Q_DECLARE_METATYPE(QUuid);


#endif // QN_RESOURCE_MODEL_H
