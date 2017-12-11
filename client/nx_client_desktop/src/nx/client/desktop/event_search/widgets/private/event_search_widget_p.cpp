#include "event_search_widget_p.h"

#include <chrono>

#include <QtWidgets/QLayout>
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

#include <nx/client/desktop/common/models/subset_list_model.h>
#include <nx/client/desktop/event_search/models/unified_search_list_model.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static const auto kFilterDelay = std::chrono::milliseconds(250);
static const auto kTextButtonHeight = 24;

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
    m_searchLineEdit(new QnSearchLineEdit(m_headerWidget)),
    m_superTypeButton(new QPushButton(m_headerWidget)),
    m_eventTypeButton(new QPushButton(m_headerWidget)),
    m_selectAreaCheckBox(new QCheckBox(m_headerWidget)),
    m_helper(new vms::event::StringsHelper(commonModule()))
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

    auto layout = m_headerWidget->layout();
    layout->addWidget(m_searchLineEdit);
    layout->addWidget(buttonsHolder);

    auto sortModel = new EventSortFilterModel(this);
    sortModel->setSourceModel(m_model);
    sortModel->setFilterRole(Qn::ResourceSearchStringRole);
    sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    sortModel->setTriggeringRoles({Qn::TimestampRole});

    connect(m_searchLineEdit, &QnSearchLineEdit::textChanged,
        sortModel, &EventSortFilterModel::setFilterWildcard);

    setModel(new SubsetListModel(sortModel, 0, QModelIndex(), this));
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

    for (auto w = m_eventTypeButton->parentWidget(); w != nullptr; w = w->parentWidget())
    {
        if (w->layout())
            w->layout()->activate();
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
