#pragma once

#include <ui/models/resource/resource_tree_model_node.h>

#include <network/base_system_description.h>

class QnResourceTreeModelCloudSystemNode: public QnResourceTreeModelNode
{
    using base_type = QnResourceTreeModelNode;
public:
    QnResourceTreeModelCloudSystemNode(
        const QnSystemDescriptionPtr& system,
        QnResourceTreeModel* model);

    virtual ~QnResourceTreeModelCloudSystemNode();

    virtual void initialize() override;
    virtual void deinitialize() override;

    virtual QVariant data(int role, int column) const override;
    virtual Qt::ItemFlags flags(int column) const override;

protected:
    virtual QIcon calculateIcon() const override;

private:
    QnSystemDescriptionPtr m_system;
};
