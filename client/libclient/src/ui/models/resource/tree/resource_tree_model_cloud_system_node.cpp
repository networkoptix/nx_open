#include "resource_tree_model_cloud_system_node.h"

#include <ui/style/skin.h>

QnResourceTreeModelCloudSystemNode::QnResourceTreeModelCloudSystemNode(
    const QnSystemDescriptionPtr& system,
    QnResourceTreeModel* model)
    :
    base_type(model, Qn::CloudSystemNode, QnUuid()),
    m_system(system)
{

}

QnResourceTreeModelCloudSystemNode::~QnResourceTreeModelCloudSystemNode()
{
}

void QnResourceTreeModelCloudSystemNode::initialize()
{
    base_type::initialize();

    connect(m_system, &QnBaseSystemDescription::systemNameChanged, this,
        [this]
        {
            setName(m_system->name());
        });

    connect(m_system, &QnBaseSystemDescription::onlineStateChanged, this,
        [this]
        {
            setIcon(calculateIcon());
        });

    setName(m_system->name());
    setIcon(calculateIcon());
}

void QnResourceTreeModelCloudSystemNode::deinitialize()
{
    m_system->disconnect(this);
    base_type::deinitialize();
}

QVariant QnResourceTreeModelCloudSystemNode::data(int role, int column) const
{
    if (role == Qn::CloudSystemIdRole)
        return m_system->id();

    return base_type::data(role, column);
}

QIcon QnResourceTreeModelCloudSystemNode::calculateIcon() const
{
    return m_system->isOnline()
        ? qnSkin->icon("welcome_page/cloud_online.png")
        : qnSkin->icon("welcome_page/cloud_unavailable.png");
}
