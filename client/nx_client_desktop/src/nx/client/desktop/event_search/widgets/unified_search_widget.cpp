#include "unified_search_widget.h"
#include "ui_unified_search_widget.h"

#include <chrono>

#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>

#include <nx/client/desktop/common/utils/custom_painted.h>
#include <nx/client/desktop/common/utils/widget_anchor.h>
#include <nx/client/desktop/common/widgets/search_line_edit.h>
#include <nx/client/desktop/event_search/models/visual_search_list_model.h>
#include <nx/client/desktop/ui/common/color_theme.h>
#include <nx/client/desktop/utils/managed_camera_set.h>

#include <nx/utils/disconnect_helper.h>
#include <nx/utils/pending_operation.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

using namespace std::literals::chrono_literals;
using namespace std::chrono;

static constexpr int kPlaceholderFontPixelSize = 15;
static constexpr milliseconds kQueuedFetchMoreDelay = 50ms;
static constexpr milliseconds kTimeSelectionDelay = 250ms;
static constexpr milliseconds kTextFilterDelay = 250ms;

SearchLineEdit* createSearchLineEdit(QWidget* parent)
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

    auto result = new CustomPainted<SearchLineEdit>(parent);
    result->setGlassVisible(false);
    result->setCustomPaintFunction(paintFunction);
    result->setAttribute(Qt::WA_TranslucentBackground);
    result->setAttribute(Qt::WA_Hover);
    result->setAutoFillBackground(false);
    result->setTextChangedSignalFilterMs(kTextFilterDelay.count());
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
    m_fetchMoreOperation(new utils::PendingOperation(this))
{
    ui->setupUi(this);
    ui->headerLayout->insertWidget(0, m_searchLineEdit);

    ui->headerWidget->setAutoFillBackground(true);

    ui->placeholder->setParent(ui->ribbonContainer);
    ui->placeholder->hide();
    anchorWidgetToParent(ui->placeholder);

    QFont font;
    font.setPixelSize(kPlaceholderFontPixelSize);
    ui->placeholderText->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->placeholderText->setFont(font);
    ui->placeholderText->setForegroundRole(QPalette::Mid);

    ui->counterLabel->setForegroundRole(QPalette::Mid);

    ui->typeButton->hide();
    ui->areaButton->hide();

    ui->timeButton->setSelectable(false);
    ui->typeButton->setSelectable(false);
    ui->areaButton->setSelectable(false);
    ui->cameraButton->setSelectable(false);

    ui->timeButton->setDeactivatable(true);
    ui->typeButton->setDeactivatable(true);
    ui->areaButton->setDeactivatable(true);
    ui->cameraButton->setDeactivatable(true);

    setPaletteColor(ui->filterLine, QPalette::Shadow, colorTheme()->color("dark6"));

    connect(ui->ribbon->scrollBar(), &QScrollBar::valueChanged,
        this, &UnifiedSearchWidget::requestFetch, Qt::QueuedConnection);

    installEventHandler(this, QEvent::Show, this, &UnifiedSearchWidget::requestFetch);

    installEventHandler(this, {QEvent::Show, QEvent::Hide}, this,
        [this]()
        {
            if (m_model)
                m_model->setLivePaused(!isVisible());
        });

    m_fetchMoreOperation->setFlags(utils::PendingOperation::FireOnlyWhenIdle);
    m_fetchMoreOperation->setIntervalMs(kQueuedFetchMoreDelay.count());
    m_fetchMoreOperation->setCallback(
        [this]()
        {
            if (model() && model()->canFetchMore())
                model()->fetchMore();
        });

    auto applyTimePeriod = new utils::PendingOperation([this]() { updateCurrentTimePeriod(); },
        kTimeSelectionDelay.count(), this);

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
    setupCameraSelection();

    ui->areaButton->setIcon(qnSkin->icon(lit("text_buttons/area.png")));

    connect(ui->ribbon, &EventRibbon::tileHovered, this, &UnifiedSearchWidget::tileHovered);

    ui->showInfoButton->hide();
    ui->showPreviewsButton->hide();
    ui->showInfoButton->setChecked(ui->ribbon->footersEnabled());
    ui->showPreviewsButton->setChecked(ui->ribbon->previewsEnabled());
    ui->showInfoButton->setDrawnBackgrounds(ToolButton::ActiveBackgrounds);
    ui->showPreviewsButton->setDrawnBackgrounds(ToolButton::ActiveBackgrounds);
    ui->showInfoButton->setIcon(qnSkin->icon(
        "text_buttons/text.png", "text_buttons/text_selected.png"));
    ui->showPreviewsButton->setIcon(qnSkin->icon(
        "text_buttons/image.png", "text_buttons/image_selected.png"));

    const auto updateInformationToolTip =
        [this]()
        {
            ui->showInfoButton->setToolTip(ui->showInfoButton->isChecked()
                ? tr("Hide information")
                : tr("Show information"));
        };

    const auto updateThumbnailsToolTip =
        [this]()
        {
            ui->showPreviewsButton->setToolTip(ui->showPreviewsButton->isChecked()
                ? tr("Hide thumbnails")
                : tr("Show thumbnails"));
        };

    updateInformationToolTip();
    updateThumbnailsToolTip();

    connect(ui->showInfoButton, &QToolButton::toggled,
        ui->ribbon, &EventRibbon::setFootersEnabled);
    connect(ui->showInfoButton, &QToolButton::toggled,
        this, updateInformationToolTip);
    connect(ui->showPreviewsButton, &QToolButton::toggled,
        ui->ribbon, &EventRibbon::setPreviewsEnabled);
    connect(ui->showPreviewsButton, &QToolButton::toggled,
        this, updateThumbnailsToolTip);

    ui->ribbon->setViewportMargins(0, style::Metrics::kStandardPadding);

    ui->ribbon->scrollBar()->ensurePolished();
    setPaletteColor(ui->ribbon->scrollBar(), QPalette::Disabled, QPalette::Midlight,
        colorTheme()->color("dark5"));

    const auto updaterFor =
        [this](Cameras mode)
        {
            const auto updater =
                [this, mode]()
                {
                    if (m_cameras == mode)
                        updateCurrentCameras();
                };

            return updater;
        };

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, updaterFor(Cameras::current));

    connect(workbench(), &QnWorkbench::currentLayoutChanged, this, updaterFor(Cameras::layout));
    connect(workbench(), &QnWorkbench::currentLayoutItemsChanged, this, updaterFor(Cameras::layout));
}

UnifiedSearchWidget::~UnifiedSearchWidget()
{
    m_modelConnections.reset();
}

VisualSearchListModel* UnifiedSearchWidget::model() const
{
    return m_model;
}

void UnifiedSearchWidget::setModel(VisualSearchListModel* value)
{
    if (m_model == value)
        return;

    m_modelConnections.reset();

    m_model = value;
    ui->ribbon->setModel(value);

    if (!value)
        return;

    m_modelConnections.reset(new QnDisconnectHelper());

    *m_modelConnections << connect(value, &VisualSearchListModel::camerasChanged,
        this, &UnifiedSearchWidget::requestFetch);

    *m_modelConnections << connect(value, &QAbstractItemModel::modelReset,
        this, &UnifiedSearchWidget::updatePlaceholderState);

    *m_modelConnections << connect(value, &QAbstractItemModel::rowsRemoved,
        this, &UnifiedSearchWidget::updatePlaceholderState);

    *m_modelConnections << connect(value, &QAbstractItemModel::rowsInserted,
        this, &UnifiedSearchWidget::updatePlaceholderState);

    *m_modelConnections << connect(value, &VisualSearchListModel::liveChanged,
        ui->ribbon, &EventRibbon::setLive);

    *m_modelConnections << connect(value, &VisualSearchListModel::isOnlineChanged, this,
        [this](bool isOnline)
        {
            if (isOnline)
                updateCurrentCameras();
        });

    // For busy indicator going on/off.
    *m_modelConnections << connect(value, &QAbstractItemModel::dataChanged,
        this, &UnifiedSearchWidget::updatePlaceholderState);

    updateCurrentCameras();
}

SearchLineEdit* UnifiedSearchWidget::filterEdit() const
{
    return m_searchLineEdit;
}

SelectableTextButton* UnifiedSearchWidget::typeButton() const
{
    return ui->typeButton;
}

SelectableTextButton* UnifiedSearchWidget::areaButton() const
{
    return ui->areaButton;
}

SelectableTextButton* UnifiedSearchWidget::timeButton() const
{
    return ui->timeButton;
}

SelectableTextButton* UnifiedSearchWidget::cameraButton() const
{
    return ui->cameraButton;
}

QToolButton* UnifiedSearchWidget::showInfoButton() const
{
    return ui->showInfoButton;
}

QToolButton* UnifiedSearchWidget::showPreviewsButton() const
{
    return ui->showPreviewsButton;
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

QnTimePeriod UnifiedSearchWidget::currentTimePeriod() const
{
    return m_currentTimePeriod;
}

void UnifiedSearchWidget::updateCurrentTimePeriod()
{
    const auto newTimePeriod = effectiveTimePeriod();
    if (m_currentTimePeriod == newTimePeriod)
        return;

    m_currentTimePeriod = newTimePeriod;
    m_model->setRelevantTimePeriod(m_currentTimePeriod);
    requestFetch();

    emit currentTimePeriodChanged(m_currentTimePeriod);
}

QnTimePeriod UnifiedSearchWidget::effectiveTimePeriod() const
{
    int days = 0;
    switch (m_period)
    {
        case Period::all:
            return QnTimePeriod::anytime();

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
                        ? SelectableTextButton::State::deactivated
                        : SelectableTextButton::State::unselected);

                    setSelectedPeriod(period);
                });

            return action;
        };

    auto defaultAction = addMenuAction(tr("Any time"), UnifiedSearchWidget::Period::all);
    addMenuAction(tr("Last day"), UnifiedSearchWidget::Period::day);
    addMenuAction(tr("Last 7 days"), UnifiedSearchWidget::Period::week);
    addMenuAction(tr("Last 30 days"), UnifiedSearchWidget::Period::month);
    addMenuAction(tr("Selected on Timeline"), UnifiedSearchWidget::Period::selection);

    connect(ui->timeButton, &SelectableTextButton::stateChanged, this,
        [defaultAction](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                defaultAction->trigger();
        });

    defaultAction->trigger();
    ui->timeButton->setMenu(timeMenu);
}

void UnifiedSearchWidget::setupCameraSelection()
{
    ui->cameraButton->setIcon(qnSkin->icon(lit("text_buttons/camera.png")));

    auto cameraMenu = new QMenu(this);
    cameraMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    cameraMenu->setWindowFlags(cameraMenu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    auto addMenuAction =
        [this, cameraMenu](const QString& title, UnifiedSearchWidget::Cameras cameras)
        {
            auto action = cameraMenu->addAction(title);
            connect(action, &QAction::triggered, this,
                [this, action, cameras]()
                {
                    ui->cameraButton->setText(action->text());
                    ui->cameraButton->setState(cameras == UnifiedSearchWidget::Cameras::all
                        ? SelectableTextButton::State::deactivated
                        : SelectableTextButton::State::unselected);

                    setSelectedCameraSet(cameras);
                });

            return action;
        };

    auto defaultAction = addMenuAction(tr("All cameras"), UnifiedSearchWidget::Cameras::all);
    addMenuAction(tr("Cameras on layout"), UnifiedSearchWidget::Cameras::layout);
    addMenuAction(tr("Current camera"), UnifiedSearchWidget::Cameras::current);

    connect(ui->cameraButton, &SelectableTextButton::stateChanged, this,
        [defaultAction](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                defaultAction->trigger();
        });

    defaultAction->trigger();
    ui->cameraButton->setMenu(cameraMenu);
}

UnifiedSearchWidget::Cameras UnifiedSearchWidget::selectedCameraSet() const
{
    return m_cameras;
}

void UnifiedSearchWidget::setSelectedCameraSet(Cameras value)
{
    if (value == m_cameras)
        return;

    m_cameras = value;
    updateCurrentCameras();
}

QnVirtualCameraResourceSet UnifiedSearchWidget::currentCameras() const
{
    return m_currentCameras;
}

void UnifiedSearchWidget::updateCurrentCameras()
{
    switch (m_cameras)
    {
        case Cameras::all:
            m_model->cameraSet()->setAllCameras();
            break;

        case Cameras::current:
            m_model->cameraSet()->setSingleCamera(
                navigator()->currentResource().dynamicCast<QnVirtualCameraResource>());
            break;

        case Cameras::layout:
        {
            QnVirtualCameraResourceSet cameras;
            if (auto workbenchLayout = workbench()->currentLayout())
            {
                for (const auto& item: workbenchLayout->items())
                {
                    if (const auto camera = item->resource().dynamicCast<QnVirtualCameraResource>())
                        cameras.insert(camera);
                }
            }

            m_model->cameraSet()->setMultipleCameras(cameras);
            break;
        }

        default:
            NX_ASSERT(false);
    }
}

void UnifiedSearchWidget::requestFetch()
{
    if (!ui->ribbon->isVisible() || !model())
        return;

    const auto scrollBar = ui->ribbon->scrollBar();

    if (model()->relevantCount() == 0 || scrollBar->value() == scrollBar->maximum())
        model()->setFetchDirection(AbstractSearchListModel::FetchDirection::earlier);
    else if (scrollBar->value() == scrollBar->minimum())
        model()->setFetchDirection(AbstractSearchListModel::FetchDirection::later);
    else
        return; //< Scroll bar is not at the beginning nor the end.

    m_fetchMoreOperation->requestOperation();
}

void UnifiedSearchWidget::updatePlaceholderState()
{
    const bool placeholderVisible = !m_model || m_model->relevantCount() == 0;
    if (placeholderVisible)
    {
        ui->placeholderText->setText(m_model && m_model->isConstrained()
            ? m_placeholderTextConstrained
            : m_placeholderTextUnconstrained);
    }

    ui->placeholder->setVisible(placeholderVisible);
    ui->counterContainer->setVisible(!placeholderVisible);
}

QLabel* UnifiedSearchWidget::counterLabel() const
{
    return ui->counterLabel;
}

} // namespace desktop
} // namespace client
} // namespace nx
