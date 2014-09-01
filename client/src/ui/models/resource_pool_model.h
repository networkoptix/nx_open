#ifndef QN_RESOURCE_POOL_MODEL_H
#define QN_RESOURCE_POOL_MODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QScopedPointer>
#include <QtCore/QUuid>

#include <utils/common/id.h>
#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>
#include <client/client_globals.h>

#include <utils/common/connective.h>

class QnResourceModelPrivate;
class QnResourcePool;
class QnLayoutItemData;
class QnVideoWallItem;
class QnVideoWallMatrix;
class QnWorkbenchContext;
class QnWorkbenchLayoutSnapshotManager;
class QnResourcePoolModelNode;

class QnResourcePoolModel : public Connective<QAbstractItemModel>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QAbstractItemModel> base_type;
public:
    /** Narrowed scope for the minor widgets and dialogs. */
    enum Scope {
        FullScope,
        CamerasScope,
        UsersScope
        //TODO: #GDM non-admin-scope?
    };

    explicit QnResourcePoolModel(Scope scope = FullScope, QObject *parent = NULL);
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
    virtual QHash<int,QByteArray> roleNames() const;

    virtual QStringList mimeTypes() const override;
    virtual QMimeData *mimeData(const QModelIndexList &indexes) const override;
    virtual bool dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    virtual Qt::DropActions supportedDropActions() const override;

    QnResourcePtr resource(const QModelIndex &index) const;

    bool isUrlsShown();
    void setUrlsShown(bool urlsShown);
private:
    QnResourcePoolModelNode *node(const QnResourcePtr &resource);
    QnResourcePoolModelNode *node(const QUuid &uuid);
    QnResourcePoolModelNode *node(const QModelIndex &index) const;
    QnResourcePoolModelNode *node(Qn::NodeType nodeType, const QUuid &uuid, const QnResourcePtr &resource);
    QnResourcePoolModelNode *node(const QnResourcePtr &resource, const QString &groupId, const QString &groupName);
    QnResourcePoolModelNode *systemNode(const QString &systemName);
    QnResourcePoolModelNode *expectedParent(QnResourcePoolModelNode *node);
    bool isIgnored(const QnResourcePtr &resource) const;

    void removeNode(QnResourcePoolModelNode *node);

    void removeNode(Qn::NodeType nodeType, const QUuid &uuid, const QnResourcePtr &resource);

    /** Some nodes can have deleting node set as parent, but this node will not
     ** have them as children because of their 'bastard' flag.*/
    void deleteNode(QnResourcePoolModelNode *node);
private slots:
    void at_resPool_resourceAdded(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);

    void at_context_userChanged();
    void at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &resource);
    void at_accessController_permissionsChanged(const QnResourcePtr &resource);

    void at_resource_parentIdChanged(const QnResourcePtr &resource);
    void at_resource_resourceChanged(const QnResourcePtr &resource);

    void at_layout_itemAdded(const QnLayoutResourcePtr &layout, const QnLayoutItemData &item);
    void at_layout_itemRemoved(const QnLayoutResourcePtr &layout, const QnLayoutItemData &item);

    void at_videoWall_itemAddedOrChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);
    void at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);

    void at_videoWall_matrixAddedOrChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallMatrix &matrix);
    void at_videoWall_matrixRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallMatrix &matrix);

    void at_camera_groupNameChanged(const QnSecurityCamResourcePtr &camera);

    void at_commonModule_systemNameChanged();

private:
    friend class QnResourcePoolModelNode;

    typedef QPair<QnResourcePtr, QUuid> NodeKey;

    typedef QHash<QString, QnResourcePoolModelNode *> RecorderHash;

    /** Root nodes array */
    QnResourcePoolModelNode *m_rootNodes[Qn::NodeTypeCount];

    /** Generic node list */
    QHash<NodeKey, QnResourcePoolModelNode*> m_nodes[Qn::NodeTypeCount];

    /** Set of top-level node types */
    QList<Qn::NodeType> m_rootNodeTypes;

    /** Mapping for resource nodes, by resource. */
    QHash<QnResourcePtr, QnResourcePoolModelNode *> m_resourceNodeByResource;

    /** Mapping for recorder nodes, by resource. */
    QHash<QnResourcePtr, RecorderHash> m_recorderHashByResource;

    /** Mapping for item nodes, by item id. */
    QHash<QUuid, QnResourcePoolModelNode *> m_itemNodeByUuid;

    /** Mapping for item nodes, by resource id. Is managed by nodes. */
    QHash<QnResourcePtr, QList<QnResourcePoolModelNode *> > m_itemNodesByResource;

    /** Mapping for system nodes, by system name. */
    QHash<QString, QnResourcePoolModelNode *> m_systemNodeBySystemName;

    /** Full list of all created nodes. */
    QList<QnResourcePoolModelNode *> m_allNodes;

    /** Whether item urls should be shown. */
    bool m_urlsShown;

    /** Narrowed scope for displaying the limited set of nodes. */
    Scope m_scope;
};

#endif // QN_RESOURCE_POOL_MODEL_H
