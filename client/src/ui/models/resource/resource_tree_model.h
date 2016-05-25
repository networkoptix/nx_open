#pragma once

#include <array>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QScopedPointer>

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

#include <ui/models/resource/resource_tree_model_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/id.h>
#include <utils/common/connective.h>

class QnResourceModelPrivate;
class QnResourcePool;
class QnVideoWallItem;
class QnVideoWallMatrix;
class QnWorkbenchContext;
class QnWorkbenchLayoutSnapshotManager;

class QnResourceTreeModelCustomColumnDelegate;

class QnResourceTreeModel : public Connective<QAbstractItemModel>, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QAbstractItemModel> base_type;
public:
    /** Narrowed scope for the minor widgets and dialogs. */
    enum Scope
    {
        FullScope,
        CamerasScope,
        UsersScope
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
    QnResourceTreeModelNodePtr node(const QModelIndex& index) const;

    QnResourceTreeModelNodePtr ensureResourceNode(const QnResourcePtr& resource);
    QnResourceTreeModelNodePtr ensureItemNode(const QnResourceTreeModelNodePtr& parentNode, const QnUuid& uuid, Qn::NodeType nodeType = Qn::LayoutItemNode);
    QnResourceTreeModelNodePtr ensureRecorderNode(const QnResourceTreeModelNodePtr& parentNode, const QString &groupId, const QString &groupName);
    QnResourceTreeModelNodePtr ensureSystemNode(const QString &systemName);
    QnResourceTreeModelNodePtr ensureSharedLayoutNode(const QnResourceTreeModelNodePtr& parentNode, const QnResourcePtr& sharedLayout);

    QnResourceTreeModelNodePtr expectedParent(const QnResourceTreeModelNodePtr& node);
    QnResourceTreeModelNodePtr expectedParentForResourceNode(const QnResourceTreeModelNodePtr& node);

    void updateNodeParent(const QnResourceTreeModelNodePtr& node);
    void updateNodeResource(const QnResourceTreeModelNodePtr& node, const QnResourcePtr& resource);

    void updateSharedLayoutNodes(const QnLayoutResourcePtr& layout);

    /* Remove virtual 'system' nodes that are not used anymore. */
    void cleanupSystemNodes();

    /** Cleanup all node references. */
    void removeNode(const QnResourceTreeModelNodePtr& node);

    /** Fully rebuild resources tree. */
    void rebuildTree();

    /**
     * @brief                       Handle drag-n-drop result.
     * @param sourceResources       What is dragged
     * @param targetResource        On what drop was done.
     * @param mimeData              Full drag-n-drop data.
     */
    void handleDrop(const QnResourceList& sourceResources, const QnResourcePtr& targetResource, const QMimeData *mimeData);
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

    typedef QHash<QString, QnResourceTreeModelNodePtr> RecorderHash;
    typedef QHash<QnUuid, QnResourceTreeModelNodePtr> ItemHash;
    typedef QHash<QnResourcePtr, QnResourceTreeModelNodePtr> ResourceHash;
    typedef QList<QnResourceTreeModelNodePtr> NodeList;

    /** Root nodes array */
    std::array<QnResourceTreeModelNodePtr, Qn::NodeTypeCount> m_rootNodes;

    /** Mapping for resource nodes by resource. */
    QHash<QnResourcePtr, QnResourceTreeModelNodePtr> m_resourceNodeByResource;

    /** Mapping for recorder nodes by parent node. */
    QHash<QnResourceTreeModelNodePtr, RecorderHash> m_recorderHashByParent;

    /** Mapping for item nodes by parent node. */
    QHash<QnResourceTreeModelNodePtr, ItemHash> m_itemNodesByParent;

    /** Mapping for all nodes by resource (for quick update). */
    QHash<QnResourcePtr, NodeList> m_nodesByResource;

    /** Mapping for system nodes, by system name. */
    QHash<QString, QnResourceTreeModelNodePtr> m_systemNodeBySystemName;

    /** Mapping of shared layouts links by parent node (user's) */
    QHash<QnResourceTreeModelNodePtr, ResourceHash> m_sharedLayoutNodesByParent;

    /** Full list of all created nodes. */
    QList<QnResourceTreeModelNodePtr> m_allNodes;

    /** Delegate for custom column data. */
    QPointer<QnResourceTreeModelCustomColumnDelegate> m_customColumnDelegate;

    /** Whether item urls should be shown. */
    bool m_urlsShown;

    /** Narrowed scope for displaying the limited set of nodes. */
    const Scope m_scope;
};
