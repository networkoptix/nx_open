#pragma once

#include <core/resource_access/resource_access_subject.h>

#include <ui/models/resource/resource_tree_model_node.h>

class QnResourceTreeModelLayoutNode: public QnResourceTreeModelNode
{
    using base_type = QnResourceTreeModelNode;
public:
    QnResourceTreeModelLayoutNode(QnResourceTreeModel* model, const QnResourcePtr& resource,
        Qn::NodeType nodeType = Qn::ResourceNode);
    virtual ~QnResourceTreeModelLayoutNode();

    virtual void setResource(const QnResourcePtr &resource) override;
    virtual void setParent(const QnResourceTreeModelNodePtr& parent) override;
    virtual void updateRecursive() override;

protected:
    void handleAccessChanged(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource);
    virtual void handlePermissionsChanged(const QnResourcePtr& resource) override;
    virtual QIcon calculateIcon() const override;

private:
    QnResourceAccessSubject getOwner() const;
    QIcon iconBySubject(const QnResourceAccessSubject& subject) const;

    void removeNode(const QnResourceTreeModelNodePtr& node);

    void handleResourceAdded(const QnResourcePtr& resource);

    void at_layout_itemAdded(const QnLayoutResourcePtr& layout, const QnLayoutItemData& item);
    void at_layout_itemRemoved(const QnLayoutResourcePtr& layout, const QnLayoutItemData& item);

    void at_snapshotManager_flagsChanged(const QnLayoutResourcePtr& layout);


private:
    ItemHash m_items;
};
