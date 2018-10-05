#include "abstract_search_widget_p.h"
#include "ui_abstract_search_widget.h"

#include <chrono>

#include <QtCore/QTimer>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <ui/common/palette.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>

#include <nx/client/desktop/common/models/concatenation_list_model.h>
#include <nx/client/desktop/common/utils/custom_painted.h>
#include <nx/client/desktop/common/utils/widget_anchor.h>
#include <nx/client/desktop/common/widgets/search_line_edit.h>
#include <nx/client/desktop/event_search/models/private/busy_indicator_model_p.h>
#include <nx/client/desktop/ui/common/color_theme.h>
#include <nx/client/desktop/utils/managed_camera_set.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/pending_operation.h>

namespace nx::client::desktop {

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
    m_textFilterEdit(createSearchLineEdit(q)),
    m_dayChangeTimer(new QTimer()),
    m_fetchMoreOperation(new utils::PendingOperation())
{
    NX_CRITICAL(model, "Model must be specified.");
    m_mainModel->setParent(nullptr); //< Stored as a scoped pointer.

    ui->setupUi(q);

    setPaletteColor(ui->separatorLine, QPalette::Shadow, colorTheme()->color("dark6"));

    ui->headerWidget->setAutoFillBackground(true);
    ui->headerLayout->insertWidget(0, m_textFilterEdit);
    connect(m_textFilterEdit, &SearchLineEdit::textChanged,
        q, &AbstractSearchWidget::textFilterChanged);

    ui->itemCounterLabel->setForegroundRole(QPalette::Mid);
    ui->itemCounterLabel->setText({});

    setupModels();
    setupRibbon();
    setupToolbar();
    setupPlaceholder();
    setupTimeSelection();
    setupCameraSelection();
    setupAreaSelection();

    installEventHandler(q, QEvent::Show, this, &Private::requestFetchIfNeeded);

    installEventHandler(q, {QEvent::Show, QEvent::Hide}, this,
        [this, q]()
        {
            m_mainModel->setLivePaused(!q->isVisible());
        });

    m_fetchMoreOperation->setFlags(utils::PendingOperation::FireOnlyWhenIdle);
    m_fetchMoreOperation->setIntervalMs(kQueuedFetchMoreDelay.count());
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

    connect(m_mainModel.data(), &AbstractSearchListModel::liveChanged, this,
        [this](bool isLive)
        {
            ui->ribbon->setLive(isLive);
            if (isLive)
                m_headIndicatorModel->setVisible(false);
        });

    connect(m_mainModel.data(), &AbstractSearchListModel::camerasChanged, this,
        [this]() { emit q->cameraSetChanged(m_mainModel->cameras()); });

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

    connect(m_mainModel.data(), &AbstractSearchListModel::fetchFinished,
        this, &Private::handleFetchFinished);

    m_visualModel->setModels({
        m_headIndicatorModel.data(), m_mainModel.data(), m_tailIndicatorModel.data()});
}

void AbstractSearchWidget::Private::setupRibbon()
{
    ui->ribbon->setModel(m_visualModel.data());
    ui->ribbon->setViewportMargins(0, style::Metrics::kStandardPadding);
    ui->ribbon->scrollBar()->ensurePolished();
    setPaletteColor(ui->ribbon->scrollBar(), QPalette::Disabled, QPalette::Midlight,
        colorTheme()->color("dark5"));

    connect(ui->ribbon->scrollBar(), &QScrollBar::valueChanged,
        this, &Private::requestFetchIfNeeded, Qt::QueuedConnection);

    connect(ui->ribbon, &EventRibbon::tileHovered, q, &AbstractSearchWidget::tileHovered);
}

void AbstractSearchWidget::Private::setupToolbar()
{
    ui->toggleFootersButton->setChecked(ui->ribbon->footersEnabled());
    ui->toggleFootersButton->setDrawnBackgrounds(ToolButton::ActiveBackgrounds);
    ui->toggleFootersButton->setIcon(qnSkin->icon(
        "text_buttons/text.png", "text_buttons/text_selected.png"));

    const auto updateInformationToolTip =
        [this]()
        {
            ui->toggleFootersButton->setToolTip(ui->toggleFootersButton->isChecked()
                ? tr("Hide information")
                : tr("Show information"));
        };

    connect(ui->toggleFootersButton, &QToolButton::toggled,
        ui->ribbon, &EventRibbon::setFootersEnabled);

    connect(ui->toggleFootersButton, &QToolButton::toggled,
        this, updateInformationToolTip);

    updateInformationToolTip();

    ui->togglePreviewsButton->setChecked(ui->ribbon->previewsEnabled());
    ui->togglePreviewsButton->setDrawnBackgrounds(ToolButton::ActiveBackgrounds);
    ui->togglePreviewsButton->setIcon(qnSkin->icon(
        "text_buttons/image.png", "text_buttons/image_selected.png"));

    const auto updateThumbnailsToolTip =
        [this]()
        {
            ui->togglePreviewsButton->setToolTip(ui->togglePreviewsButton->isChecked()
                ? tr("Hide thumbnails")
                : tr("Show thumbnails"));
        };

    connect(ui->togglePreviewsButton, &QToolButton::toggled,
        ui->ribbon, &EventRibbon::setPreviewsEnabled);

    connect(ui->togglePreviewsButton, &QToolButton::toggled,
        this, updateThumbnailsToolTip);

    updateThumbnailsToolTip();
}

void AbstractSearchWidget::Private::setupPlaceholder()
{
    ui->placeholder->setParent(ui->ribbonContainer);
    ui->placeholder->hide();
    anchorWidgetToParent(ui->placeholder);

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
    ui->timeSelectionButton->setIcon(qnSkin->icon(lit("text_buttons/rapid_review.png")));

    auto timeMenu = q->createDropdownMenu();
    auto addMenuAction =
        [this, timeMenu](const QString& title, Period period)
        {
            auto action = timeMenu->addAction(title);
            connect(action, &QAction::triggered, this,
                [this, action, period]()
                {
                    ui->timeSelectionButton->setText(action->text());
                    ui->timeSelectionButton->setState(period == Period::all
                        ? SelectableTextButton::State::deactivated
                        : SelectableTextButton::State::unselected);

                    ui->timeSelectionButton->setAccented(period == Period::selection);
                    setSelectedPeriod(period);
                });

            return action;
        };

    addMenuAction(tr("Last day"), Period::day);
    addMenuAction(tr("Last 7 days"), Period::week);
    addMenuAction(tr("Last 30 days"), Period::month);
    addMenuAction(tr("Selected on Timeline"), Period::selection);
    timeMenu->addSeparator();
    auto defaultAction = addMenuAction(tr("Any time"), Period::all);

    connect(ui->timeSelectionButton, &SelectableTextButton::stateChanged, this,
        [defaultAction](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                defaultAction->trigger();
        });

    defaultAction->trigger();
    ui->timeSelectionButton->setMenu(timeMenu);

    // Setup timeline selection watcher.

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

    // Setup day change watcher.

    using namespace std::chrono;
    m_dayChangeTimer->setInterval(milliseconds(seconds(1)).count());

    connect(m_dayChangeTimer.data(), &QTimer::timeout,
        this, &Private::updateCurrentTimePeriod);
}

void AbstractSearchWidget::Private::setupCameraSelection()
{
    ui->cameraSelectionButton->setSelectable(false);
    ui->cameraSelectionButton->setDeactivatable(true);
    ui->cameraSelectionButton->setIcon(qnSkin->icon(lit("text_buttons/camera.png")));

    auto cameraMenu = q->createDropdownMenu();
    auto addMenuAction =
        [this, cameraMenu](const QString& title, Cameras cameras, bool dynamicTitle = false)
        {
            auto action = cameraMenu->addAction(title);
            connect(action, &QAction::triggered, this,
                [this, action, cameras]()
                {
                    ui->cameraSelectionButton->setText(action->text());
                    ui->cameraSelectionButton->setState(cameras == Cameras::all
                        ? SelectableTextButton::State::deactivated
                        : SelectableTextButton::State::unselected);

                    ui->cameraSelectionButton->setAccented(cameras == Cameras::current);
                    setSelectedCameras(cameras);
                });

            if (dynamicTitle)
            {
                connect(action, &QAction::changed, this,
                    [this, action, cameras]()
                    {
                        if (selectedCameras() == cameras)
                            ui->cameraSelectionButton->setText(action->text());
                    });
            }

            return action;
        };

    addDeviceDependentAction(addMenuAction("<cameras on layout>", Cameras::layout, true),
        tr("Devices on layout"), tr("Cameras on layout"));

    addDeviceDependentAction(addMenuAction("<current camera>", Cameras::current, true),
        tr("Current device"), tr("Current camera"));

    cameraMenu->addSeparator();

    auto defaultAction = addMenuAction("<any camera>", Cameras::all, true);
    addDeviceDependentAction(defaultAction, tr("Any device"), tr("Any camera"));

    connect(ui->cameraSelectionButton, &SelectableTextButton::stateChanged, this,
        [defaultAction](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                defaultAction->trigger();
        });

    defaultAction->trigger();
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
}

void AbstractSearchWidget::Private::setupAreaSelection()
{
    ui->areaSelectionButton->setSelectable(true);
    ui->areaSelectionButton->setDeactivatable(true);
    ui->areaSelectionButton->setAccented(true);
    ui->areaSelectionButton->setDeactivatedText(tr("Anywhere on the video"));
    ui->areaSelectionButton->setIcon(qnSkin->icon(lit("text_buttons/area.png")));

    connect(q, &AbstractSearchWidget::cameraSetChanged, this,
        [this]()
        {
            const auto cameras = q->cameras();
            ui->areaSelectionButton->setEnabled(
                cameras.size() == 1 && (*cameras.cbegin())->hasVideo());

            ui->areaSelectionButton->deactivate();
        });

    connect(ui->areaSelectionButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::selected)
                ui->areaSelectionButton->setText(tr("Select some area on video"));
            else if (state == SelectableTextButton::State::unselected)
                ui->areaSelectionButton->setText(tr("In selected area"));
            else
                setSelectedArea({});
        });

    connect(q, &AbstractSearchWidget::selectedAreaChanged, this,
        [this](const QRectF& value)
        {
            if (value.isValid())
                ui->areaSelectionButton->setState(SelectableTextButton::State::unselected);
            else if (ui->areaSelectionButton->state() != SelectableTextButton::State::deactivated)
                ui->areaSelectionButton->setState(SelectableTextButton::State::selected);
        });

    // TODO: #vkutin Implement unified area selector on QnMediaResourceWidget
    // and add bi-directional connection with it here.
}

AbstractSearchListModel* AbstractSearchWidget::Private::model() const
{
    return m_mainModel.data();
}

AbstractSearchWidget::Controls AbstractSearchWidget::Private::relevantControls() const
{
    Controls result;
    result.setFlag(Control::cameraSelector, !ui->cameraSelectionButton->isHidden());
    result.setFlag(Control::timeSelector, !ui->timeSelectionButton->isHidden());
    result.setFlag(Control::areaSelector, !ui->areaSelectionHolder->isHidden());
    result.setFlag(Control::freeTextFilter, !m_textFilterEdit->isHidden());
    result.setFlag(Control::footersToggler, !ui->toggleFootersButton->isHidden());
    result.setFlag(Control::previewsToggler, !ui->togglePreviewsButton->isHidden());
    return result;
}

void AbstractSearchWidget::Private::setRelevantControls(Controls value)
{
    ui->cameraSelectionButton->setVisible(value.testFlag(Control::cameraSelector));
    ui->timeSelectionButton->setVisible(value.testFlag(Control::timeSelector));
    ui->areaSelectionHolder->setVisible(value.testFlag(Control::areaSelector));
    m_textFilterEdit->setVisible(value.testFlag(Control::freeTextFilter));
    ui->toggleFootersButton->setVisible(value.testFlag(Control::footersToggler));
    ui->togglePreviewsButton->setVisible(value.testFlag(Control::previewsToggler));
}

AbstractSearchWidget::Period AbstractSearchWidget::Private::selectedPeriod() const
{
    return m_period;
}

void AbstractSearchWidget::Private::setSelectedPeriod(Period value)
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

    emit q->timePeriodChanged(m_currentTimePeriod);
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

    auto current = qnSyncTime->currentDateTime();
    current.setTime(QTime(0, 0, 0, 0));
    return QnTimePeriod(current.addDays(1 - days).toMSecsSinceEpoch(),
        QnTimePeriod::infiniteDuration());
}

AbstractSearchWidget::Cameras AbstractSearchWidget::Private::selectedCameras() const
{
    return m_cameras;
}

void AbstractSearchWidget::Private::setSelectedCameras(Cameras value)
{
    if (value == m_cameras)
        return;

    m_cameras = value;
    updateCurrentCameras();
}

void AbstractSearchWidget::Private::setSelectedArea(const QRectF& value)
{
    if (qFuzzyEquals(m_selectedArea, value))
        return;

    m_selectedArea = value;
    emit q->selectedAreaChanged(m_selectedArea);
}

QnVirtualCameraResourceSet AbstractSearchWidget::Private::cameras() const
{
    return m_mainModel->cameraSet()->cameras();
}

QString AbstractSearchWidget::Private::textFilter() const
{
    return m_textFilterEdit->text();
}

QRectF AbstractSearchWidget::Private::selectedArea() const
{
    return m_selectedArea;
}

void AbstractSearchWidget::Private::setPlaceholderPixmap(const QPixmap& value)
{
    ui->placeholderIcon->setPixmap(value);
}

SelectableTextButton* AbstractSearchWidget::Private::createCustomFilterButton()
{
    auto result = new nx::client::desktop::SelectableTextButton(ui->filters);
    result->setDeactivatable(true);
    result->setSelectable(false);
    ui->filtersLayout->addWidget(result, 0, Qt::AlignLeft);
    return result;
}

void AbstractSearchWidget::Private::updateCurrentCameras()
{
    if (!m_mainModel->isOnline())
        return;

    ui->areaSelectionButton->setVisible(m_cameras == Cameras::current);

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

void AbstractSearchWidget::Private::requestFetchIfNeeded()
{
    if (!ui->ribbon->isVisible() || !model())
        return;

    const auto scrollBar = ui->ribbon->scrollBar();

    if (m_mainModel->rowCount() == 0 || scrollBar->value() == scrollBar->maximum())
        setFetchDirection(AbstractSearchListModel::FetchDirection::earlier);
    else if (scrollBar->value() == scrollBar->minimum())
        setFetchDirection(AbstractSearchListModel::FetchDirection::later);
    else
        return; //< Scroll bar is not at the beginning nor the end.

    m_fetchMoreOperation->requestOperation();
}

void AbstractSearchWidget::Private::resetFilters()
{
    ui->cameraSelectionButton->deactivate();
    ui->timeSelectionButton->deactivate();
    ui->areaSelectionButton->deactivate();
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
        indicator->setVisible(true); //< All hiding is done in handleFetchFinished.
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
}

void AbstractSearchWidget::Private::handleItemCountChanged()
{
    const auto itemCount = m_mainModel->rowCount();
    const bool placeholderVisible = itemCount == 0;
    if (placeholderVisible)
        ui->placeholderText->setText(q->placeholderText(m_mainModel->isConstrained()));

    ui->placeholder->setVisible(placeholderVisible);
    ui->counterContainer->setVisible(!placeholderVisible);

    static constexpr int kThreshold = 99;
    if (itemCount > kThreshold)
        ui->itemCounterLabel->setText(QString(">") + q->itemCounterText(kThreshold));
    else
        ui->itemCounterLabel->setText(q->itemCounterText(itemCount));
}

} // namespace nx::client::desktop
