// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_search_list_model.h"

#include <QtWidgets/QMenu>

#include <analytics/db/analytics_db_types.h>
#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <nx/analytics/action_type_descriptor_manager.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>

namespace nx::vms::client::desktop {

QVariant AnalyticsSearchListModel::data(const QModelIndex& index, int role) const
{
    if (!NX_ASSERT(qBetween<int>(0, index.row(), rowCount())))
        return {};

    const auto& track = trackByRow(index.row());
    switch (role)
    {
        case Qn::HelpTopicIdRole:
            return HelpTopic::Id::Empty;

        case core::ItemZoomRectRole:
            return QVariant::fromValue(previewParams(track).boundingBox);

        case Qn::CreateContextMenuRole:
            return QVariant::fromValue(contextMenu(track));

        default:
            return base_type::data(index, role);
    }
}

QSharedPointer<QMenu> AnalyticsSearchListModel::contextMenu(
    const nx::analytics::db::ObjectTrack& track) const
{
    const auto cameraResource = camera(track);
    if (!cameraResource)
        return {};

    const nx::analytics::ActionTypeDescriptorManager descriptorManager(systemContext());
    const auto actionByEngine = descriptorManager.availableObjectActionTypeDescriptors(
        track.objectTypeId,
        cameraResource);

    QSharedPointer<QMenu> menu(new QMenu());
    for (const auto& [engineId, actionById]: actionByEngine)
    {
        if (!menu->isEmpty())
            menu->addSeparator();

        for (const auto& [actionId, actionDescriptor]: actionById)
        {
            const auto name = actionDescriptor.name;
            menu->addAction<std::function<void()>>(name, nx::utils::guarded(this,
                [this, engineId = engineId, actionId = actionDescriptor.id, track, cameraResource]()
                {
                    emit this->pluginActionRequested(engineId, actionId, track, cameraResource);
                }));
        }
    }

    menu->addSeparator();

    if (ResourceAccessManager::hasPermissions(cameraResource, Qn::ReadWriteSavePermission))
    {
        const auto listManager = systemContext()->lookupListManager();
        auto& lists = listManager->lookupLists();
        if (lists.empty())
        {
            addCreateNewListAction(menu.get(), track);
        }
        else
        {
            auto addMenu = menu->addMenu(tr("Add To List"));
            addCreateNewListAction(addMenu, track);

            addMenu->addSeparator();
            for (const auto& list: lists)
            {
                if (list.objectTypeId != track.objectTypeId)
                    continue; //< We can add object only to list with same objectTypeId.

                addMenu->addAction<std::function<void()>>(list.name,
                    [this, track, id = list.id]()
                    {
                        api::LookupListEntry entry;
                        for(const auto& attribute: track.attributes)
                            entry[attribute.name] = attribute.value;

                        menu::Parameters parameters;
                        parameters.setArguments({
                            {Qn::LookupListEntryRole, QVariant::fromValue(entry)},
                            {Qn::ItemUuidRole, QVariant::fromValue(id)}});
                        appContext()->mainWindowContext()->menu()->triggerForced(
                            menu::AddEntryToLookupListAction, parameters);
                    });
            }
        }
    }

    if (menu->isEmpty())
        menu.reset();

    return menu;
}

void AnalyticsSearchListModel::addCreateNewListAction(
    QMenu* menu, const nx::analytics::db::ObjectTrack& track) const
{
    menu->addAction<std::function<void()>>(tr("Create New List..."),
        [id = track.objectTypeId, this]()
        {
            appContext()->mainWindowContext()->menu()->triggerForced(
                menu::OpenEditLookupListsDialogAction,
                menu::Parameters().withArgument(Qn::AnalyticsObjectTypeIdRole, id));
        });
}

} // namespace nx::vms::client::desktop
