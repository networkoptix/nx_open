#pragma once

#include <core/resource_access/resource_access_subject.h>

#include <ui/models/resource/resource_tree_model_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

/**
 * Helper class for constructing users and roles part of the resource tree.
 */
class QnResourceTreeModelUserNodes: public QObject, public QnWorkbenchContextAware
{
    using base_type = QObject;
public:
    QnResourceTreeModelUserNodes(QObject* parent = nullptr);
    virtual ~QnResourceTreeModelUserNodes();

    QnResourceTreeModel* model() const;
    void setModel(QnResourceTreeModel* value);

    QnResourceTreeModelNodePtr rootNode() const;
    void setRootNode(const QnResourceTreeModelNodePtr& node);

    void rebuild();
private:
    /** Calculate real children as node's children() method does not return bastard nodes. */
    QList<QnResourceTreeModelNodePtr> children(const QnResourceTreeModelNodePtr& node) const;

    QList<Qn::NodeType> allPlaceholders() const;

    bool placeholderAllowedForSubject(const QnResourceAccessSubject& subject,
        Qn::NodeType nodeType) const;

    bool showLayoutForSubject(const QnResourceAccessSubject& subject,
        const QnLayoutResourcePtr& layout) const;

    bool showMediaForSubject(const QnResourceAccessSubject& subject,
        const QnResourcePtr& media) const;

    /** Get or create node for user or user role. */
    QnResourceTreeModelNodePtr ensureSubjectNode(const QnResourceAccessSubject& subject);

    /** Get or create user role node. */
    QnResourceTreeModelNodePtr ensureRoleNode(const ec2::ApiUserGroupData& role);

    /** Get or create user node. */
    QnResourceTreeModelNodePtr ensureUserNode(const QnUserResourcePtr& user);

    /** Get or create layout node. */
    QnResourceTreeModelNodePtr ensureLayoutNode(const QnResourceAccessSubject& subject,
        const QnLayoutResourcePtr& layout);

    /** Get or create shared media node. */
    QnResourceTreeModelNodePtr ensureMediaNode(const QnResourceAccessSubject& subject,
        const QnResourcePtr& media);

    /** Get or create placeholder node like 'All Shared layouts'. */
    QnResourceTreeModelNodePtr ensurePlaceholderNode(const QnResourceAccessSubject& subject,
        Qn::NodeType nodeType);

    /** Get or create recorder placeholder for grouped cameras. */
    QnResourceTreeModelNodePtr ensureRecorderNode(const QnResourceTreeModelNodePtr& parentNode,
        const QString& groupId, const QString& groupName);

    void rebuildSubjectTree(const QnResourceAccessSubject& subject);

    /** Cleanup all node references. */
    void removeNode(const QnResourceTreeModelNodePtr& node);

    /** Remove node from the model. */
    void removeNodeInternal(const QnResourceTreeModelNodePtr& node);

    /** Remove all nodes. */
    void clean();

    /** Remove recorder nodes that are not in use. */
    void cleanupRecorders();

    void handleAccessChanged(const QnResourceAccessSubject& subject, const QnResourcePtr& resource);
    void handleGlobalPermissionsChanged(const QnResourceAccessSubject& subject);
private:
    QnResourceTreeModel* m_model;

    /** Root node for all users and roles. */
    QnResourceTreeModelNodePtr m_rootNode;

    /** Full list of all created nodes (to avoid double removing). */
    QList<QnResourceTreeModelNodePtr> m_allNodes;

    /** Mapping of role nodes by id. */
    QHash<QnUuid, QnResourceTreeModelNodePtr> m_roles;

    /** Mapping of user nodes by id. */
    QHash<QnUuid, QnResourceTreeModelNodePtr> m_users;

    /** Mapping of placeholder nodes by subject id. */
    QHash<QnUuid, NodeList> m_placeholders;

    /** Mapping of shared layouts links by subject id. */
    QHash<QnUuid, NodeList> m_sharedLayouts;

    /** Mapping of shared resources (cameras, web pages, statistics) by subject id. */
    QHash<QnUuid, NodeList> m_sharedResources;

    /** Mapping for recorder nodes by parent node. */
    QHash<QnResourceTreeModelNodePtr, RecorderHash> m_recorders;
};
