#pragma once

#include <ui/models/resource/resource_tree_model_node.h>

#include <network/base_system_description.h>

#include <nx/utils/disconnect_helper.h>

/**
 * Node which displays cloud systems, accessible to the client, and local systems, accessible to
 * the server.
 */
class QnResourceTreeModelOtherSystemsNode: public QnResourceTreeModelNode
{
    using base_type = QnResourceTreeModelNode;

    Q_OBJECT
public:
    QnResourceTreeModelOtherSystemsNode(QnResourceTreeModel* model);
    virtual ~QnResourceTreeModelOtherSystemsNode();

    virtual void initialize() override;
    virtual void deinitialize() override;

protected:
    virtual QIcon calculateIcon() const override;

private:
    void handleSystemDiscovered(const QnSystemDescriptionPtr& system);
    void handleSystemLost(const QString& id);
    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);

    void updateFakeServerNode(const QnFakeMediaServerResourcePtr& server);

    QnResourceTreeModelNodePtr ensureCloudSystemNode(const QnSystemDescriptionPtr& system);
    QnResourceTreeModelNodePtr ensureLocalSystemNode(const QString &systemName);
    QnResourceTreeModelNodePtr ensureFakeServerNode(const QnFakeMediaServerResourcePtr& server);

    bool canSeeFakeServers() const;
    bool canSeeCloudSystem(const QnSystemDescriptionPtr& system) const;

    void cleanupEmptyLocalNodes();

    void rebuild();

    /** Cleanup all node references. */
    void removeNode(const QnResourceTreeModelNodePtr& node);

    /** Remove all nodes. */
    void clean();

private:
    QHash<QString, QnDisconnectHelper> m_disconnectHelpers;
    QHash<QString, QnResourceTreeModelNodePtr> m_cloudNodes;
    QHash<QString, QnResourceTreeModelNodePtr> m_localNodes;
    ResourceHash m_fakeServers;
};
