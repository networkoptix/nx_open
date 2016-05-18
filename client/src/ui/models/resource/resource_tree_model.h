#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QScopedPointer>

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/id.h>
#include <utils/common/connective.h>

class QnResourceModelPrivate;
class QnResourcePool;
class QnLayoutItemData;
class QnVideoWallItem;
class QnVideoWallMatrix;
class QnWorkbenchContext;
class QnWorkbenchLayoutSnapshotManager;
class QnResourceTreeModelNode;
class QnResourceTreeModelCustomColumnDelegate;

class QnResourceTreeModel : public Connective<QAbstractItemModel>, public QnWorkbenchContextAware 
{
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

    explicit QnResourceTreeModel(Scope scope = FullScope, QObject *parent = NULL);
    virtual ~QnResourceTreeModel();

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

    QnResourceTreeModelCustomColumnDelegate* customColumnDelegate() const;
    void setCustomColumnDelegate(QnResourceTreeModelCustomColumnDelegate *columnDelegate);
private:
    QnResourceTreeModelNode *node(const QnResourcePtr &resource);
    QnResourceTreeModelNode *node(const QnUuid &uuid);
    QnResourceTreeModelNode *node(const QModelIndex &index) const;
    QnResourceTreeModelNode *node(Qn::NodeType nodeType, const QnUuid &uuid, const QnResourcePtr &resource);
    QnResourceTreeModelNode *node(const QnResourcePtr &resource, const QString &groupId, const QString &groupName);
    QnResourceTreeModelNode *systemNode(const QString &systemName);
    QnResourceTreeModelNode *expectedParent(QnResourceTreeModelNode *node);
    bool isIgnored(const QnResourcePtr &resource) const;

    void removeNode(QnResourceTreeModelNode *node);

    void removeNode(Qn::NodeType nodeType, const QnUuid &uuid, const QnResourcePtr &resource);

    /** Some nodes can have deleting node set as parent, but this node will not
     ** have them as children because of their 'bastard' flag.*/
    void deleteNode(QnResourceTreeModelNode *node);

    /** Fully rebuild resources tree. */
    void rebuildTree();

private slots:
    void at_resPool_resourceAdded(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);

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

    void at_camera_groupNameChanged(const QnResourcePtr &resource);

    void at_server_systemNameChanged(const QnResourcePtr &resource);
    void at_server_redundancyChanged(const QnResourcePtr &resource);
    void at_commonModule_systemNameChanged();

    void at_user_enabledChanged(const QnResourcePtr &resource);

    void at_serverAutoDiscoveryEnabledChanged();

private:
    friend class QnResourceTreeModelNode;

    typedef QPair<QnResourcePtr, QnUuid> NodeKey;

    typedef QHash<QString, QnResourceTreeModelNode *> RecorderHash;

    /** Root nodes array */
    QnResourceTreeModelNode *m_rootNodes[Qn::NodeTypeCount];

    /** Generic node list */
    QHash<NodeKey, QnResourceTreeModelNode*> m_nodes[Qn::NodeTypeCount];

    /** Set of top-level node types */
    QList<Qn::NodeType> m_rootNodeTypes;

    /** Mapping for resource nodes, by resource. */
    QHash<QnResourcePtr, QnResourceTreeModelNode *> m_resourceNodeByResource;

    /** Mapping for recorder nodes, by resource. */
    QHash<QnResourcePtr, RecorderHash> m_recorderHashByResource;

    /** Mapping for item nodes, by item id. */
    QHash<QnUuid, QnResourceTreeModelNode *> m_itemNodeByUuid;

    /** Mapping for item nodes, by resource id. Is managed by nodes. */
    QHash<QnResourcePtr, QList<QnResourceTreeModelNode *> > m_itemNodesByResource;

    /** Mapping for system nodes, by system name. */
    QHash<QString, QnResourceTreeModelNode *> m_systemNodeBySystemName;

    /** Full list of all created nodes. */
    QList<QnResourceTreeModelNode *> m_allNodes;

    /** Delegate for custom column data. */
    QPointer<QnResourceTreeModelCustomColumnDelegate> m_customColumnDelegate;

    /** Whether item urls should be shown. */
    bool m_urlsShown;

    /** Narrowed scope for displaying the limited set of nodes. */
    Scope m_scope;
};
