#include "right_panel_models_adapter.h"

#include <QtQml/QtQml>

#include <core/resource/resource.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/html.h>

#include <nx/utils/scope_guard.h>
#include <nx/vms/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/bookmark_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/event_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/simple_motion_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/notification_tab_model.h>

namespace nx::vms::client::desktop {

RightPanelModelsAdapter::RightPanelModelsAdapter(QObject* parent):
    base_type(parent)
{
}

QnWorkbenchContext* RightPanelModelsAdapter::context() const
{
    return m_context;
}

void RightPanelModelsAdapter::setContext(QnWorkbenchContext* context)
{
    if (m_context == context)
        return;

    m_context = context;
    recreateSourceModel();
    emit contextChanged();
}

RightPanel::Tab RightPanelModelsAdapter::type() const
{
    return m_type;
}

void RightPanelModelsAdapter::setType(RightPanel::Tab value)
{
    if (m_type == value)
        return;

    m_type = value;
    recreateSourceModel();
    emit typeChanged();
}

QVariant RightPanelModelsAdapter::data(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case Qn::ResourceRole:
        {
            const auto resource = base_type::data(index, role).value<QnResourcePtr>();
            if (!resource)
                return {};

            QQmlEngine::setObjectOwnership(resource.get(), QQmlEngine::CppOwnership);
            return QVariant::fromValue(resource.get());
        }

        case Qn::DisplayedResourceListRole:
        {
            const auto data = base_type::data(index, role);
            QStringList result = data.value<QStringList>();

            if (result.empty())
            {
                auto resources = data.value<QnResourceList>();
                for (const auto& resource: resources)
                    result.push_back(htmlBold(resource->getName()));
            }

            return result;
        }

        default:
            return base_type::data(index, role);
    }
}

QHash<int, QByteArray> RightPanelModelsAdapter::roleNames() const
{
    auto roles = base_type::roleNames();
    roles.insert(Qt::ForegroundRole, "foregroundColor");
    roles.insert(Qn::ResourceRole, "previewResource");
    roles.insert(Qn::DescriptionTextRole, "description");
    roles.insert(Qn::DecorationPathRole, "decorationPath");
    roles.insert(Qn::TimestampTextRole, "timestamp");
    roles.insert(Qn::AdditionalTextRole, "additionalText");
    roles.insert(Qn::DisplayedResourceListRole, "resourceList");
    roles.insert(Qn::PreviewTimeRole, "previewTimestamp");
    roles.insert(Qn::ItemZoomRectRole, "previewCropRect");
    roles.insert(Qn::ForcePrecisePreviewRole, "precisePreview");
    roles.insert(Qn::RemovableRole, "isCloseable");
    roles.insert(Qn::AlternateColorRole, "isInformer");
    // TODO: Preview stream
    return roles;
}

bool RightPanelModelsAdapter::removeRow(int row)
{
    return base_type::removeRows(row, 1);
}

void RightPanelModelsAdapter::recreateSourceModel()
{
    const auto oldSourceModel = sourceModel();
    setSourceModel(nullptr);

    if (oldSourceModel && oldSourceModel->parent() == this)
        delete oldSourceModel;

    if (!m_context)
        return;

    switch (m_type)
    {
        case RightPanel::Tab::invalid:
            break;

        case RightPanel::Tab::notifications:
            setSourceModel(new NotificationTabModel(m_context, this));
            break;

        case RightPanel::Tab::motion:
            setSourceModel(new SimpleMotionSearchListModel(m_context, this));
            break;

        case RightPanel::Tab::bookmarks:
            setSourceModel(new BookmarkSearchListModel(m_context, this));
            break;

        case RightPanel::Tab::events:
            setSourceModel(new EventSearchListModel(m_context, this));
            break;

        case RightPanel::Tab::analytics:
            setSourceModel(new AnalyticsSearchListModel(m_context, this));
            break;
    }
}

void RightPanelModelsAdapter::registerQmlType()
{
    qmlRegisterType<RightPanelModelsAdapter>("nx.vms.client.desktop", 1, 0, "RightPanelModel");
}

} // namespace nx::vms::client::desktop
