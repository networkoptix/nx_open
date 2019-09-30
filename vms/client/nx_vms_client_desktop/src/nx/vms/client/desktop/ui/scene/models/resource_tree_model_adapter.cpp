#include "resource_tree_model_adapter.h"

#include <QtQml/QtQml>

#include <client/client_globals.h>
#include <client/client_module.h>
#include <core/resource/camera_resource.h>
#include <ui/models/resource/resource_tree_model.h>
#include <ui/workbench/workbench_context.h>

#include <nx/vms/client/desktop/utils/wearable_manager.h>
#include <nx/vms/client/desktop/utils/wearable_state.h>

namespace nx::vms::client::desktop {

ResourceTreeModelAdapter::ResourceTreeModelAdapter(QObject* parent):
    base_type(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterKeyColumn(Qn::NameColumn);
    setSortRole(Qt::DisplayRole);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    sort(Qn::NameColumn);
}

QnWorkbenchContext* ResourceTreeModelAdapter::context() const
{
    return m_context;
}

void ResourceTreeModelAdapter::setContext(QnWorkbenchContext* context)
{
    if (m_context == context)
        return;

    const auto oldSourceModel = sourceModel();
    setSourceModel(nullptr);

    if (oldSourceModel && oldSourceModel->parent() == this)
        delete oldSourceModel;

    m_context = context;
    if (m_context)
    {
        const auto model = new QnResourceTreeModel(
            QnResourceTreeModel::FullScope,
            m_context->accessController(),
            m_context->snapshotManager(),
            m_context);
        model->setParent(this);
        setSourceModel(model);
    }

    emit contextChanged();
}

QVariant ResourceTreeModelAdapter::data(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case Qn::ExtraResourceInfoRole:
        {
            static const QString kCustomExtInfoTemplate = //< "- %1" with en-dash
                QString::fromWCharArray(L"\x2013 %1");

            const auto resource = base_type::data(index, Qn::ResourceRole).value<QnResourcePtr>();
            const auto nodeType = base_type::data(
                index, Qn::NodeTypeRole).value<ResourceTree::NodeType>();

            if ((nodeType == ResourceTree::NodeType::layoutItem
                    || nodeType == ResourceTree::NodeType::sharedResource)
                && resource->hasFlags(Qn::server) && !resource->hasFlags(Qn::fake))
            {
                return kCustomExtInfoTemplate.arg(tr("Health Monitor"));
            }

            const auto extraInfo = base_type::data(index, role).toString();
            if (extraInfo.isEmpty())
                return {};

            if (resource->hasFlags(Qn::user))
                return kCustomExtInfoTemplate.arg(extraInfo);

            if (resource->hasFlags(Qn::wearable_camera))
            {
                const auto camera = resource.dynamicCast<QnSecurityCamResource>();
                const auto state = qnClientModule->wearableManager()->state(camera);
                if (state.isRunning())
                    return kCustomExtInfoTemplate.arg(QString::number(state.progress()) + lit("%"));
            }

            return extraInfo;
        }

        case Qn::ResourceRole:
        {
            const auto resource = base_type::data(index, role).value<QnResourcePtr>();
            if (!resource)
                return {};

            QQmlEngine::setObjectOwnership(resource.get(), QQmlEngine::CppOwnership);
            return QVariant::fromValue(resource.get());
        }

        default:
            return base_type::data(index, role);
    }
}

void ResourceTreeModelAdapter::registerQmlType()
{
    qmlRegisterType<ResourceTreeModelAdapter>("nx.vms.client.desktop", 1, 0, "ResourceTreeModel");
}

} // nx::vms::client::desktop
