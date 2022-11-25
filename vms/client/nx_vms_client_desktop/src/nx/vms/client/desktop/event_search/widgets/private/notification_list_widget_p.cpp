// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_list_widget_p.h"

#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QAction>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <business/business_resource_validation.h>
#include <client/client_settings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/models/subset_list_model.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/notification_tab_model.h>
#include <nx/vms/client/desktop/event_search/widgets/abstract_search_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/client/desktop/event_search/widgets/event_tile.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/rules/engine.h>
#include <ui/common/palette.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>
#include <watchers/cloud_status_watcher.h>

#include "tile_interaction_handler_p.h"

namespace nx::vms::client::desktop {

NotificationListWidget::Private::Private(NotificationListWidget* q):
    QObject(),
    QnWorkbenchContextAware(q),
    q(q),
    m_mainLayout(new QVBoxLayout(q)),
    m_headerWidget(new QWidget(q)),
    m_separatorLine(new QFrame(q)),
    m_itemCounterLabel(new QLabel(q)),
    m_ribbonContainer(new QWidget(q)),
    m_filterSystemsButton(new SelectableTextButton(q)),
    m_eventRibbon(new EventRibbon(m_ribbonContainer)),
    m_model(new NotificationTabModel(context(), this)),
    m_filterModel(new QSortFilterProxyModel(q))
{
    m_headerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_headerWidget->setStyleSheet(
        nx::format("QWidget { background-color: %1; border-left: 1px solid %2; }",
            colorTheme()->color("dark4").name(),
            colorTheme()->color("dark8").name()));

    m_ribbonContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    m_separatorLine->setFrameShape(QFrame::HLine);
    m_separatorLine->setFrameShadow(QFrame::Plain);
    m_separatorLine->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_separatorLine->setMinimumSize({0, 1});
    setPaletteColor(m_separatorLine, QPalette::Shadow, colorTheme()->color("dark6"));

    m_mainLayout->setContentsMargins({});
    m_mainLayout->setSpacing(0);
    m_mainLayout->addWidget(m_headerWidget);
    m_mainLayout->addWidget(m_separatorLine);
    m_mainLayout->addWidget(m_ribbonContainer);

    auto headerLayout = new QVBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(8, 8, 8, 8);
    headerLayout->setSpacing(0);
    headerLayout->addWidget(m_filterSystemsButton);
    setupFilterSystemsButton();

    anchorWidgetToParent(m_eventRibbon);

    const auto viewportHeader = new QWidget(q);
    viewportHeader->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    const auto viewportLayout = new QVBoxLayout(viewportHeader);
    viewportLayout->setContentsMargins(0, 8, 0, 8);
    viewportLayout->addWidget(m_itemCounterLabel);

    m_eventRibbon->setViewportHeader(viewportHeader);
    m_eventRibbon->setViewportMargins(0, 0);

    m_itemCounterLabel->setForegroundRole(QPalette::Mid);
    m_itemCounterLabel->setText({});

    m_eventRibbon->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_eventRibbon->scrollBar()->ensurePolished();
    setPaletteColor(m_eventRibbon->scrollBar(), QPalette::Disabled, QPalette::Midlight,
        colorTheme()->color("dark5"));

    m_filterModel->setSourceModel(m_model);
    m_filterModel->setFilterRole(Qn::CloudSystemIdRole);
    auto listFilterModelProxy = new SubsetListModel(m_filterModel, 0, QModelIndex(), q);
    m_eventRibbon->setModel(listFilterModelProxy);

    connect(m_eventRibbon, &EventRibbon::hovered, q, &NotificationListWidget::tileHovered);

    connect(m_eventRibbon,
        &EventRibbon::unreadCountChanged,
        q,
        &NotificationListWidget::unreadCountChanged);

    const auto updateCountLabel =
        [this](int count)
        {
            static constexpr int kMinimumCount = 1; //< Separator.
            m_itemCounterLabel->setText(
                count <= kMinimumCount ? "" : tr("%n notifications", "", count - kMinimumCount));
        };

    connect(m_eventRibbon, &EventRibbon::countChanged, this, updateCountLabel);
    connect(context(), &QnWorkbenchContext::userChanged, this,
        [this] { changeFilterVisibilityIfNeeded(); });

    NX_ASSERT(qnCloudStatusWatcher, "Cloud status watcher is not ready");
    connect(qnCloudStatusWatcher,
        &QnCloudStatusWatcher::cloudSystemsChanged,
        this,
        [this] { changeFilterVisibilityIfNeeded(); });

    updateCountLabel(m_eventRibbon->count());

    TileInteractionHandler::install(m_eventRibbon);

    changeFilterVisibilityIfNeeded();
}

NotificationListWidget::Private::~Private()
{
}

void NotificationListWidget::Private::setupFilterSystemsButton()
{
    m_filterSystemsButton->setFlat(true);
    m_filterSystemsButton->setSelectable(false);
    m_filterSystemsButton->setDeactivatable(true);
    m_filterSystemsButton->setIcon(qnSkin->icon("text_buttons/system.svg"));
    m_filterSystemsButton->setFocusPolicy(Qt::NoFocus);

    auto menu = new QMenu(q);
    menu->setProperty(style::Properties::kMenuAsDropdown, true);
    menu->setWindowFlags(menu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    const auto updateButtonText =
        [this](RightPanel::SystemSelection system)
        {
            m_filterSystemsButton->setText(m_systemSelectionActions[system]->text());
            m_filterSystemsButton->setState(system == RightPanel::SystemSelection::current
                    ? SelectableTextButton::State::deactivated
                    : SelectableTextButton::State::unselected);
           m_filterSystemsButton->setAccented(system == RightPanel::SystemSelection::all);
        };


    const auto addMenuAction =
        [this, menu, updateButtonText](const QString& title, RightPanel::SystemSelection system)
        {
            auto action = menu->addAction(title);
            connect(action, &QAction::triggered, this,
                [this, updateButtonText, system]()
                {
                    if (system == RightPanel::SystemSelection::current)
                        m_filterModel->setFilterRegularExpression("^$");
                    else
                        m_filterModel->setFilterWildcard("*");

                    updateButtonText(system);
                });
            m_systemSelectionActions[system] = action;
            return action;
        };

    addMenuAction(tr("Current System"), RightPanel::SystemSelection::current);
    addMenuAction(tr("All Systems"), RightPanel::SystemSelection::all);

    connect(m_filterSystemsButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                m_systemSelectionActions[RightPanel::SystemSelection::current]->trigger();
        });

    m_systemSelectionActions[RightPanel::SystemSelection::current]->trigger();
    m_filterSystemsButton->setMenu(menu);
}

void NotificationListWidget::Private::changeFilterVisibilityIfNeeded()
{
    if (auto user = context()->user();
        user && user->isCloud() && qnCloudStatusWatcher->cloudSystems().size() > 1
        && systemContext()->vmsRulesEngine()->isEnabled())
    {
        m_headerWidget->show();
        m_separatorLine->show();
    }
    else
    {
        m_headerWidget->hide();
        m_separatorLine->hide();
    }
}

} // namespace nx::vms::client::desktop
