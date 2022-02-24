// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_list_widget_p.h"
#include "tile_interaction_handler_p.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QMenu>
#include <QtGui/QMouseEvent>

#include <business/business_resource_validation.h>
#include <client/client_settings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <utils/common/event_processors.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <ui/common/palette.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <ui/workbench/workbench_context.h>

#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/client/desktop/event_search/widgets/event_tile.h>
#include <nx/vms/client/desktop/event_search/models/notification_tab_model.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kPlaceholderFontPixelSize = 15;

} // namespace


NotificationListWidget::Private::Private(NotificationListWidget* q) :
    QObject(),
    QnWorkbenchContextAware(q),
    q(q),
    m_eventRibbon(new EventRibbon(q)),
    m_placeholder(new QWidget(q)),
    m_model(new NotificationTabModel(context(), this))
{
    m_placeholder->setMinimumSize(QSize(0, 100));
    anchorWidgetToParent(m_placeholder);
    anchorWidgetToParent(m_eventRibbon);

    const auto verticalLayout = new QVBoxLayout(m_placeholder);
    verticalLayout->setSpacing(16);
    verticalLayout->setContentsMargins(24, 64, 24, 24);
    const auto placeholderIcon = new QLabel(m_placeholder);
    placeholderIcon->setFixedSize(QSize(64, 64));
    placeholderIcon->setPixmap(qnSkin->pixmap(lit("left_panel/placeholders/notifications.svg")));
    verticalLayout->addWidget(placeholderIcon, 0, Qt::AlignHCenter);
    const auto placeholderText = new QLabel(m_placeholder);
    QFont font;
    font.setPixelSize(kPlaceholderFontPixelSize);
    placeholderText->setProperty(style::Properties::kDontPolishFontProperty, true);
    placeholderText->setFont(font);
    placeholderText->setForegroundRole(QPalette::Mid);
    placeholderText->setText(tr("No new notifications"));
    placeholderText->setAlignment(Qt::AlignCenter);
    placeholderText->setWordWrap(true);
    verticalLayout->addWidget(placeholderText);

    const auto settingsButton = new QPushButton(tr("Notifications Settings"), m_placeholder);
    settingsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    verticalLayout->addWidget(settingsButton);
    verticalLayout->setAlignment(settingsButton, Qt::AlignHCenter);
    connect(settingsButton, &QPushButton::clicked,
        this, [this]() { menu()->trigger(ui::action::PreferencesNotificationTabAction); });

    verticalLayout->addStretch(1);

    m_eventRibbon->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_eventRibbon->scrollBar()->ensurePolished();
    setPaletteColor(m_eventRibbon->scrollBar(), QPalette::Disabled, QPalette::Midlight,
        colorTheme()->color("dark5"));

    m_eventRibbon->setModel(m_model);

    connect(m_eventRibbon, &EventRibbon::hovered, q, &NotificationListWidget::tileHovered);

    connect(m_eventRibbon, &EventRibbon::unreadCountChanged,
        q, &NotificationListWidget::unreadCountChanged);

    const auto updatePlaceholderVisibility =
        [this](int count)
        {
            static constexpr int kMinimumCount = 1; //< Separator.
            m_placeholder->setVisible(count <= kMinimumCount);
        };

    connect(m_eventRibbon, &EventRibbon::countChanged, this, updatePlaceholderVisibility);
    updatePlaceholderVisibility(m_eventRibbon->count());

    TileInteractionHandler::install(m_eventRibbon);
}

NotificationListWidget::Private::~Private()
{
}

} // namespace nx::vms::client::desktop
