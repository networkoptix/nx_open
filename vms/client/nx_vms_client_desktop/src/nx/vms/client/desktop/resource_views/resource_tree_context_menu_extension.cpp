// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_context_menu_extension.h"

#include <QtGui/QContextMenuEvent>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTreeView>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_context.h>

#include "resource_tree_interaction_handler.h"

namespace {

QModelIndexList getSelectedIndexes(QTreeView* treeView)
{
    if (!NX_ASSERT(treeView) || !NX_ASSERT(treeView->selectionModel()))
        return QModelIndexList();

    NX_ASSERT(treeView->selectionBehavior() == QAbstractItemView::SelectRows);
    return treeView->selectionModel()->selectedRows();
}

} // namespace

namespace nx::vms::client::desktop {

ResourceTreeContextMenuExtension::ResourceTreeContextMenuExtension(
    QTreeView* treeView,
    QnWorkbenchContext* workbenchContext)
    :
    base_type(treeView),
    m_treeView(treeView),
    m_interactionHandler(new ResourceTreeInteractionHandler(workbenchContext))
{
    if (NX_ASSERT(m_treeView) && NX_ASSERT(workbenchContext))
        m_treeView->installEventFilter(this);
}

ResourceTreeContextMenuExtension::~ResourceTreeContextMenuExtension()
{
    if (m_treeView)
        m_treeView->removeEventFilter(this);
}

bool ResourceTreeContextMenuExtension::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_treeView || event->type() != QEvent::ContextMenu)
        return base_type::eventFilter(watched, event);

    const auto menuEvent = static_cast<QContextMenuEvent*>(event);
    const auto index = m_treeView->indexAt(menuEvent->pos());

    if (index.isValid())
    {
        m_interactionHandler->showContextMenu(m_treeView->window(), menuEvent->globalPos(),
            index, getSelectedIndexes(m_treeView));
    }

    return base_type::eventFilter(watched, event);
}

} // namespace nx::vms::client::desktop
