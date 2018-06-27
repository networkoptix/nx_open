#include "multiple_layout_selection_dialog.h"
#include "ui_multiple_layout_selection_dialog.h"

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <nx/client/core/watchers/user_watcher.h>

#include <nx/client/desktop/resource_views/node_view/base_view_node.h>
#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/resource_node.h>
#include <nx/client/desktop/resource_views/node_view/parent_resource_node.h>


namespace {

using namespace nx::client::desktop;

NodeList getUsers()
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto accessProvider = commonModule->resourceAccessProvider();
    const auto userWatcher = commonModule->instance<nx::client::core::UserWatcher>();
    const auto currentUser = userWatcher->user();
    const auto currentUserId = currentUser->getId();

    NodeList childNodes;
    const auto pool = qnClientCoreModule->commonModule()->resourcePool();

    const auto filterUser =
        [accessProvider, currentUser](const QnResourcePtr& resource)
        {
            return accessProvider->hasAccess(currentUser, resource.dynamicCast<QnUserResource>());
        };

    QSet<QnUuid> accessibleUserIds;
    QList<QnUserResourcePtr> accessibleUsers;
    for (const auto& userResource: pool->getResources<QnUserResource>(filterUser))
    {
        accessibleUserIds.insert(userResource->getId());
        accessibleUsers.append(userResource);
    }

    const auto isChildLayout=
        [accessProvider, accessibleUserIds, currentUserId](
            const QnResourcePtr& parent,
            const QnResourcePtr& child)
        {
            const auto user = parent.dynamicCast<QnUserResource>();
            const auto layout = child.dynamicCast<QnLayoutResource>();
            if (!layout || !accessProvider->hasAccess(user, layout))
                return false;

            const auto parentId = layout->getParentId();
            const auto userId = user->getId();
            return userId == parentId
                || (!accessibleUserIds.contains(parentId) && userId == currentUserId);
        };

    for (const auto& userResource: accessibleUsers)
    {
        const auto node = ParentResourceNode::create(userResource, isChildLayout);
        if (node->childrenCount() > 0)
            childNodes.append(node);
    }

    return childNodes;
}

//-------------------------------------------------------------------------------------------------

class NamedNode: public BaseViewNode
{
public:
    static NodePtr create(const QString& name, const NodeList& children = NodeList())
    {
        auto raw = new NamedNode(name);
        const auto result = NodePtr(raw);
        for (const auto& child: children)
            raw->addNode(child);
        return result;
    }

    virtual QVariant data(int column, int role) const override
    {
        return column == 0 && role == Qt::DisplayRole ? QVariant(m_name) : QVariant();
    }

private:
    NamedNode(const QString& name):
        m_name(name) {}

private:
    QString m_name;
};

NodeViewModel* createTestModel(QObject* parent)
{
    const auto model = new NodeViewModel(parent);

    const auto rootNode = BaseViewNode::create({
        NamedNode::create(lit("parent #1"), {
            NamedNode::create(lit("child #1_1"), {
                NamedNode::create(lit("leaf_1_1_1")),
                NamedNode::create(lit("leaf_1_1_2")),
                }),
            NamedNode::create(lit("child #1_2"))
            }),
        NamedNode::create(lit("parent #2"), {
            NamedNode::create(lit("child_2_1"))
            })
        });
    model->loadState({rootNode});
    return model;
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

struct MultipleLayoutSelectionDialog::Private
{
    Private(MultipleLayoutSelectionDialog* owner);


    MultipleLayoutSelectionDialog* const owner;
};

MultipleLayoutSelectionDialog::Private::Private(MultipleLayoutSelectionDialog* owner):
    owner(owner)
{
}

//-------------------------------------------------------------------------------------------------

MultipleLayoutSelectionDialog::MultipleLayoutSelectionDialog(QWidget* parent):
    base_type(parent),
    d(new Private(this)),
    ui(new Ui::MultipleLayoutSelectionDialog)
{
    ui->setupUi(this);

//    const auto model = createTestModel(parent);
    const auto model = new NodeViewModel();
    model->loadState({BaseViewNode::create(getUsers())});
    const auto tree = ui->layoutsTree;
    tree->setModel(model);
    tree->expandAll();
}

MultipleLayoutSelectionDialog::~MultipleLayoutSelectionDialog()
{
}

} // namespace desktop
} // namespace client
} // namespace nx
