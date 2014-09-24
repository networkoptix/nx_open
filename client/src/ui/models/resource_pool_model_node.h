#ifndef RESOURCE_POOL_MODEL_NODE_H
#define RESOURCE_POOL_MODEL_NODE_H

#include <QtCore/QCoreApplication> /* For Q_DECLARE_TR_FUNCTIONS. */
#include <QtCore/QString>
#include <QtCore/QUuid>

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

//#TODO: #GDM #Common include resource.h
#include <core/resource/resource.h>

class QnResourcePoolModel;

class QnResourcePoolModelNode {
    Q_DECLARE_TR_FUNCTIONS(QnResourcePoolModelNode)
public:
    enum State {
        Normal,     /**< Normal node. */
        Invalid     /**< Invalid node that should not be displayed.
                     * Invalid nodes are parts of dangling tree branches during incremental
                     * tree construction. They do not emit model signals. */
    };

    /**
     * Constructor for toplevel nodes.
     */
    QnResourcePoolModelNode(QnResourcePoolModel *model, Qn::NodeType type, const QString &name = QString());

    /**
     * Constructor for resource nodes.
     */
    QnResourcePoolModelNode(QnResourcePoolModel *model, const QnResourcePtr &resource, Qn::NodeType nodeType);

    /**
     * Constructor for item nodes.
     */
    QnResourcePoolModelNode(QnResourcePoolModel *model, const QUuid &uuid, Qn::NodeType nodeType = Qn::ItemNode);

    ~QnResourcePoolModelNode();

    void clear() ;

    void setResource(const QnResourcePtr &resource);

    void update();

    void updateRecursive() ;

    Qn::NodeType type() const ;

    const QnResourcePtr &resource() const;

    Qn::ResourceFlags resourceFlags() const;

    Qn::ResourceStatus resourceStatus() const ;

    const QUuid &uuid() const ;

    State state() const ;

    bool isValid() const;

    void setState(State state) ;

    bool isBastard() const ;

    void setBastard(bool bastard);


    const QList<QnResourcePoolModelNode *> &children() const;

    QnResourcePoolModelNode *child(int index) ;

    QnResourcePoolModelNode *parent() const ;

    void setParent(QnResourcePoolModelNode *parent) ;

    QModelIndex index(int col);

    QModelIndex index(int row, int col);

    Qt::ItemFlags flags(int column) const ;

    QVariant data(int role, int column) const ;

    bool setData(const QVariant &value, int role, int column) ;

    bool isModified() const;

    void setModified(bool modified) ;
protected:
    void removeChildInternal(QnResourcePoolModelNode *child) ;
    void addChildInternal(QnResourcePoolModelNode *child);
    void changeInternal();

private:
    /** Recalculated 'bastard' state for the node. */
    bool calculateBastard() const;

private:
    //TODO: #GDM #Common need complete recorder nodes structure refactor to get rid of this shit
    friend class QnResourcePoolModel;

    /* Node state. */

    /** Model that this node belongs to. */
    QnResourcePoolModel *const m_model;

    /** Type of this node. */
    const Qn::NodeType m_type;

    /** Uuid that this node represents. */
    const QUuid m_uuid;

    /** Resource associated with this node. */
    QnResourcePtr m_resource;

    /** State of this node. */
    State m_state;

    /** Whether this node is a bastard node. Bastard nodes do not appear in
     * their parent's children list and do not inherit their parent's state. */
    bool m_bastard;

    /** Parent of this node. */
    QnResourcePoolModelNode *m_parent;

    /** Children of this node. */
    QList<QnResourcePoolModelNode *> m_children;

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
    Qt::CheckState m_checked;

    //TODO: #GDM #Common implement cache invalidating in case of permissions change
    /** Whether this resource can be renamed, cached value. */
    mutable struct {
        bool value;
        bool checked;
    } m_editable;
};

#endif // RESOURCE_POOL_MODEL_NODE_H
