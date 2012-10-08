#ifndef QN_RESOURCE_POOL_MODEL_H
#define QN_RESOURCE_POOL_MODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QScopedPointer>
#include <QtCore/QUuid>

#include <utils/common/qnid.h>
#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnResourceModelPrivate;
class QnResourcePool;
class QnLayoutItemData;
class QnWorkbenchContext;
class QnWorkbenchLayoutSnapshotManager;

class QnResourcePoolModel : public QAbstractItemModel, public QnWorkbenchContextAware {
    Q_OBJECT;

public:
    explicit QnResourcePoolModel(QObject *parent = 0);
    virtual ~QnResourcePoolModel();

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

    QnResourcePtr resource(const QModelIndex &index) const;

    bool isUrlsShown();
    void setUrlsShown(bool urlsShown);

private:
    class Node;

    void start();
    void stop();

    Node *node(const QnResourcePtr &resource);
    Node *node(const QUuid &uuid);
    Node *node(const QModelIndex &index) const;
    Node *expectedParent(Node *node);
    bool isIgnored(const QnResourcePtr &resource) const;

private slots:
    void at_resPool_resourceAdded(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);

    void at_context_userChanged();
    void at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &resource);
    void at_accessController_permissionsChanged(const QnResourcePtr &resource);

    void at_resource_parentIdChanged(const QnResourcePtr &resource);
    void at_resource_parentIdChanged();
    void at_resource_resourceChanged(const QnResourcePtr &resource);
    void at_resource_resourceChanged();
    void at_resource_itemAdded(const QnLayoutResourcePtr &layout, const QnLayoutItemData &item);
    void at_resource_itemAdded(const QnLayoutItemData &item);
    void at_resource_itemRemoved(const QnLayoutResourcePtr &layout, const QnLayoutItemData &item);
    void at_resource_itemRemoved(const QnLayoutItemData &item);

private:
    /** Root node. */
    Node *m_rootNode;

    Node *m_serversNode, *m_localNode, *m_usersNode;

    /** Mapping for resource nodes, by resource. */
    QHash<QnResource *, Node *> m_resourceNodeByResource;

    /** Mapping for item nodes, by item id. */
    QHash<QUuid, Node *> m_itemNodeByUuid;

    /** Mapping for item nodes, by resource id. Is managed by nodes. */
    QHash<QnResource *, QList<Node *> > m_itemNodesByResource;

    /** Whether item urls should be shown. */
    bool m_urlsShown;
};

#endif // QN_RESOURCE_POOL_MODEL_H
