#include "unified_search_widget.h"
#include "ui_unified_search_widget.h"

#include <chrono>

#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>

#include <ui/common/custom_painted.h>
#include <ui/common/palette.h>
#include <ui/common/widget_anchor.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/search_line_edit.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/event_processors.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include <nx/client/desktop/event_search/models/unified_async_search_list_model.h>
#include <nx/utils/pending_operation.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr int kPlaceholderFontPixelSize = 15;
static constexpr int kQueuedFetchMoreDelayMs = 50;
static constexpr int kTimeSelectionDelayMs = 250;

QnSearchLineEdit* createSearchLineEdit(QWidget* parent)
{
    const auto paintFunction =
        [](QPainter* painter, const QStyleOption* option, const QWidget* widget) -> bool
        {
            if (option->state.testFlag(QStyle::State_HasFocus))
                painter->fillRect(option->rect, option->palette.dark());
            else if (option->state.testFlag(QStyle::State_MouseOver))
                painter->fillRect(option->rect, option->palette.midlight());

            painter->setPen(option->palette.color(QPalette::Light));
            painter->drawLine(option->rect.bottomLeft(), option->rect.bottomRight());
            return false;
        };

    auto result = new CustomPainted<QnSearchLineEdit>(parent);
    result->setCustomPaintFunction(paintFunction);
    result->setAttribute(Qt::WA_TranslucentBackground);
    result->setAttribute(Qt::WA_Hover);
    result->setAutoFillBackground(false);
    setPaletteColor(result, QPalette::Shadow, Qt::transparent);
    return result;
}

} // namespace

UnifiedSearchWidget::UnifiedSearchWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UnifiedSearchWidget()),
    m_searchLineEdit(createSearchLineEdit(this)),
    m_dayChangeTimer(new QTimer(this)),
    m_fetchMoreOperation(new utils::PendingOperation([this]() { fetchMoreIfNeeded(); },
        kQueuedFetchMoreDelayMs, this))
{
    ui->setupUi(this);
    ui->headerLayout->insertWidget(0, m_searchLineEdit);

    ui->headerWidget->setAutoFillBackground(true);

    ui->placeholder->setParent(ui->ribbonContainer);
    ui->placeholder->hide();
    new QnWidgetAnchor(ui->placeholder);

    QFont font;
    font.setPixelSize(kPlaceholderFontPixelSize);
    ui->placeholderText->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->placeholderText->setFont(font);
    ui->placeholderText->setForegroundRole(QPalette::Mid);

    ui->typeButton->hide();
    ui->areaButton->hide();
    ui->cameraButton->hide();

    ui->timeButton->setDeactivatable(true);
    ui->typeButton->setDeactivatable(true);
    ui->areaButton->setDeactivatable(true);
    ui->cameraButton->setDeactivatable(true);

    connect(ui->ribbon->scrollBar(), &QScrollBar::valueChanged,
        this, &UnifiedSearchWidget::fetchMoreIfNeeded, Qt::QueuedConnection);

    installEventHandler(ui->ribbon->scrollBar(), QEvent::Hide,
        this, &UnifiedSearchWidget::fetchMoreIfNeeded, Qt::QueuedConnection);

    installEventHandler(ui->ribbon, QEvent::Show,
        this, &UnifiedSearchWidget::fetchMoreIfNeeded);

    m_fetchMoreOperation->setFlags(utils::PendingOperation::FireOnlyWhenIdle);

    auto applyTimePeriod = new utils::PendingOperation([this]() { updateCurrentTimePeriod(); },
        kTimeSelectionDelayMs, this);

    applyTimePeriod->setFlags(utils::PendingOperation::FireOnlyWhenIdle);

    connect(navigator(), &QnWorkbenchNavigator::timeSelectionChanged, this,
        [this, applyTimePeriod](const QnTimePeriod& selection)
        {
            m_timelineSelection = selection;
            if (m_period == Period::selection)
                applyTimePeriod->requestOperation();
        });

    using namespace std::chrono;
    m_dayChangeTimer->setInterval(milliseconds(seconds(1)).count());

    connect(m_dayChangeTimer, &QTimer::timeout,
        this, &UnifiedSearchWidget::updateCurrentTimePeriod);

    setupTimeSelection();

    ui->areaButton->setIcon(qnSkin->icon(lit("text_buttons/area.png")));

    connect(ui->ribbon, &EventRibbon::countChanged,
        this, &UnifiedSearchWidget::updatePlaceholderState);
}

UnifiedSearchWidget::~UnifiedSearchWidget()
{
}

QAbstractListModel* UnifiedSearchWidget::model() const
{
    return ui->ribbon->model();
}

void UnifiedSearchWidget::setModel(QAbstractListModel* value)
{
    auto oldModel = model();

    if (oldModel == value)
        return;

    if (oldModel)
    {
        oldModel->disconnect(this);
        m_searchLineEdit->disconnect(oldModel);
    }

    ui->ribbon->setModel(value);

    connect(value, &QAbstractItemModel::rowsRemoved,
        this, &UnifiedSearchWidget::requestFetch, Qt::QueuedConnection);

    connect(value, &QAbstractItemModel::dataChanged,
        this, &UnifiedSearchWidget::updatePlaceholderState);

    if (auto asyncModel = qobject_cast<UnifiedAsyncSearchListModel*>(value))
    {
        connect(m_searchLineEdit, &QnSearchLineEdit::textChanged,
            asyncModel, &UnifiedAsyncSearchListModel::setTextFilter);
    }

    fetchMoreIfNeeded();
}

QnSearchLineEdit* UnifiedSearchWidget::filterEdit() const
{
    return m_searchLineEdit;
}

ui::SelectableTextButton* UnifiedSearchWidget::typeButton() const
{
    return ui->typeButton;
}

ui::SelectableTextButton* UnifiedSearchWidget::areaButton() const
{
    return ui->areaButton;
}

ui::SelectableTextButton* UnifiedSearchWidget::timeButton() const
{
    return ui->timeButton;
}

ui::SelectableTextButton* UnifiedSearchWidget::cameraButton() const
{
    return ui->cameraButton;
}

void UnifiedSearchWidget::setPlaceholderIcon(const QPixmap& value)
{
    ui->placeholderIcon->setPixmap(value);
}

void UnifiedSearchWidget::setPlaceholderTexts(
    const QString& constrained,
    const QString& unconstrained)
{
    m_placeholderTextConstrained = constrained;
    m_placeholderTextUnconstrained = unconstrained;
    updatePlaceholderState();
}

bool UnifiedSearchWidget::isConstrained() const
{
    auto asyncModel = qobject_cast<UnifiedAsyncSearchListModel*>(model());
    return asyncModel && asyncModel->isConstrained();
}

UnifiedSearchWidget::Period UnifiedSearchWidget::selectedPeriod() const
{
    return m_period;
}

void UnifiedSearchWidget::setSelectedPeriod(Period value)
{
    if (value == m_period)
        return;

    m_period = value;
    updateCurrentTimePeriod();

    if (m_period == Period::day || m_period == Period::week || m_period == Period::month)
        m_dayChangeTimer->start();
    else
        m_dayChangeTimer->stop();
}

void UnifiedSearchWidget::setCurrentTimePeriod(const QnTimePeriod& period)
{
    auto asyncModel = qobject_cast<UnifiedAsyncSearchListModel*>(model());
    if (!asyncModel)
        return;

    if (asyncModel->selectedTimePeriod() == period)
        return;

    asyncModel->setSelectedTimePeriod(period);
    requestFetch();
}

void UnifiedSearchWidget::updateCurrentTimePeriod()
{
    setCurrentTimePeriod(effectiveTimePeriod());
}

QnTimePeriod UnifiedSearchWidget::effectiveTimePeriod() const
{
    int days = 0;
    switch (m_period)
    {
        case Period::all:
            return QnTimePeriod(QnTimePeriod::kMinTimeValue, QnTimePeriod::infiniteDuration());

        case Period::selection:
            return m_timelineSelection;

        case Period::day:
            days = 1;
            break;

        case Period::week:
            days = 7;
            break;

        case Period::month:
            days = 30;
            break;

        default:
            NX_ASSERT(false);
            break;
    }

    auto current = qnSyncTime->currentDateTime();
    current.setTime(QTime(0, 0, 0, 0));
    return QnTimePeriod(current.addDays(1 - days).toMSecsSinceEpoch(),
        QnTimePeriod::infiniteDuration());
}

void UnifiedSearchWidget::setupTimeSelection()
{
    ui->timeButton->setIcon(qnSkin->icon(lit("text_buttons/rapid_review.png")));

    auto timeMenu = new QMenu(this);
    timeMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    timeMenu->setWindowFlags(timeMenu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    auto addMenuAction =
        [this, timeMenu](const QString& title, UnifiedSearchWidget::Period period)
        {
            auto action = timeMenu->addAction(title);
            connect(action, &QAction::triggered, this,
                [this, action, period]()
                {
                    ui->timeButton->setText(action->text());
                    ui->timeButton->setState(period == UnifiedSearchWidget::Period::all
                        ? ui::SelectableTextButton::State::deactivated
                        : ui::SelectableTextButton::State::unselected);

                    setSelectedPeriod(period);
                });

            return action;
        };

    auto defaultAction = addMenuAction(tr("Any time"), UnifiedSearchWidget::Period::all);
    addMenuAction(tr("Last day"), UnifiedSearchWidget::Period::day);
    addMenuAction(tr("Last 7 days"), UnifiedSearchWidget::Period::week);
    addMenuAction(tr("Last 30 days"), UnifiedSearchWidget::Period::month);
    addMenuAction(tr("Selected on Timeline"), UnifiedSearchWidget::Period::selection);

    connect(ui->timeButton, &ui::SelectableTextButton::stateChanged, this,
        [defaultAction](ui::SelectableTextButton::State state)
        {
            if (state == ui::SelectableTextButton::State::deactivated)
                defaultAction->trigger();
        });

    defaultAction->trigger();
    ui->timeButton->setMenu(timeMenu);
}

void UnifiedSearchWidget::requestFetch()
{
    m_fetchMoreOperation->requestOperation();
}

void UnifiedSearchWidget::fetchMoreIfNeeded()
{
    if (!ui->ribbon->isVisible() || !model())
        return;

    const auto scrollBar = ui->ribbon->scrollBar();
    if (scrollBar->isVisible() && scrollBar->value() < scrollBar->maximum())
        return;

    if (!model()->canFetchMore(QModelIndex()))
        return;

    model()->fetchMore(QModelIndex());
}

bool UnifiedSearchWidget::hasRelevantTiles() const
{
    const auto count = model() ? model()->rowCount() : 0;
    if (count == 0)
        return false;

    // Last tile is a busy indicator.
    return (count > 1) || model()->data(model()->index(0), Qn::BusyIndicatorVisibleRole).toBool();
}

void UnifiedSearchWidget::updatePlaceholderState()
{
    const bool hidden = hasRelevantTiles();
    if (ui->placeholder->isHidden() == hidden)
        return;

    if (!hidden)
    {
        ui->placeholderText->setText(isConstrained()
            ? m_placeholderTextConstrained
            : m_placeholderTextUnconstrained);
    }

    ui->placeholder->setHidden(hidden);
}

} // namespace desktop
} // namespace client
} // namespace nx
