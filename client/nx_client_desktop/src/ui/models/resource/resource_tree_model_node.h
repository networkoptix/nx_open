#pragma once

#include <nx/utils/uuid.h>

#include <common/common_globals.h>

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

#include <ui/models/resource/resource_tree_model_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/from_this_to_shared.h>
#include <utils/common/connective.h>

class QnResourceTreeModelNode:
    public Connective<QObject>,
    public QnWorkbenchContextAware,
    public QnFromThisToShared<QnResourceTreeModelNode>
{
    Q_OBJECT

    using base_type = Connective<QObject>;
public:
    enum State
    {
        Normal,     /**< Normal node. */
        Invalid     /**< Invalid node that should not be displayed.
                     * Invalid nodes are parts of dangling tree branches during incremental
                     * tree construction. They do not emit model signals. */
    };

    /**
     * Constructor for root nodes.
     */
    QnResourceTreeModelNode(QnResourceTreeModel* model, Qn::NodeType type);

    /**
    * Constructor for other virtual group nodes (recorders, other systems).
    */
    QnResourceTreeModelNode(QnResourceTreeModel* model, Qn::NodeType type, const QString &name);

    /**
     * Constructor for resource nodes.
     */
    QnResourceTreeModelNode(QnResourceTreeModel* model, const QnResourcePtr &resource, Qn::NodeType nodeType);

    /**
     * Constructor for item nodes.
     */
    QnResourceTreeModelNode(QnResourceTreeModel* model, const QnUuid &uuid, Qn::NodeType nodeType);

    virtual ~QnResourceTreeModelNode();

    virtual void setResource(const QnResourcePtr &resource);
    virtual void setParent(const QnResourceTreeModelNodePtr& parent);
    virtual void updateRecursive();

    /** Setup node. Called when its shared pointer is ready. */
    virtual void initialize();

    /** Cleanup node to make sure it can be destroyed safely. */
    virtual void deinitialize();

    void update();

    Qn::NodeType type() const ;
    QnResourcePtr resource() const;
    Qn::ResourceFlags resourceFlags() const;
    QnUuid uuid() const;

    QList<QnResourceTreeModelNodePtr> children() const;
    QList<QnResourceTreeModelNodePtr> childrenRecursive() const;

    QnResourceTreeModelNodePtr child(int index) ;

    QnResourceTreeModelNodePtr parent() const ;

    QModelIndex createIndex(int row, int col);
    QModelIndex createIndex(int col);

    virtual Qt::ItemFlags flags(int column) const;

    virtual QVariant data(int role, int column) const;

    bool setData(const QVariant &value, int role, int column) ;

    bool isModified() const;

    void setModified(bool modified) ;

    bool isPrimary() const;

protected:
    QnResourceTreeModelNode(QnResourceTreeModel* model, Qn::NodeType type, const QnUuid& uuid);

    void setName(const QString& name);
    void setIcon(const QIcon& icon);

    bool isInitialized() const;

    QnResourceTreeModel* model() const;

    virtual QIcon calculateIcon() const;

    virtual void addChildInternal(const QnResourceTreeModelNodePtr& child);
    virtual void removeChildInternal(const QnResourceTreeModelNodePtr& child);
    void changeInternal();

    void updateResourceStatus();

    virtual QnResourceTreeModelNodeManager* manager() const;

private:
    void setNameInternal(const QString& name);

    bool isValid() const;
    State state() const;
    void setState(State state);

    /** Recalculated 'bastard' state for the node. */
    bool calculateBastard() const;

    bool isBastard() const;
    void setBastard(bool bastard);

    int helpTopicId() const;

    bool changeCheckStateRecursivelyUp(Qt::CheckState newState);
    void childCheckStateChanged(Qt::CheckState oldState, Qt::CheckState newState, bool forceUpdate = false);
    void propagateCheckStateRecursivelyDown();

    void handlePermissionsChanged();

private:
    // TODO: #GDM #vkutin need complete recorder nodes structure refactor to get rid of this shit
    friend class QnResourceTreeModel;
    friend class QnResourceTreeModelNodeManager;

    /* Node state. */
    bool m_initialized;

    /** Model that this node belongs to. */
    QPointer<QnResourceTreeModel> const m_model;

    /** Type of this node. */
    const Qn::NodeType m_type;

    /** Uuid that this node represents. */
    const QnUuid m_uuid;

    /** Resource associated with this node. */
    QnResourcePtr m_resource;

    /** State of this node. */
    State m_state;

    /** Whether this node is a bastard node. Bastard nodes do not appear in
     * their parent's children list and do not inherit their parent's state. */
    bool m_bastard;

    /** Parent of this node. */
    QnResourceTreeModelNodePtr m_parent;

    /** Children of this node. */
    QList<QnResourceTreeModelNodePtr> m_children;

    /* Resource-related state. */

    /** Resource flags. */
    Qn::ResourceFlags m_flags;

    /** Display name of this node */
    QString m_displayName;

    /** Name of this node. */
    QString m_name;

    /** Status of this node. */
    Qn::ResourceStatus m_status;

    /** Search string of this node. */
    QString m_searchString;

    /** Icon of this node. */
    QIcon m_icon;

    /** Whether this resource is modified. */
    bool m_modified;

    /** Whether this resource is checked. */
    Qt::CheckState m_checkState;

    /** Number of unchecked and checked children. */
    int m_uncheckedChildren;
    int m_checkedChildren;

    // TODO: #GDM #Common implement cache invalidating in case of permissions change
    /** Whether this resource can be renamed, cached value. */
    mutable struct {
        bool value;
        bool checked;
    } m_editable;

    /* Nodes of the same resource are organized into double-linked list: */
    QnResourceTreeModelNodePtr m_prev;
    QnResourceTreeModelNodePtr m_next;
};

QDebug operator<<(QDebug dbg, QnResourceTreeModelNode* node);
QDebug operator<<(QDebug dbg, const QnResourceTreeModelNodePtr& node);
