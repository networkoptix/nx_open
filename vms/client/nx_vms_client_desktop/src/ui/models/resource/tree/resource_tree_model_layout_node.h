#pragma once

#include <core/resource_access/resource_access_subject.h>

#include <ui/models/resource/resource_tree_model_node.h>

class QnResourceTreeModelLayoutNode: public QnResourceTreeModelNode
{
    Q_OBJECT
    using base_type = QnResourceTreeModelNode;

public:
    QnResourceTreeModelLayoutNode(QnResourceTreeModel* model, const QnResourcePtr& resource,
        NodeType nodeType = NodeType::resource);
    virtual ~QnResourceTreeModelLayoutNode();

    virtual void updateRecursive() override;

    virtual void initialize() override;
    virtual void deinitialize() override;

    bool itemsLoaded() const;

protected:
    virtual QnResourceTreeModelNodeManager* manager() const override;

private:
    QnResourceAccessSubject getOwner() const;

    void addItem(const QnLayoutItemData& item);
    void removeItem(const QnLayoutItemData& item);

    void updateItem(const QnUuid& item);

    void updateItemResource(const QnUuid& item, const QnResourcePtr& resource);

    void updateLoadedState();

private:
    friend class QnResourceTreeModelLayoutNodeManager;
    ItemHash m_items;
    int m_loadedItems = 0;
    bool m_loaded = true;
};
