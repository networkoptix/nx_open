#include "event_search_widget_p.h"

#include <chrono>

#include <QtWidgets/QLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>

#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/models/sort_filter_list_model.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/search_line_edit.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/delayed.h>

#include <nx/client/desktop/common/models/subset_list_model.h>
#include <nx/client/desktop/event_search/models/unified_search_list_model.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static const auto kFilterDelay = std::chrono::milliseconds(250);
static const auto kTimeSelectionDelay = std::chrono::milliseconds(250);
static constexpr int kTextButtonHeight = 24;

static const QnTimePeriod kEntiretyTimePeriod(QnTimePeriod::kMinTimeValue,
    QnTimePeriod::kMaxTimeValue);

class EventSortFilterModel: public QnSortFilterListModel
{
public:
    using QnSortFilterListModel::QnSortFilterListModel;

    bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const
    {
        return sourceLeft.data(Qn::TimestampRole).value<qint64>()
            > sourceRight.data(Qn::TimestampRole).value<qint64>();
    }
};

} // namespace

EventSearchWidget::Private::Private(EventSearchWidget* q):
    base_type(q),
    q(q),
    m_model(new UnifiedSearchListModel(this)),
    m_sortFilterModel(new EventSortFilterModel(this)),
    m_searchLineEdit(new QnSearchLineEdit(m_headerWidget)),
    m_superTypeButton(new QPushButton(m_headerWidget)),
    m_eventTypeButton(new QPushButton(m_headerWidget)),
    m_selectAreaCheckBox(new QCheckBox(m_headerWidget)),
    m_timeSelectionLabel(new QLabel(m_headerWidget)),
    m_helper(new vms::event::StringsHelper(commonModule())),
    m_applyTimePeriodTimer(new QTimer(this))
{
    m_searchLineEdit->setTextChangedSignalFilterMs(std::chrono::milliseconds(kFilterDelay).count());

    setupSuperTypeButton();
    setupEventTypeButton();
    setupSelectAreaCheckBox();

    auto buttonsHolder = new QWidget(m_headerWidget);
    auto buttonsLayout = new QVBoxLayout(buttonsHolder);
    buttonsHolder->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    buttonsLayout->setSpacing(0);
    buttonsLayout->addWidget(m_superTypeButton);
    buttonsLayout->setAlignment(m_superTypeButton, Qt::AlignLeft);
    buttonsLayout->addWidget(m_eventTypeButton);
    buttonsLayout->setAlignment(m_eventTypeButton, Qt::AlignLeft);
    buttonsLayout->addWidget(m_selectAreaCheckBox);
    buttonsLayout->setAlignment(m_selectAreaCheckBox, Qt::AlignLeft);

    m_timeSelectionLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    m_timeSelectionLabel->setHidden(true);

    auto layout = m_headerWidget->layout();
    layout->addWidget(m_searchLineEdit);
    layout->addWidget(m_timeSelectionLabel);
    layout->addWidget(buttonsHolder);

    m_sortFilterModel->setSourceModel(m_model);
    m_sortFilterModel->setFilterRole(Qn::ResourceSearchStringRole);
    m_sortFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_sortFilterModel->setTriggeringRoles({Qn::TimestampRole});

    connect(m_searchLineEdit, &QnSearchLineEdit::textChanged,
        m_sortFilterModel, &EventSortFilterModel::setFilterWildcard);

    setModel(new SubsetListModel(m_sortFilterModel, 0, QModelIndex(), this));

    connect(m_model, &UnifiedSearchListModel::fetchAboutToBeCommitted, this,
        [this]() { m_previousRowCount = m_sortFilterModel->rowCount(); });

    connect(m_model, &UnifiedSearchListModel::fetchCommitted, this,
        [this]()
        {
            // If client-side filtering is not active, do nothing.
            const bool hasClientSideFilter = !m_sortFilterModel->filterWildcard().isEmpty();
            if (!hasClientSideFilter)
                return;

            // If fetch produced enough records after filtering, do nothing.
            const auto kReFetchCountThreshold = 15;
            if (m_sortFilterModel->rowCount() - m_previousRowCount > kReFetchCountThreshold)
                return;

            // Queue more fetch after a short delay.
            static const int kReFetchDelayMs = 50;
            queueFetchMoreIfNeeded(kReFetchDelayMs);
        });

    m_applyTimePeriodTimer->setSingleShot(true);
    m_applyTimePeriodTimer->setInterval(std::chrono::milliseconds(kTimeSelectionDelay).count());

    connect(m_applyTimePeriodTimer, &QTimer::timeout, this, &Private::updateEffectiveTimePeriod);

    connect(navigator(), &QnWorkbenchNavigator::timeSelectionChanged, this,
        [this](const QnTimePeriod& selection)
        {
            m_desiredTimePeriod = selection.isEmpty() ? kEntiretyTimePeriod : selection;
            m_applyTimePeriodTimer->start(); //< Start or restart.
        });
}

EventSearchWidget::Private::~Private()
{
}

void EventSearchWidget::Private::setupSuperTypeButton()
{
    m_superTypeButton->setFlat(true);
    m_superTypeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_superTypeButton->setFixedHeight(kTextButtonHeight);

    auto typeFilterMenu = new QMenu(m_headerWidget);
    typeFilterMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    typeFilterMenu->setWindowFlags(typeFilterMenu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    using Type = UnifiedSearchListModel::Type;
    using Types = UnifiedSearchListModel::Types;

    auto addMenuAction =
        [this, typeFilterMenu](const QString& title, const QIcon& icon, Types filter)
        {
            return typeFilterMenu->addAction(icon, title,
                [this, icon, title, filter]()
                {
                    m_superTypeButton->setIcon(icon);
                    m_superTypeButton->setText(title);
                    m_model->setFilter(filter);
                    updateButtonVisibility();
            });
        };

    auto defaultAction = addMenuAction(tr("All types"), QIcon(), Type::all);
    addMenuAction(tr("Events"), qnSkin->icon(lit("text_buttons/event_rules.png")), Type::events);
    addMenuAction(tr("Bookmarks"), qnSkin->icon(lit("text_buttons/bookmark.png")), Type::bookmarks);
    addMenuAction(tr("Detected objects"), qnSkin->icon(lit("text_buttons/analytics.png")),
        Type::analytics);
    defaultAction->trigger();

    m_superTypeButton->setMenu(typeFilterMenu);
}

void EventSearchWidget::Private::setupEventTypeButton()
{
    m_eventTypeButton->setFlat(true);
    m_eventTypeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_eventTypeButton->setFixedHeight(kTextButtonHeight);
    m_eventTypeButton->setHidden(true);

    auto eventFilterMenu = new QMenu(m_headerWidget);
    eventFilterMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    eventFilterMenu->setWindowFlags(eventFilterMenu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    auto addMenuAction =
        [this, eventFilterMenu](const QString& title, vms::event::EventType type)
        {
            return eventFilterMenu->addAction(title,
                [this, title, type]()
                {
                    m_eventTypeButton->setText(title);
                    m_model->setSelectedEventType(type);
                });
        };

    // TODO: #vkutin Specify icons when they're available.
    auto defaultAction = addMenuAction(tr("All events"), vms::event::undefinedEvent);
    for (const auto type: vms::event::allEvents())
    {
        if (vms::event::isSourceCameraRequired(type))
            addMenuAction(m_helper->eventName(type), type);
    }

    defaultAction->trigger();

    m_eventTypeButton->setMenu(eventFilterMenu);
}

void EventSearchWidget::Private::setupSelectAreaCheckBox()
{
    m_selectAreaCheckBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_selectAreaCheckBox->setFixedHeight(kTextButtonHeight);
    m_selectAreaCheckBox->setText(tr("Select Area"));

    connect(m_selectAreaCheckBox, &QCheckBox::stateChanged, this,
        [this]()
        {
            const bool enabled = analyticsSearchByAreaEnabled();
            if (!enabled)
                m_model->setAnalyticsSearchRect(QRectF());

            emit q->analyticsSearchByAreaEnabledChanged(enabled);
        });
}

void EventSearchWidget::Private::updateButtonVisibility()
{
    bool updated = false;

    const bool typeButtonHidden = m_model->filter() != int(UnifiedSearchListModel::Type::events);
    if (m_eventTypeButton->isHidden() != typeButtonHidden)
    {
        m_eventTypeButton->setHidden(typeButtonHidden);
        updated = true;
    }

    const bool selectAreaHidden = !m_model->filter().testFlag(
        UnifiedSearchListModel::Type::analytics);

    if (m_selectAreaCheckBox->isHidden() != selectAreaHidden)
    {
        m_selectAreaCheckBox->setHidden(selectAreaHidden);
        if (selectAreaHidden)
            m_selectAreaCheckBox->setChecked(false);
        updated = true;
    }

    if (!updated)
        return;

    activateLayouts(m_eventTypeButton->parentWidget());
}

void EventSearchWidget::Private::updateEffectiveTimePeriod()
{
    m_model->setSelectedTimePeriod(m_desiredTimePeriod);

    const bool entirety = m_desiredTimePeriod == kEntiretyTimePeriod;
    m_timeSelectionLabel->setHidden(entirety);
    if (!entirety)
    {
        const auto toString =
            [](qint64 fromEpochMs)
            {
                return QDateTime::fromMSecsSinceEpoch(fromEpochMs).toString(Qt::RFC2822Date);
            };

        const auto text = lit("<b>%1</b> %3<br><b>%2</b> %4")
            .arg(tr("From:")).arg(tr("To:"))
            .arg(toString(m_desiredTimePeriod.startTimeMs))
            .arg(toString(m_desiredTimePeriod.endTimeMs()));

        m_timeSelectionLabel->setText(text);
    }

    activateLayouts(m_timeSelectionLabel->parentWidget());
}

void EventSearchWidget::Private::activateLayouts(QWidget* from)
{
    for (auto widget = from; widget != nullptr; widget = widget->parentWidget())
    {
        if (widget->layout())
            widget->layout()->activate();
    }
}

QnVirtualCameraResourcePtr EventSearchWidget::Private::camera() const
{
    return m_model->camera();
}

void EventSearchWidget::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    m_model->setCamera(camera);
}

void EventSearchWidget::Private::setAnalyticsSearchRect(const QRectF& relativeRect)
{
    m_model->setAnalyticsSearchRect(relativeRect);
}

bool EventSearchWidget::Private::analyticsSearchByAreaEnabled() const
{
    return m_selectAreaCheckBox->isChecked();
}

void EventSearchWidget::Private::setAnalyticsSearchByAreaEnabled(bool value)
{
    m_selectAreaCheckBox->setChecked(value);
}

} // namespace
} // namespace client
} // namespace nx
