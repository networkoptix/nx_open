// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../notification_list_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/event_search/right_panel_globals.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <ui/workbench/workbench_context_aware.h>

class QSortFilterProxyModel;
class QToolButton;

namespace nx::vms::client::desktop {

class EventRibbon;
class NotificationTabModel;
class SelectableTextButton;

class NotificationListWidget::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    Private(NotificationListWidget* q);
    virtual ~Private() override;

private:
    void setupFilterSystemsButton();
    void changeFilterVisibilityIfNeeded();

private:
    NotificationListWidget* const q;
    QVBoxLayout* const m_mainLayout;
    QWidget* const m_headerWidget;
    QFrame* const m_separatorLine;
    QLabel* const m_itemCounterLabel;
    QWidget* const m_ribbonContainer;
    SelectableTextButton* const m_filterSystemsButton;
    EventRibbon* const m_eventRibbon;
    NotificationTabModel* const m_model;
    QSortFilterProxyModel* const m_filterModel;
    QHash<RightPanel::SystemSelection, QAction*> m_systemSelectionActions;
};

} // namespace nx::vms::client::desktop
