// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_search_widget_p.h"
#include "ui_abstract_search_widget.h"

#include <chrono>

#include <QtCore/QEasingCurve>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>
#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QToolButton>

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/unicode_chars.h>
#include <nx/vms/client/core/common/models/concatenation_list_model.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/managed_camera_set.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/search_line_edit.h>
#include <nx/vms/client/desktop/event_search/models/private/busy_indicator_model_p.h>
#include <nx/vms/client/desktop/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/desktop/event_search/widgets/placeholder_widget.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/common/palette.h>
#include <ui/common/read_only.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>

#include "tile_interaction_handler_p.h"

namespace nx::vms::client::desktop {

using namespace std::chrono;
using FetchRequest = core::FetchRequest;
using FetchDirection = core::EventSearch::FetchDirection;

namespace {

constexpr int kCounterPanelHeight = 32;

constexpr int kTextSearchHeight = 28;
constexpr int kTextSearchTopMargin = 12;
constexpr int kHeaderSideMargin = 8;

constexpr milliseconds kQueuedFetchDataDelay = 50ms;
constexpr milliseconds kTextFilterDelay = 250ms;

constexpr milliseconds kPlaceholderFadeDuration = 150ms;
constexpr auto kPlaceholderFadeEasing = QEasingCurve::InOutQuad;

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kThemeSubstitutions = {
    {QIcon::Normal, {.primary = "light16"}},
    {QIcon::Active, {.primary = "light14"}},
    {QIcon::Selected, {.primary = "light10"}},
    {QnIcon::Pressed, {.primary = "light14"}},
};

static const QColor kLight16Color = "#698796";
static const QColor kDark17Color = "#53707F";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight16Color, "light16"}, {kDark17Color, "light16"}}},
    {QIcon::Active, {{kLight16Color, "light14"}, {kDark17Color, "light14"}}},
    {QIcon::Selected, {{kLight16Color, "light10"}, {kDark17Color, "light14"}}},
    {QnIcon::Pressed, {{kLight16Color, "light14"}, {kDark17Color, "light14"}}},
};

NX_DECLARE_COLORIZED_ICON(kCalendarIcon, "20x20/Outline/calendar.svg", kThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kDeviceIcon, "20x20/Outline/device.svg", kThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kPlayIcon, "20x20/Outline/play.svg", kThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kCheckedEventLogIcon,
    "20x20/Outline/event_log.svg", kThemeSubstitutions,
    "20x20/Outline/no_list.svg", kThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kCheckedImageIcon,
    "20x20/Outline/image.svg", kThemeSubstitutions,
    "20x20/Outline/image_hide.svg", kThemeSubstitutions)

SearchLineEdit* createSearchLineEdit(QWidget* parent)
{
    auto result = new SearchLineEdit(parent);
    result->setDarkMode(true);
    result->setAttribute(Qt::WA_TranslucentBackground);
    result->setAttribute(Qt::WA_Hover);
    result->setAutoFillBackground(false);
    result->setTextChangedSignalFilterMs(kTextFilterDelay.count());
    result->setFocusPolicy(Qt::ClickFocus);
    setPaletteColor(result, QPalette::Shadow, Qt::transparent);

    result->setFixedHeight(kTextSearchHeight);
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

            button->icon().paint(painter, iconRect, Qt::Alignment(), mode, state);
            return true;
        };

    auto result = new CustomPainted<QToolButton>(parent);
    result->setCustomPaintFunction(paintFunction);
    result->setCheckable(true);
    result->setFocusPolicy(Qt::NoFocus);
    return result;
}

} // namespace

AbstractSearchWidgetPrivate::AbstractSearchWidgetPrivate(
    AbstractSearchWidget* q,
    core::AbstractSearchListModel* model)
    :
    WindowContextAware(q),
    q(q),
    ui(new Ui::AbstractSearchWidget()),
    m_mainModel(model),
    m_headIndicatorModel(new BusyIndicatorModel()),
    m_tailIndicatorModel(new BusyIndicatorModel()),
    m_visualModel(new core::ConcatenationListModel()),
    m_placeholderWidget(new PlaceholderWidget(q)),
    m_togglePreviewsButton(createCheckableToolButton(q)),
    m_toggleFootersButton(createCheckableToolButton(q)),
    m_itemCounterLabel(new QLabel(q)),
    m_textFilterEdit(createSearchLineEdit(q)),
    m_commonSetup(new CommonObjectSearchSetup(system())),
    m_fetchDataOperation(new nx::utils::PendingOperation())
{
    NX_CRITICAL(model, "Model must be specified.");
    m_mainModel->setParent(nullptr); //< Stored as a scoped pointer.

    m_commonSetup->setWindowContext(windowContext());
    m_commonSetup->setModel(m_mainModel.get());

    ui->setupUi(q);

    setPaletteColor(ui->separatorLine, QPalette::Shadow, core::colorTheme()->color("dark6"));

    ui->headerWidget->setStyleSheet(
        nx::format("#headerWidget { background-color: %1; border-left: 1px solid %2; }",
            core::colorTheme()->color("dark4").name(),
            core::colorTheme()->color("dark8").name()));

    ui->headerLayout->setContentsMargins(
        kHeaderSideMargin, kTextSearchTopMargin, kHeaderSideMargin, 0);

    auto margins = ui->filtersLayout->contentsMargins();
    margins.setLeft(0);
    ui->filtersLayout->setContentsMargins(margins);

    ui->headerSearchLayout->addWidget(m_textFilterEdit);
    m_textFilterEdit->setSizePolicy({QSizePolicy::Expanding, QSizePolicy::Preferred});

    connect(m_textFilterEdit, &SearchLineEdit::textChanged, this,
        [this](const QString& value)
        {
            if (auto textFilter = m_commonSetup->textFilter())
                textFilter->setText(value);
        });

    if (auto textFilter = m_commonSetup->textFilter())
    {
        connect(m_commonSetup->textFilter(), &core::TextFilterSetup::textChanged, this,
            [this]()
            {
                auto newText = m_commonSetup->textFilter()->text();
                if (m_textFilterEdit->text().trimmed() != newText)
                    m_textFilterEdit->setText(newText);
            });
    }

    connect(q->navigator(), &QnWorkbenchNavigator::currentResourceChanged, this,
        &AbstractSearchWidgetPrivate::updateCameraDisplaying);

    setupModels();
    setupRibbon();
    setupPlaceholder();
    setupTimeSelection();
    setupCameraSelection();
    setupCameraDisplaying();

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

    m_fetchDataOperation->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
    m_fetchDataOperation->setInterval(kQueuedFetchDataDelay);
    m_fetchDataOperation->setCallback([this]() { tryFetchData(); });

    connect(
        q->workbench(),
        &Workbench::currentLayoutChanged,
        this,
        [this]
        {
            if (!m_commonSetup)
                return;

            updateControlsVisibility();
        });

    updateControlsVisibility();
}

AbstractSearchWidgetPrivate::~AbstractSearchWidgetPrivate()
{
    // Required here for forward-declared scoped pointer destruction.
}

void AbstractSearchWidgetPrivate::setupModels()
{
    connect(m_mainModel.data(), &core::AbstractSearchListModel::dataNeeded,
        this, &AbstractSearchWidgetPrivate::requestFetchIfNeeded);

    connect(m_mainModel.data(), &QAbstractItemModel::modelReset,
        this,
        [this]()
        {
            m_dataFetched = false;
            handleItemCountChanged();
        });

    connect(m_mainModel.data(), &QAbstractItemModel::rowsRemoved,
        this, &AbstractSearchWidgetPrivate::handleItemCountChanged);

    connect(m_mainModel.data(), &QAbstractItemModel::rowsInserted,
        this, &AbstractSearchWidgetPrivate::handleItemCountChanged);

    using UpdateMode = EventRibbon::UpdateMode;

    connect(m_mainModel.data(), &QAbstractItemModel::modelAboutToBeReset, this,
        [this]() { ui->ribbon->setRemovalMode(UpdateMode::instant); });

    connect(m_mainModel.data(), &QAbstractItemModel::rowsAboutToBeRemoved,
        [this]() { ui->ribbon->setRemovalMode(UpdateMode::animated); });

    connect(m_mainModel.data(), &core::AbstractSearchListModel::fetchCommitStarted, this,
        [this](const FetchRequest& request)
        {
            ui->ribbon->setInsertionMode(UpdateMode::instant,
                request.direction == FetchDirection::newer);
        });

    connect(m_mainModel.data(), &core::AbstractSearchListModel::fetchFinished, this,
        [this](core::EventSearch::FetchResult fetchResult,
            int /*centralItemIndex*/,
            const FetchRequest& request)
        {
            m_dataFetched = fetchResult != core::EventSearch::FetchResult::failed
                && fetchResult != core::EventSearch::FetchResult::cancelled;
            ui->ribbon->setInsertionMode(UpdateMode::animated, false);
            setIndicatorVisible(request.direction, false);
            handleItemCountChanged();
        });

    connect(m_mainModel.data(), &core::AbstractSearchListModel::camerasChanged, this,
        [this]()
        {
            emit q->cameraSetChanged(
                m_mainModel->cameraSet().cameras(), AbstractSearchWidget::QPrivateSignal());
        });

    connect(m_mainModel.data(), &core::AbstractSearchListModel::isOnlineChanged, this,
        [this](bool isOnline)
        {
            m_mainModel->cameraSet().invalidateFilter();

            if (isOnline)
                updateDeviceDependentActions();
            else
                q->resetFilters();
        });

    if (m_mainModel->isOnline())
        executeLater([this]() { updateDeviceDependentActions(); }, this);

    // Busy indicator tile can be visible or hidden (`visible` property of the pseudo-model),
    // and the indicator on the tile can also be visible or hidden itself (`active` property of the
    // pseudo-model).
    // In current implementation of loading mechanic, busy indicator tiles are shown only during
    // a fetch, and are always active.
    m_headIndicatorModel->setVisible(false);
    m_tailIndicatorModel->setVisible(false);
    m_headIndicatorModel->setActive(true);
    m_tailIndicatorModel->setActive(true);

    m_headIndicatorModel->setObjectName("head");
    m_tailIndicatorModel->setObjectName("tail");

    m_visualModel->setModels({
        m_headIndicatorModel.data(), m_mainModel.data(), m_tailIndicatorModel.data()});
}

void AbstractSearchWidgetPrivate::setupRibbon()
{
    ui->ribbon->setModel(m_visualModel.data());
    ui->ribbon->setViewportMargins(0, style::Metrics::kStandardPadding);
    ui->ribbon->setFocusPolicy(Qt::NoFocus);
    ui->ribbon->scrollBar()->ensurePolished();
    setPaletteColor(ui->ribbon->scrollBar(), QPalette::Disabled, QPalette::Midlight,
        core::colorTheme()->color("dark5"));

    setupViewportHeader();

    connect(ui->ribbon->scrollBar(), &QScrollBar::valueChanged, this,
        [this]()
        {
            if (!m_skipFetchOnScrollChange)
                executeLater([this]() { requestFetchIfNeeded(); }, this);
        });

    connect(ui->ribbon, &EventRibbon::hovered, this,
        [this](const QModelIndex& idx, EventTile* tile)
        {
            emit q->tileHovered(idx, tile, AbstractSearchWidget::QPrivateSignal());
        });

    TileInteractionHandler::install(ui->ribbon);
}

void AbstractSearchWidgetPrivate::setupViewportHeader()
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

    setPaletteColor(m_itemCounterLabel, QPalette::Text, core::colorTheme()->color("dark16"));
    m_itemCounterLabel->setText({});

    m_toggleFootersButton->setChecked(ui->ribbon->footersEnabled());
    m_toggleFootersButton->setIcon(qnSkin->icon(kCheckedEventLogIcon));

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

    m_togglePreviewsButton->setVisible(!isVisibleModelEmpty());
    m_togglePreviewsButton->setChecked(ui->ribbon->previewsEnabled());
    m_togglePreviewsButton->setIcon(qnSkin->icon(kCheckedImageIcon));

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

    connect(m_togglePreviewsButton, &QToolButton::toggled,
        q, &AbstractSearchWidget::previewToggleChanged);

    updateThumbnailsToolTip();
}

void AbstractSearchWidgetPrivate::setupPlaceholder()
{
    anchorWidgetToParent(m_placeholderWidget);

    auto effect = new QGraphicsOpacityEffect(m_placeholderWidget);
    m_placeholderWidget->setGraphicsEffect(effect);

    connect(effect, &QGraphicsOpacityEffect::opacityChanged, this,
        [this](qreal opacity)
        {
            m_placeholderWidget->setHidden(qFuzzyIsNull(opacity));
            m_placeholderWidget->graphicsEffect()->setEnabled(!qFuzzyIsNull(opacity - 1.0));
        });

    m_placeholderVisible = false;
    effect->setOpacity(0.0);
}

void AbstractSearchWidgetPrivate::setupTimeSelection()
{
    ui->timeSelectionButton->setSelectable(false);
    ui->timeSelectionButton->setDeactivatable(true);
    ui->timeSelectionButton->setIcon(qnSkin->icon(kCalendarIcon));
    ui->timeSelectionButton->setFocusPolicy(Qt::NoFocus);

    const auto updateButton =
        [this]()
        {
            const auto selection = m_commonSetup->timeSelection();
            ui->timeSelectionButton->setText(m_timeSelectionActions[selection]->text());

            ui->timeSelectionButton->setState(selection == core::EventSearch::TimeSelection::anytime
                ? SelectableTextButton::State::deactivated
                : SelectableTextButton::State::unselected);

            ui->timeSelectionButton->setAccented(
                selection == core::EventSearch::TimeSelection::selection);
        };

    connect(m_commonSetup, &CommonObjectSearchSetup::timeSelectionChanged, this, updateButton);

    auto timeMenu = q->createDropdownMenu();
    auto addMenuAction =
        [this, timeMenu](const QString& title, core::EventSearch::TimeSelection period)
        {
            auto action = timeMenu->addAction(title);
            connect(action, &QAction::triggered, this,
                [this, period]() { m_commonSetup->setTimeSelection(period); });

            m_timeSelectionActions[period] = action;
            return action;
        };

    addMenuAction(tr("Last day"), core::EventSearch::TimeSelection::day);
    addMenuAction(tr("Last 7 days"), core::EventSearch::TimeSelection::week);
    addMenuAction(tr("Last 30 days"), core::EventSearch::TimeSelection::month);
    addMenuAction(tr("Selected on Timeline"), core::EventSearch::TimeSelection::selection);
    timeMenu->addSeparator();
    addMenuAction(tr("Any time"), core::EventSearch::TimeSelection::anytime);

    connect(ui->timeSelectionButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                m_timeSelectionActions[core::EventSearch::TimeSelection::anytime]->trigger();
        });

    m_timeSelectionActions[core::EventSearch::TimeSelection::anytime]->trigger();
    ui->timeSelectionButton->setMenu(timeMenu);
    updateButton();

    m_timeSelectionActions[core::EventSearch::TimeSelection::selection]->setVisible(false);
}

void AbstractSearchWidgetPrivate::setupCameraSelection()
{
    ui->cameraSelectionButton->setSelectable(false);
    ui->cameraSelectionButton->setDeactivatable(true);
    auto icon = qnSkin->icon(kDeviceIcon);
    ui->cameraSelectionButton->setIcon(icon);
    ui->cameraSelectionButton->setFocusPolicy(Qt::NoFocus);

    const auto updateButtonText =
        [this](core::EventSearch::CameraSelection cameras)
        {
            ui->cameraSelectionButton->setText(deviceButtonText(cameras));
        };

    const auto updateButton =
        [this, updateButtonText]()
        {
            const auto selection = m_commonSetup->cameraSelection();
            updateButtonText(selection);

            ui->cameraSelectionButton->setState(selection == core::EventSearch::CameraSelection::all
                ? SelectableTextButton::State::deactivated
                : SelectableTextButton::State::unselected);

            ui->cameraSelectionButton->setAccented(
                selection == core::EventSearch::CameraSelection::current);
        };

    connect(m_commonSetup, &CommonObjectSearchSetup::selectedCamerasChanged, this, updateButton);

    auto cameraMenu = q->createDropdownMenu();
    const auto addMenuAction =
        [this, cameraMenu, updateButtonText](
            const QString& title, core::EventSearch::CameraSelection cameras, bool dynamicTitle = false)
        {
            auto action = cameraMenu->addAction(title);
            connect(action, &QAction::triggered, this,
                [this, cameras]()
                {
                    if (cameras == core::EventSearch::CameraSelection::custom)
                        executeLater([this]() { m_commonSetup->chooseCustomCameras(); }, this);
                    else
                        m_commonSetup->setCameraSelection(cameras);
                });

            if (dynamicTitle)
            {
                connect(action, &QAction::changed, this,
                    [this, cameras, updateButtonText]()
                    {
                        if (m_commonSetup->cameraSelection() == cameras)
                            updateButtonText(cameras);
                    });
            }

            m_cameraSelectionActions[cameras] = action;
            return action;
        };

    addDeviceDependentAction(
        addMenuAction("<cameras on layout>", core::EventSearch::CameraSelection::layout, true),
        tr("Devices on layout"), tr("Cameras on layout"));

    addDeviceDependentAction(
        addMenuAction("<current camera>", core::EventSearch::CameraSelection::current, true),
        tr("Selected device"), tr("Selected camera"));

    cameraMenu->addSeparator();

    addDeviceDependentAction(
        addMenuAction("<choose cameras...>", core::EventSearch::CameraSelection::custom, true),
        tr("Choose devices..."), tr("Choose cameras..."));

    cameraMenu->addSeparator();

    addDeviceDependentAction(addMenuAction("<any camera>", core::EventSearch::CameraSelection::all, true),
        tr("Any device"), tr("Any camera"));

    connect(ui->cameraSelectionButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            const auto currentLayout = q->windowContext()->workbench()->currentLayoutResource();

            if (state == SelectableTextButton::State::deactivated
                && !currentLayout->flags().testFlag(Qn::cross_system))
            {
                m_cameraSelectionActions[core::EventSearch::CameraSelection::all]->trigger();
            }
        });

    ui->cameraSelectionButton->setMenu(cameraMenu);
    updateButton();

    // Current camera name watchers.

    connect(m_commonSetup, &CommonObjectSearchSetup::singleCameraChanged, this,
        [this]()
        {
            disconnect(m_currentCameraConnection);

            if (m_commonSetup->cameraSelection() != core::EventSearch::CameraSelection::current)
                return;

            const auto updateCurrentCameraName =
                [this]()
                {
                    m_cameraSelectionActions[core::EventSearch::CameraSelection::current]->trigger();
                };

            updateCurrentCameraName();

            const auto camera = m_commonSetup->singleCamera();
            if (!camera)
                return;

            m_currentCameraConnection = connect(camera.get(), &QnResource::nameChanged,
                this, updateCurrentCameraName);
        });
}

void AbstractSearchWidgetPrivate::setupCameraDisplaying()
{
    setReadOnly(ui->cameraDisplayingButton, true);

    ui->cameraDisplayingButton->setDeactivatable(false);
    ui->cameraDisplayingButton->setAccented(true);
    ui->cameraDisplayingButton->setFlat(true);
    ui->cameraDisplayingButton->setSelectable(false);
    ui->cameraDisplayingButton->setFocusPolicy(Qt::NoFocus);

    updateCameraDisplaying();
}

void AbstractSearchWidgetPrivate::updateCameraDisplaying()
{
    const auto resource = q->navigator()->currentResource();
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();

    ui->cameraDisplayingButton->setIcon(
        qnSkin->icon((!resource || camera) ? kDeviceIcon : kPlayIcon));

    if (resource)
    {
        const auto name = resource->getName();
        if (camera)
        {
            const auto baseText = QnDeviceDependentStrings::getNameFromSet(
                q->system()->resourcePool(),
                QnCameraDeviceStringSet(tr("Selected device"), tr("Selected camera")),
                camera);

            ui->cameraDisplayingButton->setText(
                QString("%1 %2 %3").arg(baseText, nx::UnicodeChars::kEnDash, name));
        }
        else
        {
            ui->cameraDisplayingButton->setText(
                QString("%1 %2 %3").arg(tr("Selected media"), nx::UnicodeChars::kEnDash, name));
        }
    }
    else
    {
        ui->cameraDisplayingButton->setText(QString("%1 %2 %3").arg(
            tr("Selected camera"),
            nx::UnicodeChars::kEnDash,
            tr("none", "No currently selected camera")));
    }
}

QString AbstractSearchWidgetPrivate::currentDeviceText() const
{
    const auto camera = m_commonSetup->singleCamera();
    const auto baseText = QnDeviceDependentStrings::getNameFromSet(system()->resourcePool(),
        QnCameraDeviceStringSet(tr("Selected device"), tr("Selected camera")), camera);

    return singleDeviceText(baseText, camera);
}

QString AbstractSearchWidgetPrivate::singleDeviceText(
    const QString& baseText, const QnVirtualCameraResourcePtr& device) const
{
    return QString("%1 %2 %3").arg(baseText, nx::UnicodeChars::kEnDash, device
        ? device->getName()
        : tr("none", "No currently selected camera"));
};

QString AbstractSearchWidgetPrivate::deviceButtonText(
    core::EventSearch::CameraSelection selection) const
{
    switch (selection)
    {
        case core::EventSearch::CameraSelection::current:
            return currentDeviceText();

        case core::EventSearch::CameraSelection::custom:
        {
            if (m_commonSetup->cameraCount() > 1)
            {
                return QnDeviceDependentStrings::getDefaultNameFromSet(
                    system()->resourcePool(),
                    tr("%n chosen devices", "", m_commonSetup->cameraCount()),
                    tr("%n chosen cameras", "", m_commonSetup->cameraCount()));
            }

            const auto camera = m_commonSetup->singleCamera();
            const auto baseText = QnDeviceDependentStrings::getNameFromSet(
                system()->resourcePool(),
                QnCameraDeviceStringSet(tr("Chosen device"), tr("Chosen camera")),
                camera);

            return singleDeviceText(baseText, camera);
        }

        default:
            return m_cameraSelectionActions[selection]->text();
    }
}

core::AbstractSearchListModel* AbstractSearchWidgetPrivate::model() const
{
    return m_mainModel.data();
}

EventRibbon* AbstractSearchWidgetPrivate::view() const
{
    return ui->ribbon;
}

bool AbstractSearchWidgetPrivate::isAllowed() const
{
    if (!m_isAllowed.has_value())
        q->updateAllowance();

    NX_ASSERT(m_isAllowed.has_value());
    return *m_isAllowed;
}

void AbstractSearchWidgetPrivate::setAllowed(bool value)
{
    if (m_isAllowed.has_value() && *m_isAllowed == value)
        return;

    m_isAllowed = value;
    emit q->allowanceChanged(*m_isAllowed, AbstractSearchWidget::QPrivateSignal());
}

void AbstractSearchWidgetPrivate::goToLive()
{
    if (m_mainModel->effectiveLiveSupported())
        m_mainModel->setLive(true);

    ui->ribbon->scrollBar()->setValue(0);
}

AbstractSearchWidget::Controls AbstractSearchWidgetPrivate::relevantControls() const
{
    return m_relevantControls;
}

void AbstractSearchWidgetPrivate::setRelevantControls(Controls value)
{
    if (m_relevantControls == value)
        return;

    m_relevantControls = value;
    updateControlsVisibility();
}

void AbstractSearchWidgetPrivate::updateControlsVisibility()
{
    if (workbench()->currentLayoutResource()->isCrossSystem())
        q->commonSetup()->setCameraSelection(nx::vms::client::core::EventSearch::CameraSelection::current);

    ui->cameraSelectionButton->setVisible(m_relevantControls.testFlag(Control::cameraSelector));
    ui->cameraDisplayingButton->setVisible(
        m_relevantControls.testFlag(Control::cameraSelectionDisplay));

    ui->timeSelectionButton->setVisible(m_relevantControls.testFlag(Control::timeSelector));
    const bool hasTextSearch = m_relevantControls.testFlag(Control::freeTextFilter);
    m_textFilterEdit->setVisible(hasTextSearch);
    ui->headerLayout->setContentsMargins(
        kHeaderSideMargin, hasTextSearch ? kTextSearchTopMargin : 0, kHeaderSideMargin, 0);
    m_toggleFootersButton->setVisible(m_relevantControls.testFlag(Control::footersToggler));
    m_togglePreviewsButton->setVisible(m_relevantControls.testFlag(Control::previewsToggler));
}

CommonObjectSearchSetup* AbstractSearchWidgetPrivate::commonSetup() const
{
    return m_commonSetup;
}

void AbstractSearchWidgetPrivate::setPlaceholderPixmap(const QPixmap& value)
{
    m_placeholderWidget->setPixmap(value);
}

SelectableTextButton* AbstractSearchWidgetPrivate::createCustomFilterButton()
{
    auto result = new SelectableTextButton(ui->filters);
    result->setFlat(true);
    result->setDeactivatable(true);
    result->setSelectable(false);
    result->setFocusPolicy(Qt::NoFocus);
    result->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    addFilterWidget(result, Qt::AlignLeft);
    return result;
}

void AbstractSearchWidgetPrivate::addFilterWidget(QWidget* widget, Qt::Alignment alignment)
{
    if (!NX_ASSERT(widget))
        return;

    widget->setParent(ui->filters);
    ui->filtersLayout->addWidget(widget, 0, alignment);
}

std::optional<FetchDirection> AbstractSearchWidgetPrivate::getFetchDirection()
{
    if (!m_mainModel)
        return {};

    const auto scrollBar = ui->ribbon->scrollBar();

    if (m_mainModel->rowCount() == 0 || scrollBar->value() == scrollBar->maximum())
        return FetchDirection::older;
    if (scrollBar->value() == scrollBar->minimum())
        return FetchDirection::newer;

    return {}; //< Scroll bar is not at the beginning nor the end.
}

void AbstractSearchWidgetPrivate::requestFetchIfNeeded()
{
    if (!ui->ribbon->isVisible())
        return;

    if (const auto direction = getFetchDirection(); !!direction)
        m_fetchDataOperation->requestOperation();
}

void AbstractSearchWidgetPrivate::resetFilters()
{
    ui->cameraSelectionButton->deactivate(); //< Will do nothing if selector is set to read-only.
    ui->timeSelectionButton->deactivate();
    m_textFilterEdit->clear();
}

void AbstractSearchWidgetPrivate::addDeviceDependentAction(
    QAction* action, const QString& mixedString, const QString& cameraString)
{
    NX_ASSERT(action);
    m_deviceDependentActions.push_back({action, mixedString, cameraString});
}

void AbstractSearchWidgetPrivate::setPreviewToggled(bool value)
{
    m_togglePreviewsButton->setChecked(value);
}

bool AbstractSearchWidgetPrivate::previewToggled() const
{
    return m_togglePreviewsButton->isChecked();
}

void AbstractSearchWidgetPrivate::setFooterToggled(bool value)
{
    m_toggleFootersButton->setChecked(value);
}

bool AbstractSearchWidgetPrivate::footerToggled() const
{
    return m_toggleFootersButton->isChecked();
}

void AbstractSearchWidgetPrivate::updateDeviceDependentActions()
{
    for (const auto& item: m_deviceDependentActions)
    {
        if (item.action)
        {
            item.action->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
                system()->resourcePool(), item.mixedString, item.cameraString));
        }
    }
}

void AbstractSearchWidgetPrivate::tryFetchData()
{
    const auto direction = getFetchDirection();

    FetchRequest request = { .direction = FetchDirection::older};
    const int count = m_mainModel->rowCount();
    if (direction.has_value() && count)
    {

        request.direction = *direction;
        const auto index = request.direction == FetchDirection::newer
            ? m_mainModel->index(0)
            : m_mainModel->index(count - 1);

        request.centralPointUs =
            m_mainModel->data(index, core::TimestampRole).value<microseconds>();
    }
    else
    {
        request.centralPointUs = currentCentralPointUs();
    }

    if (m_mainModel->fetchData(request)
        && m_mainModel->fetchInProgress())
    {
        setIndicatorVisible(request.direction, true); //< All hiding is done when fetch is finished.
    }

    updatePlaceholderVisibility();
    m_togglePreviewsButton->setVisible(!isVisibleModelEmpty());
}

void AbstractSearchWidgetPrivate::setIndicatorVisible(FetchDirection direction, bool value)
{
    const QScopedValueRollback scrollGuard(m_skipFetchOnScrollChange, true);

    const auto& indicator = direction == FetchDirection::older
        ? m_tailIndicatorModel
        : m_headIndicatorModel;

    indicator->setVisible(value);
}

void AbstractSearchWidgetPrivate::handleItemCountChanged()
{
    updatePlaceholderVisibility();
    m_togglePreviewsButton->setVisible(!isVisibleModelEmpty());

    const auto itemCount = m_mainModel->rowCount();
    ui->ribbon->viewportHeader()->setVisible(!m_placeholderVisible && itemCount > 0);

    static constexpr int kThreshold = 99;
    if (itemCount > kThreshold)
        m_itemCounterLabel->setText(QString(">") + q->itemCounterText(kThreshold));
    else
        m_itemCounterLabel->setText(q->itemCounterText(itemCount));
}

bool AbstractSearchWidgetPrivate::isVisibleModelEmpty() const
{
    return (m_visualModel->rowCount() == 0 && m_dataFetched) || m_mainModel->isFilterDegenerate();
}

void AbstractSearchWidgetPrivate::updatePlaceholderVisibility()
{
    m_placeholderVisible = isVisibleModelEmpty();

    if (m_placeholderVisible)
        m_placeholderWidget->setText(q->placeholderText(m_mainModel->isConstrained()));

    const auto effect = qobject_cast<QGraphicsOpacityEffect*>(m_placeholderWidget->graphicsEffect());
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

void AbstractSearchWidgetPrivate::setCurrentDate(const QDateTime& value)
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
        model->index(range.lower()), model->index(range.upper()), {core::TimestampTextRole});
}

void AbstractSearchWidgetPrivate::addSearchAction(QAction* action)
{
    const auto advancedButton = new QPushButton(action->text());
    connect(advancedButton, &QPushButton::clicked, action, &QAction::triggered);
    advancedButton->setSizePolicy({QSizePolicy::Minimum, QSizePolicy::Preferred});
    ui->headerSearchLayout->addWidget(advancedButton);
}

microseconds AbstractSearchWidgetPrivate::currentCentralPointUs() const
{
    if (!m_mainModel->rowCount())
        return microseconds(qnSyncTime->currentUSecsSinceEpoch());

    const int centralRowIndex =
        [this]()
    {
        auto visibleRange = ui->ribbon->visibleRange();
        if (m_headIndicatorModel->visible())
            visibleRange.shifted(-1);
        if (m_tailIndicatorModel->visible())
            visibleRange = visibleRange.truncatedRight(visibleRange.upper() - 1);

        if (visibleRange.lower() == 0)
            return 0;
        if (visibleRange.upper() == m_mainModel->rowCount() - 1)
            return visibleRange.upper();

        return (visibleRange.upper() + visibleRange.lower()) / 2;
    }();

    const auto index = m_mainModel->index(centralRowIndex);
    return m_mainModel->data(index, core::TimestampRole).value<microseconds>();
}

void AbstractSearchWidgetPrivate::setSearchDelay(std::chrono::milliseconds delay)
{
    m_textFilterEdit->setTextChangedSignalFilterMs(delay.count());
}

} // namespace nx::vms::client::desktop
