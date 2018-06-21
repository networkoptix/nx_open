#include "layout_selection_dialog_store.h"

#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <nx/client/core/watchers/user_watcher.h>
#include <nx/client/desktop/resource_views/layout/layout_selection_dialog_state_reducer.h>

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/providers/resource_access_provider.h>

using State = nx::client::desktop::LayoutSelectionDialogState;

namespace {

State setResources(const QnResourceList& resources)
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto accessProvider = commonModule->resourceAccessProvider();
    const auto userWatcher = commonModule->instance<nx::client::core::UserWatcher>();
    const auto currentUser = userWatcher->user();
    const auto currentUserId = currentUser->getId();

    State state;
    auto& subjects = state.subjects;
    for (const auto& user: resources.filtered<QnUserResource>())
    {
        if (accessProvider->hasAccess(currentUser, user))
            subjects.insert(user->getId(), user->getName());
    }

    auto& relations = state.relations;
    for (const auto& layout: resources.filtered<QnLayoutResource>())
    {
        if (!accessProvider->hasAccess(currentUser, layout))
            continue;

        const auto layoutId = layout->getId();
        const auto parentId = layout->getParentId();
        state.layouts.insert(layoutId, {layout->getName(), layout->isShared(), false});

        const auto parentSubjectId = subjects.contains(parentId) ? parentId : currentUserId;
        relations[parentSubjectId].append(layoutId);
    }

    int nonEmptySubjectsCount = 0;
    for (const auto layoutIdsForSubject: relations)
    {
        if (!layoutIdsForSubject.isEmpty())
            ++nonEmptySubjectsCount;
    }
    state.flatView = nonEmptySubjectsCount <= 1;
    return state;
}

State setSelection(State state, const QnUuid& layoutId, bool selected)
{
    const auto it = state.layouts.find(layoutId);
    if (it == state.layouts.end())
    {
        NX_EXPECT(false, "Can't find layout with specified id!");
        return state;
    }

    it->data.checked = selected;
    return state;
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

LayoutSelectionDialogStore::LayoutSelectionDialogStore(QObject* parent):
    base_type(parent)
{
}

void LayoutSelectionDialogStore::setLayoutSelection(const QnUuid& id, bool selected)
{
    execute([id, selected](const State& state) { return ::setSelection(state, id, selected); });
}

void LayoutSelectionDialogStore::setResources(const QnResourceList& resources)
{
    execute([resources](const State& /* state */) { return ::setResources(resources); });
}

} // namespace desktop
} // namespace client
} // namespace nx

