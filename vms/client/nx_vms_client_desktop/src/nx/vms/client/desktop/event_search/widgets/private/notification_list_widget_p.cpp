#include "notification_list_widget_p.h"
#include "tile_interaction_handler_p.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QToolButton>
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
#include <ui/models/sort_filter_list_model.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>

#include <nx/vms/client/desktop/common/models/subset_list_model.h>
#include <nx/vms/client/desktop/common/models/concatenation_list_model.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/client/desktop/event_search/widgets/event_tile.h>
#include <nx/vms/client/desktop/event_search/models/system_health_list_model.h>
#include <nx/vms/client/desktop/event_search/models/notification_list_model.h>
#include <nx/vms/client/desktop/event_search/models/progress_list_model.h>
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

class SystemHealthSortFilterModel: public QnSortFilterListModel
{
public:
    using QnSortFilterListModel::QnSortFilterListModel;

    bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const
    {
        return sourceLeft.data(Qn::PriorityRole).toInt()
            > sourceRight.data(Qn::PriorityRole).toInt();
    }
};

class SeparatorListModel: public QAbstractListModel
{
public:
    using QAbstractListModel::QAbstractListModel; //< Forward constructors.

    virtual int rowCount(const QModelIndex& parent) const override
    {
        return parent.isValid() ? 0 : 1;
    }

    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        return QVariant();
    }
};

static constexpr int kPlaceholderFontPixelSize = 15;

} // namespace


NotificationListWidget::Private::Private(NotificationListWidget* q) :
    QObject(),
    QnWorkbenchContextAware(q),
    q(q),
    m_eventRibbon(new EventRibbon(q)),
    m_placeholder(new QWidget(q)),
    m_systemHealthModel(new SystemHealthListModel(context(), this)),
    m_notificationsModel(new NotificationListModel(context(), this))
{
    m_placeholder->setMinimumSize(QSize(0, 100));
    anchorWidgetToParent(m_placeholder);
    anchorWidgetToParent(m_eventRibbon);

    const auto verticalLayout = new QVBoxLayout(m_placeholder);
    verticalLayout->setSpacing(16);
    verticalLayout->setContentsMargins(24, 64, 24, 24);
    const auto placeholderIcon = new QLabel(m_placeholder);
    placeholderIcon->setFixedSize(QSize(64, 64));
    placeholderIcon->setPixmap(qnSkin->pixmap(lit("events/placeholders/notifications.png")));
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
    verticalLayout->addStretch(1);

    m_eventRibbon->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_eventRibbon->scrollBar()->ensurePolished();
    setPaletteColor(m_eventRibbon->scrollBar(), QPalette::Disabled, QPalette::Midlight,
        colorTheme()->color("dark5"));

    auto sortModel = new SystemHealthSortFilterModel(this);
    auto systemHealthListModel = new SubsetListModel(sortModel, 0, QModelIndex(), this);
    sortModel->setSourceModel(m_systemHealthModel);

    auto progressModel = new ProgressListModel(this);

    auto separatorModel = new SeparatorListModel(this);

    m_eventRibbon->setModel(new ConcatenationListModel(
        {systemHealthListModel, progressModel, separatorModel, m_notificationsModel}, this));

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
