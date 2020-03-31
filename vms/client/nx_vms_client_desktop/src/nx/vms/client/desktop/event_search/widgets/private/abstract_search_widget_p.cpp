#include "abstract_search_widget_p.h"
#include "tile_interaction_handler_p.h"
#include "ui_abstract_search_widget.h"

#include <chrono>

#include <QtCore/QTimer>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QScrollBar>

#include <nx/vms/client/desktop/ini.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <ui/common/palette.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>

#include <nx/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/common/models/concatenation_list_model.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/search_line_edit.h>
#include <nx/vms/client/desktop/event_search/models/private/busy_indicator_model_p.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/utils/managed_camera_set.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/pending_operation.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

namespace {

static constexpr int kPlaceholderFontPixelSize = 15;
static constexpr int kCounterPanelHeight = 32;

static constexpr milliseconds kQueuedFetchMoreDelay = 50ms;
static constexpr milliseconds kTimeSelectionDelay = 250ms;
static constexpr milliseconds kTextFilterDelay = 250ms;

static constexpr milliseconds kPlaceholderFadeDuration = 150ms;
static constexpr auto kPlaceholderFadeEasing = QEasingCurve::InOutQuad;

SearchLineEdit* createSearchLineEdit(QWidget* parent)
{
    const auto paintFunction =
        [](QPainter* painter, const QStyleOption* option, const QWidget* /*widget*/) -> bool
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
    result->setFocusPolicy(Qt::ClickFocus);
    setPaletteColor(result, QPalette::Shadow, Qt::transparent);
    return result;
}

QToolButton* createCheckableToolButton(QWidget* parent)
{
    const auto paintFunction =
        [](QPainter* painter, const QStyleOption* option, const QWidget* widget) -> bool
        {
            const auto button = qobject_cast<const QToolButton*>(widget);
            if (!NX_ASSERT(button))
                return false;

            const bool hovered = option->state.testFlag(QStyle::State_MouseOver);

            const auto mode = hovered && !button->isDown() ? QIcon::Active : QIcon::Normal;
            const auto state = button->isChecked() ? QIcon::On : QIcon::Off;
            const auto iconSize = qnSkin->maximumSize(button->icon(), mode, state);

            const auto iconRect = QStyle::alignedRect(
                Qt::LeftToRight, Qt::AlignCenter, iconSize, option->rect);

            button->icon().paint(painter, iconRect, 0, mode, state);
            return true;
        };

    auto result = new CustomPainted<QToolButton>(parent);
    result->setCustomPaintFunction(paintFunction);
    result->setCheckable(true);
    result->setFocusPolicy(Qt::NoFocus);
    return result;
}

} // namespace

AbstractSearchWidget::Private::Private(
    AbstractSearchWidget* q,
    AbstractSearchListModel* model)
    :
    QnWorkbenchContextAware(q),
    q(q),
    ui(new Ui::AbstractSearchWidget()),
    m_mainModel(model),
    m_headIndicatorModel(new BusyIndicatorModel()),
    m_tailIndicatorModel(new BusyIndicatorModel()),
    m_visualModel(new ConcatenationListModel()),
    m_togglePreviewsButton(createCheckableToolButton(q)),
    m_toggleFootersButton(createCheckableToolButton(q)),
    m_itemCounterLabel(new QLabel(q)),
    m_textFilterEdit(createSearchLineEdit(q)),
    m_dayChangeTimer(new QTimer()),
    m_fetchMoreOperation(new nx::utils::PendingOperation())
{
    NX_CRITICAL(model, "Model must be specified.");
    m_mainModel->setParent(nullptr); //< Stored as a scoped pointer.

    ui->setupUi(q);

    setPaletteColor(ui->separatorLine, QPalette::Shadow, colorTheme()->color("dark6"));

    ui->headerWidget->setAutoFillBackground(true);
    ui->headerLayout->insertWidget(0, m_textFilterEdit);
    connect(m_textFilterEdit, &SearchLineEdit::textChanged, this,
        [this](const QString& value) { emit this->q->textFilterChanged(value, {}); });

    setupModels();
    setupRibbon();
    setupPlaceholder();
    setupTimeSelection();
    setupCameraSelection();

    installEventHandler(q, {QEvent::Show, QEvent::Hide}, this,
        [this, q]()
        {
            const bool hidden = !q->isVisible();
            m_mainModel->setLivePaused(hidden);
            if (hidden)
                return;

            q->setFocus();
            requestFetchIfNeeded();
        });

    q->setFocusPolicy(Qt::ClickFocus);

    m_fetchMoreOperation->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
    m_fetchMoreOperation->setInterval(kQueuedFetchMoreDelay);
    m_fetchMoreOperation->setCallback([this]() { tryFetchMore(); });
}

AbstractSearchWidget::Private::~Private()
{
    // Required here for forward-declared scoped pointer destruction.
}

void AbstractSearchWidget::Private::setupModels()
{
    connect(m_mainModel.data(), &AbstractSearchListModel::dataNeeded,
        this, &Private::requestFetchIfNeeded);

    connect(m_mainModel.data(), &QAbstractItemModel::modelReset,
        this, &Private::handleItemCountChanged);

    connect(m_mainModel.data(), &QAbstractItemModel::rowsRemoved,
        this, &Private::handleItemCountChanged);

    connect(m_mainModel.data(), &QAbstractItemModel::rowsInserted,
        this, &Private::handleItemCountChanged);

    connect(m_mainModel.data(), &AbstractSearchListModel::liveAboutToBeCommitted,
        this, &Private::updateFetchDirection);

    using FetchDirection = AbstractSearchListModel::FetchDirection;
    using UpdateMode = EventRibbon::UpdateMode;

    connect(m_mainModel.data(), &QAbstractItemModel::modelAboutToBeReset, this,
        [this]() { ui->ribbon->setRemovalMode(UpdateMode::instant); });

    connect(m_mainModel.data(), &QAbstractItemModel::rowsAboutToBeRemoved,
        [this]() { ui->ribbon->setRemovalMode(UpdateMode::animated); });

    connect(m_mainModel.data(), &AbstractSearchListModel::fetchCommitStarted, this,
        [this](FetchDirection direction)
        {
            ui->ribbon->setInsertionMode(UpdateMode::instant, direction == FetchDirection::later);
        });

    connect(m_mainModel.data(), &AbstractSearchListModel::fetchFinished, this,
        [this]()
        {
            ui->ribbon->setInsertionMode(UpdateMode::animated, false);
            handleFetchFinished();
        });

    connect(m_mainModel.data(), &AbstractSearchListModel::liveChanged, this,
        [this](bool isLive)
        {
            if (isLive)
                m_headIndicatorModel->setVisible(false);
        });

    connect(m_mainModel.data(), &AbstractSearchListModel::camerasChanged, this,
        [this]() { emit q->cameraSetChanged(m_mainModel->cameras(), {}); });

    connect(m_mainModel.data(), &AbstractSearchListModel::isOnlineChanged, this,
        [this](bool isOnline)
        {
            if (isOnline)
            {
                updateCurrentCameras();
                updateDeviceDependentActions();
            }
            else
            {
                q->resetFilters();
            }
        });

    if (m_mainModel->isOnline())
        executeLater([this]() { updateDeviceDependentActions(); }, this);

    m_headIndicatorModel->setObjectName("head");
    m_tailIndicatorModel->setObjectName("tail");

    m_headIndicatorModel->setVisible(false);
    m_tailIndicatorModel->setVisible(true);

    m_visualModel->setModels({
        m_headIndicatorModel.data(), m_mainModel.data(), m_tailIndicatorModel.data()});
}

void AbstractSearchWidget::Private::setupRibbon()
{
    ui->ribbon->setModel(m_visualModel.data());
    ui->ribbon->setViewportMargins(0, style::Metrics::kStandardPadding);
    ui->ribbon->setFocusPolicy(Qt::NoFocus);
    ui->ribbon->scrollBar()->ensurePolished();
    setPaletteColor(ui->ribbon->scrollBar(), QPalette::Disabled, QPalette::Midlight,
        colorTheme()->color("dark5"));

    setupViewportHeader();

    connect(ui->ribbon->scrollBar(), &QScrollBar::valueChanged,
        this, &Private::requestFetchIfNeeded, Qt::QueuedConnection);

    connect(ui->ribbon, &EventRibbon::hovered, this,
        [this](const QModelIndex& idx, EventTile* tile) { emit q->tileHovered(idx, tile, {}); });

    TileInteractionHandler::install(ui->ribbon);
}

void AbstractSearchWidget::Private::setupViewportHeader()
{
    const auto toolbar = new QWidget(q);
    toolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    const auto toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setSpacing(style::Metrics::kStandardPadding);
    toolbarLayout->setContentsMargins(0, 2, 0, 0); //< Fine-tuned.
    toolbarLayout->addWidget(m_toggleFootersButton);
    toolbarLayout->addWidget(m_togglePreviewsButton);

    const auto viewportHeader = new QWidget(q);
    viewportHeader->setFixedHeight(kCounterPanelHeight);

    const auto headerLayout = new QHBoxLayout(viewportHeader);
    headerLayout->setSpacing(style::Metrics::kStandardPadding);
    headerLayout->setContentsMargins({});

    headerLayout->addWidget(m_itemCounterLabel);
    headerLayout->addWidget(toolbar);

    ui->ribbon->setViewportHeader(viewportHeader);

    m_itemCounterLabel->setForegroundRole(QPalette::Mid);
    m_itemCounterLabel->setText({});

    m_toggleFootersButton->setChecked(ui->ribbon->footersEnabled());
    m_toggleFootersButton->setIcon(qnSkin->icon(
        "text_buttons/text.png", "text_buttons/text_selected.png"));

    const auto updateInformationToolTip =
        [this]()
        {
            m_toggleFootersButton->setToolTip(m_toggleFootersButton->isChecked()
                ? tr("Hide information")
                : tr("Show information"));
        };

    connect(m_toggleFootersButton, &QToolButton::toggled,
        ui->ribbon, &EventRibbon::setFootersEnabled);

    connect(m_toggleFootersButton, &QToolButton::toggled,
        this, updateInformationToolTip);

    updateInformationToolTip();

    m_togglePreviewsButton->setChecked(ui->ribbon->previewsEnabled());
    m_togglePreviewsButton->setIcon(qnSkin->icon(
        "text_buttons/image.png", "text_buttons/image_selected.png"));

    const auto updateThumbnailsToolTip =
        [this]()
        {
            m_togglePreviewsButton->setToolTip(m_togglePreviewsButton->isChecked()
                ? tr("Hide thumbnails")
                : tr("Show thumbnails"));
        };

    connect(m_togglePreviewsButton, &QToolButton::toggled,
        ui->ribbon, &EventRibbon::setPreviewsEnabled);

    connect(m_togglePreviewsButton, &QToolButton::toggled,
        this, updateThumbnailsToolTip);

    updateThumbnailsToolTip();
}

void AbstractSearchWidget::Private::setupPlaceholder()
{
    ui->placeholder->setParent(ui->ribbonContainer);
    anchorWidgetToParent(ui->placeholder);

    auto effect = new QGraphicsOpacityEffect(ui->placeholder);
    ui->placeholder->setGraphicsEffect(effect);

    connect(effect, &QGraphicsOpacityEffect::opacityChanged, this,
        [this](qreal opacity)
        {
            ui->placeholder->setHidden(qFuzzyIsNull(opacity));
            ui->placeholder->graphicsEffect()->setEnabled(!qFuzzyIsNull(opacity - 1.0));
        });

    m_placeholderVisible = false;
    effect->setOpacity(0.0);

    QFont font;
    font.setPixelSize(kPlaceholderFontPixelSize);
    ui->placeholderText->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->placeholderText->setFont(font);
    ui->placeholderText->setForegroundRole(QPalette::Mid);
}

void AbstractSearchWidget::Private::setupTimeSelection()
{
    ui->timeSelectionButton->setSelectable(false);
    ui->timeSelectionButton->setDeactivatable(true);
    ui->timeSelectionButton->setIcon(qnSkin->icon("text_buttons/calendar.png"));
    ui->timeSelectionButton->setFocusPolicy(Qt::NoFocus);

    auto timeMenu = q->createDropdownMenu();
    auto addMenuAction =
        [this, timeMenu](const QString& title, Period period)
        {
            auto action = timeMenu->addAction(title);
            connect(action, &QAction::triggered, this,
                [this, period]()
                {
                    ui->timeSelectionButton->setText(m_timeSelectionActions[period]->text());
                    ui->timeSelectionButton->setState(period == Period::all
                        ? SelectableTextButton::State::deactivated
                        : SelectableTextButton::State::unselected);

                    ui->timeSelectionButton->setAccented(period == Period::selection);
                    setSelectedPeriod(period);

                    if (period != Period::selection)
                        navigator()->clearTimelineSelection();
                });

            m_timeSelectionActions[period] = action;
            return action;
        };

    addMenuAction(tr("Last day"), Period::day);
    addMenuAction(tr("Last 7 days"), Period::week);
    addMenuAction(tr("Last 30 days"), Period::month);
    addMenuAction(tr("Selected on Timeline"), Period::selection);
    timeMenu->addSeparator();
    addMenuAction(tr("Any time"), Period::all);

    connect(ui->timeSelectionButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                m_timeSelectionActions[Period::all]->trigger();
        });

    m_timeSelectionActions[Period::all]->trigger();
    ui->timeSelectionButton->setMenu(timeMenu);

    m_timeSelectionActions[Period::selection]->setVisible(false);

    // Setup timeline selection watcher.

    auto applyTimePeriod = new nx::utils::PendingOperation(this);
    applyTimePeriod->setInterval(kTimeSelectionDelay);
    applyTimePeriod->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
    applyTimePeriod->setCallback(
        [this]()
        {
            const bool selectionExists = !m_timelineSelection.isNull();
            const bool selectionFilter = m_period == Period::selection;

            if (selectionExists != selectionFilter)
            {
                const auto action = selectionExists
                    ? m_timeSelectionActions[Period::selection]
                    : m_timeSelectionActions[m_previousPeriod];

                action->trigger();
                return;
            }

            if (m_period == Period::selection)
                updateCurrentTimePeriod();
        });

    connect(navigator(), &QnWorkbenchNavigator::timeSelectionChanged, this,
        [this, applyTimePeriod](const QnTimePeriod& selection)
        {
            m_timelineSelection = selection;

            // If selection was cleared, update immediately, otherwise update after small delay.
            if (m_timelineSelection.isNull())
                applyTimePeriod->fire();
            else
                applyTimePeriod->requestOperation();
        });

    // Setup day change watcher.

    m_dayChangeTimer->start(1s);

    connect(m_dayChangeTimer.data(), &QTimer::timeout, this,
        [this]()
        {
            const auto timeWatcher = context()->instance<nx::vms::client::core::ServerTimeWatcher>();
            setCurrentDate(timeWatcher->displayTime(qnSyncTime->currentMSecsSinceEpoch()));
            updateCurrentTimePeriod();
        });
}

void AbstractSearchWidget::Private::setupCameraSelection()
{
    ui->cameraSelectionButton->setSelectable(false);
    ui->cameraSelectionButton->setDeactivatable(true);
    ui->cameraSelectionButton->setIcon(qnSkin->icon("text_buttons/camera.png"));
    ui->cameraSelectionButton->setFocusPolicy(Qt::NoFocus);

    auto cameraMenu = q->createDropdownMenu();
    auto addMenuAction =
        [this, cameraMenu](const QString& title, Cameras cameras, bool dynamicTitle = false)
        {
            const auto updateButtonText =
                [this](Cameras cameras)
                {
                    const auto text = m_cameraSelectionActions[cameras]->text();
                    ui->cameraSelectionButton->setText(cameras == Cameras::current
                        ? currentDeviceText()
                        : text);
                };

            auto action = cameraMenu->addAction(title);
            connect(action, &QAction::triggered, this,
                [this, cameras, updateButtonText]()
                {
                    updateButtonText(cameras);
                    ui->cameraSelectionButton->setState(cameras == Cameras::all
                        ? SelectableTextButton::State::deactivated
                        : SelectableTextButton::State::unselected);

                    ui->cameraSelectionButton->setAccented(cameras == Cameras::current);
                    setSelectedCameras(cameras);
                });

            if (dynamicTitle)
            {
                connect(action, &QAction::changed, this,
                    [this, cameras, updateButtonText]()
                    {
                        if (selectedCameras() == cameras)
                            updateButtonText(cameras);
                    });
            }

            m_cameraSelectionActions[cameras] = action;
            return action;
        };

    addDeviceDependentAction(addMenuAction("<cameras on layout>", Cameras::layout, true),
        tr("Devices on layout"), tr("Cameras on layout"));

    addDeviceDependentAction(addMenuAction("<current camera>", Cameras::current, true),
        tr("Selected device"), tr("Selected camera"));

    cameraMenu->addSeparator();

    addDeviceDependentAction(addMenuAction("<any camera>", Cameras::all, true),
        tr("Any device"), tr("Any camera"));

    connect(ui->cameraSelectionButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                m_cameraSelectionActions[Cameras::all]->trigger();
        });

    m_cameraSelectionActions[Cameras::all]->trigger();
    ui->cameraSelectionButton->setMenu(cameraMenu);

    updateCurrentCameras();

    // Setup camera watchers.

    const auto camerasUpdaterFor =
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
        this, camerasUpdaterFor(Cameras::current));

    connect(workbench(), &QnWorkbench::currentLayoutChanged,
        this, camerasUpdaterFor(Cameras::layout));

    connect(workbench(), &QnWorkbench::currentLayoutItemsChanged,
        this, camerasUpdaterFor(Cameras::layout));

    // Current camera name watchers.

    connect(q, &AbstractSearchWidget::cameraSetChanged, this,
        [this]()
        {
            disconnect(m_currentCameraConnection);

            if (m_cameras != Cameras::current)
                return;

            const auto updateCurrentCameraName =
                [this]() { m_cameraSelectionActions[Cameras::current]->trigger(); };

            updateCurrentCameraName();

            const auto camera = m_mainModel->cameraSet()->singleCamera();
            if (!camera)
                return;

            m_currentCameraConnection = connect(camera.data(), &QnResource::nameChanged,
                this, updateCurrentCameraName);
        });
}

QString AbstractSearchWidget::Private::currentDeviceText() const
{
    static const auto kTemplate = QString::fromWCharArray(L"%1 \x2013 %2");

    const auto camera = m_mainModel->cameraSet()->singleCamera();
    if (!camera)
        return kTemplate.arg(tr("Selected camera"), tr("none", "No currently selected camera"));

    const auto baseText = QnDeviceDependentStrings::getNameFromSet(q->resourcePool(),
        QnCameraDeviceStringSet("<unused>", tr("Selected camera"), tr("Selected device")), camera);

    return kTemplate.arg(baseText, camera->getName());
};

AbstractSearchListModel* AbstractSearchWidget::Private::model() const
{
    return m_mainModel.data();
}

EventRibbon* AbstractSearchWidget::Private::view() const
{
    return ui->ribbon;
}

bool AbstractSearchWidget::Private::isAllowed() const
{
    if (!m_isAllowed.has_value())
        q->updateAllowance();

    NX_ASSERT(m_isAllowed.has_value());
    return *m_isAllowed;
}

void AbstractSearchWidget::Private::setAllowed(bool value)
{
    if (m_isAllowed.has_value() && *m_isAllowed == value)
        return;

    m_isAllowed = value;
    emit q->allowanceChanged(*m_isAllowed, {});
}

void AbstractSearchWidget::Private::goToLive()
{
    if (m_mainModel->effectiveLiveSupported())
        m_mainModel->setLive(true);

    ui->ribbon->scrollBar()->setValue(0);
}

AbstractSearchWidget::Controls AbstractSearchWidget::Private::relevantControls() const
{
    Controls result;
    result.setFlag(Control::cameraSelector, !ui->cameraSelectionButton->isHidden());
    result.setFlag(Control::timeSelector, !ui->timeSelectionButton->isHidden());
    result.setFlag(Control::freeTextFilter, !m_textFilterEdit->isHidden());
    result.setFlag(Control::footersToggler, !m_toggleFootersButton->isHidden());
    result.setFlag(Control::previewsToggler, !m_togglePreviewsButton->isHidden());
    return result;
}

void AbstractSearchWidget::Private::setRelevantControls(Controls value)
{
    ui->cameraSelectionButton->setVisible(value.testFlag(Control::cameraSelector));
    ui->timeSelectionButton->setVisible(value.testFlag(Control::timeSelector));
    m_textFilterEdit->setVisible(value.testFlag(Control::freeTextFilter));
    m_toggleFootersButton->setVisible(value.testFlag(Control::footersToggler));
    m_togglePreviewsButton->setVisible(value.testFlag(Control::previewsToggler));
}

AbstractSearchWidget::Period AbstractSearchWidget::Private::selectedPeriod() const
{
    return m_period;
}

void AbstractSearchWidget::Private::setSelectedPeriod(Period value)
{
    if (value == m_period)
        return;

    m_previousPeriod = m_period;
    m_period = value;
    updateCurrentTimePeriod();
}

QnTimePeriod AbstractSearchWidget::Private::currentTimePeriod() const
{
    return m_currentTimePeriod;
}

void AbstractSearchWidget::Private::updateCurrentTimePeriod()
{
    const auto newTimePeriod = effectiveTimePeriod();
    if (m_currentTimePeriod == newTimePeriod)
        return;

    m_currentTimePeriod = newTimePeriod;
    m_mainModel->setRelevantTimePeriod(m_currentTimePeriod);

    emit q->timePeriodChanged(m_currentTimePeriod, {});
}

QnTimePeriod AbstractSearchWidget::Private::effectiveTimePeriod() const
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

    return QnTimePeriod(m_currentDate.addDays(1 - days).toMSecsSinceEpoch(),
        QnTimePeriod::kInfiniteDuration);
}

AbstractSearchWidget::Cameras AbstractSearchWidget::Private::selectedCameras() const
{
    return m_cameras;
}

void AbstractSearchWidget::Private::selectCameras(Cameras value)
{
    if (m_cameras == value)
        return;

    const auto action = m_cameraSelectionActions.value(value);
    NX_ASSERT(action);
    if (action)
        action->trigger();
}

AbstractSearchWidget::Cameras AbstractSearchWidget::Private::previousCameras() const
{
    return m_previousCameras;
}

void AbstractSearchWidget::Private::setSelectedCameras(Cameras value)
{
    if (value == m_cameras)
        return;

    m_previousCameras = m_cameras;
    m_cameras = value;
    updateCurrentCameras();
}

QnVirtualCameraResourceSet AbstractSearchWidget::Private::cameras() const
{
    return m_mainModel->cameraSet()->cameras();
}

QString AbstractSearchWidget::Private::textFilter() const
{
    return m_textFilterEdit->text();
}

void AbstractSearchWidget::Private::setPlaceholderPixmap(const QPixmap& value)
{
    ui->placeholderIcon->setPixmap(value);
}

SelectableTextButton* AbstractSearchWidget::Private::createCustomFilterButton()
{
    auto result = new SelectableTextButton(ui->filters);
    result->setFlat(true);
    result->setDeactivatable(true);
    result->setSelectable(false);
    result->setFocusPolicy(Qt::NoFocus);
    result->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    ui->filtersLayout->addWidget(result, 0, Qt::AlignLeft);
    return result;
}

void AbstractSearchWidget::Private::updateCurrentCameras()
{
    if (!m_mainModel->isOnline())
        return;

    switch (m_cameras)
    {
        case Cameras::all:
            m_mainModel->cameraSet()->setAllCameras();
            break;

        case Cameras::current:
            m_mainModel->cameraSet()->setSingleCamera(
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

            m_mainModel->cameraSet()->setMultipleCameras(cameras);
            break;
        }

        default:
            NX_ASSERT(false);
    }
}

bool AbstractSearchWidget::Private::updateFetchDirection()
{
    if (!m_mainModel)
        return false;

    const auto scrollBar = ui->ribbon->scrollBar();

    if (m_mainModel->rowCount() == 0 || scrollBar->value() == scrollBar->maximum())
        setFetchDirection(AbstractSearchListModel::FetchDirection::earlier);
    else if (scrollBar->value() == scrollBar->minimum())
        setFetchDirection(AbstractSearchListModel::FetchDirection::later);
    else
    {
        setFetchDirection(AbstractSearchListModel::FetchDirection::earlier);
        return false; //< Scroll bar is not at the beginning nor the end.
    }

    return true;
}

void AbstractSearchWidget::Private::requestFetchIfNeeded()
{
    if (ui->ribbon->isVisible() && updateFetchDirection())
        m_fetchMoreOperation->requestOperation();
}

void AbstractSearchWidget::Private::resetFilters()
{
    ui->cameraSelectionButton->deactivate(); //< Will do nothing if selector is set to read-only.
    ui->timeSelectionButton->deactivate();
    m_textFilterEdit->clear();
}

void AbstractSearchWidget::Private::addDeviceDependentAction(
    QAction* action, const QString& mixedString, const QString& cameraString)
{
    NX_ASSERT(action);
    m_deviceDependentActions.push_back({action, mixedString, cameraString});
}

void AbstractSearchWidget::Private::updateDeviceDependentActions()
{
    for (const auto& item: m_deviceDependentActions)
    {
        if (item.action)
        {
            item.action->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
                q->resourcePool(), item.mixedString, item.cameraString));
        }
    }
}

void AbstractSearchWidget::Private::setFetchDirection(AbstractSearchListModel::FetchDirection value)
{
    if (value == m_mainModel->fetchDirection())
        return;

    auto prevIndicator = relevantIndicatorModel();
    prevIndicator->setActive(false);
    prevIndicator->setVisible(m_mainModel->canFetchMore());

    m_mainModel->setFetchDirection(value);
}

void AbstractSearchWidget::Private::tryFetchMore()
{
    if (!m_mainModel->canFetchMore())
        return;

    m_mainModel->fetchMore();

    auto indicator = relevantIndicatorModel();
    const bool active = m_mainModel->fetchInProgress();
    indicator->setActive(active);
    if (active)
    {
        indicator->setVisible(true); //< All hiding is done in handleFetchFinished.
        updatePlaceholderVisibility();
    }
}

BusyIndicatorModel* AbstractSearchWidget::Private::relevantIndicatorModel() const
{
    return m_mainModel->fetchDirection() == AbstractSearchListModel::FetchDirection::earlier
        ? m_tailIndicatorModel.data()
        : m_headIndicatorModel.data();
}

void AbstractSearchWidget::Private::handleFetchFinished()
{
    const auto indicator = relevantIndicatorModel();
    indicator->setVisible(m_mainModel->canFetchMore());
    indicator->setActive(false);
    handleItemCountChanged();
}

void AbstractSearchWidget::Private::handleItemCountChanged()
{
    updatePlaceholderVisibility();

    const auto itemCount = m_mainModel->rowCount();
    ui->ribbon->viewportHeader()->setVisible(!m_placeholderVisible && itemCount > 0);

    static constexpr int kThreshold = 99;
    if (itemCount > kThreshold)
        m_itemCounterLabel->setText(QString(">") + q->itemCounterText(kThreshold));
    else
        m_itemCounterLabel->setText(q->itemCounterText(itemCount));
}

void AbstractSearchWidget::Private::updatePlaceholderVisibility()
{
    m_placeholderVisible = m_visualModel->rowCount() == 0 && !m_mainModel->canFetchMore();
    if (m_placeholderVisible)
        ui->placeholderText->setText(q->placeholderText(m_mainModel->isConstrained()));

    const auto effect = qobject_cast<QGraphicsOpacityEffect*>(ui->placeholder->graphicsEffect());
    if (!effect)
        return;

    const qreal targetOpacity = m_placeholderVisible ? 1.0 : 0.0;
    const qreal currentOpacity = effect->opacity();

    if (m_placeholderOpacityAnimation)
    {
        m_placeholderOpacityAnimation->pause();
        m_placeholderOpacityAnimation->deleteLater();
    }

    const qreal delta = qAbs(targetOpacity - currentOpacity);
    if (qFuzzyIsNull(delta))
        return;

    m_placeholderOpacityAnimation = new QPropertyAnimation(effect, "opacity", effect);
    m_placeholderOpacityAnimation->setStartValue(currentOpacity);
    m_placeholderOpacityAnimation->setEndValue(targetOpacity);
    m_placeholderOpacityAnimation->setEasingCurve(kPlaceholderFadeEasing);
    m_placeholderOpacityAnimation->setDuration(kPlaceholderFadeDuration.count() * delta);
    m_placeholderOpacityAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}

void AbstractSearchWidget::Private::setCurrentDate(const QDateTime& value)
{
    auto startOfDay = value;
    startOfDay.setTime(QTime(0, 0));

    if (m_currentDate == startOfDay)
        return;

    m_currentDate = startOfDay;

    const auto model = ui->ribbon->model();
    const auto range = ui->ribbon->visibleRange();

    if (range.isEmpty() || !NX_ASSERT(model))
        return;

    emit model->dataChanged(
        model->index(range.lower()), model->index(range.upper()), {Qn::TimestampTextRole});
}

} // namespace nx::vms::client::desktop
