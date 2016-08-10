#pragma once

#include <nx/utils/uuid.h>

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

#include <ui/models/resource/resource_tree_model_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/from_this_to_shared.h>

class QnResourceTreeModelNode: public QObject, public QnWorkbenchContextAware, public QnFromThisToShared<QnResourceTreeModelNode>
{
    Q_OBJECT

    typedef QObject base_type;
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
    QnResourceTreeModelNode(QnResourceTreeModel* model, const QnUuid &uuid, Qn::NodeType nodeType = Qn::LayoutItemNode);

    ~QnResourceTreeModelNode();

    void setResource(const QnResourcePtr &resource);

    void update();

    void updateRecursive() ;

    Qn::NodeType type() const ;
    QnResourcePtr resource() const;
    Qn::ResourceFlags resourceFlags() const;

    QList<QnResourceTreeModelNodePtr> children() const;
    QList<QnResourceTreeModelNodePtr> childrenRecursive() const;

    QnResourceTreeModelNodePtr child(int index) ;

    QnResourceTreeModelNodePtr parent() const ;

    void setParent(const QnResourceTreeModelNodePtr& parent) ;

    QModelIndex createIndex(int row, int col);
    QModelIndex createIndex(int col);

    Qt::ItemFlags flags(int column) const ;

    QVariant data(int role, int column) const ;

    bool setData(const QVariant &value, int role, int column) ;

    bool isModified() const;

    void setModified(bool modified) ;

protected:
    void removeChildInternal(const QnResourceTreeModelNodePtr& child) ;
    void addChildInternal(const QnResourceTreeModelNodePtr& child);
    void changeInternal();

private:
    QnResourceTreeModelNode(QnResourceTreeModel* model, Qn::NodeType type, const QnUuid& uuid);

    const QnUuid &uuid() const;
    bool isValid() const;
    State state() const;
    void setState(State state);

    /** Recalculated 'bastard' state for the node. */
    bool calculateBastard() const;

    bool isBastard() const;
    void setBastard(bool bastard);

private:
    //TODO: #GDM #Common need complete recorder nodes structure refactor to get rid of this shit
    friend class QnResourceTreeModel;

    /* Node state. */

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

    //TODO: #GDM #Common implement cache invalidating in case of permissions change
    /** Whether this resource can be renamed, cached value. */
    mutable struct {
        bool value;
        bool checked;
    } m_editable;
};


