// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_settings_dialog.h"
#include "ui_export_settings_dialog.h"

#include <limits>

#include <QtCore/QStandardPaths>

#include <client/client_runtime_settings.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/media_resource.h>
#include <nx/core/layout/layout_file_info.h>
#include <nx/core/transcoding/filters/timestamp_filter.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/common/widgets/control_bars.h>
#include <nx/vms/client/desktop/common/widgets/selectable_text_button_group.h>
#include <nx/vms/client/desktop/export/widgets/export_password_widget.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/image_providers/layout_thumbnail_loader.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/timezone_helper.h>
#include <nx/vms/time/formatter.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/event_processors.h>

#include "private/export_settings_dialog_p.h"

namespace nx::vms::client::desktop {

using State = ExportSettingsDialogState;
using Reducer = ExportSettingsDialogStateReducer;

namespace {

using namespace nx::vms::client::core;

static const QSize kPreviewSize(512, 288);
static constexpr int kBusyIndicatorDotRadius = 8;
static constexpr int kNoDataDefaultFontSize = 18;
static const char* kTabTitlePropertyName = "tabTitle";

static const QColor kLight16Color = "#698796";

static const SvgIconColorer::IconSubstitutions kIconSubstitutionsSettings =
{
    {QIcon::Normal, {{kLight16Color, "light14"}}},
};

static const SvgIconColorer::IconSubstitutions kCheckedIconSubstitutionsSettings =
{
    {QIcon::Normal, {{kLight16Color, "light10"}}},
};

static const SvgIconColorer::IconSubstitutions kDisabledIconSubstitutions =
{
    {QIcon::Normal, {{kLight16Color, "light16"}}},
    {QIcon::Active, {{kLight16Color, "light14"}}},
};

template<class Widget>
void setMaxOverlayWidth(Widget* settingsWidget, int width)
{
    QSignalBlocker blocker(settingsWidget);
    settingsWidget->setMaxOverlayWidth(width);
}

template<class Widget>
void setMaxOverlayHeight(Widget* settingsWidget, int height)
{
    QSignalBlocker blocker(settingsWidget);
    settingsWidget->setMaxOverlayHeight(height);
}

} // namespace

ExportSettingsDialog::ExportSettingsDialog(
    const QnTimePeriod& timePeriod,
    const QnCameraBookmark& bookmark,
    FileNameValidator isFileNameValid,
    const nx::core::Watermark& watermark,
    QWidget* parent)
    :
    base_type(parent),
    QnSessionAwareDelegate(parent),
    d(new Private(bookmark, kPreviewSize, watermark)),
    ui(new ::Ui::ExportSettingsDialog),
    isFileNameValid(isFileNameValid),
    m_passwordWidget(new ExportPasswordWidget(this))
{
    ui->setupUi(this);
    setHelpTopic(this, HelpTopic::Id::Exporting);

    // Save titles for restoring tabs when they are removed
    for (int i = 0; i < ui->tabWidget->count(); i++)
        ui->tabWidget->widget(i)->setProperty(kTabTitlePropertyName, ui->tabWidget->tabText(i));

    ui->mediaPreviewWidget->setMaximumSize(kPreviewSize);
    ui->layoutPreviewWidget->setMaximumSize(kPreviewSize);

    auto font = ui->mediaPreviewWidget->font();
    font.setPixelSize(kNoDataDefaultFontSize);
    ui->mediaPreviewWidget->setFont(font);

    ui->mediaFrame->ensurePolished();
    ui->layoutFrame->ensurePolished();

    const auto background = qApp->palette().color(QPalette::Shadow);
    setPaletteColor(ui->mediaFrame, QPalette::Window, background);
    setPaletteColor(ui->layoutFrame, QPalette::Window, background);

    ui->mediaPreviewWidget->setBorderRole(QPalette::NoRole);
    ui->mediaPreviewWidget->busyIndicator()->dots()->setDotRadius(kBusyIndicatorDotRadius);
    ui->mediaPreviewWidget->busyIndicator()->dots()->setDotSpacing(kBusyIndicatorDotRadius * 2);
    ui->layoutPreviewWidget->setAutoFillBackground(false);
    ui->layoutPreviewWidget->busyIndicator()->dots()->setDotRadius(kBusyIndicatorDotRadius);
    ui->layoutPreviewWidget->busyIndicator()->dots()->setDotSpacing(kBusyIndicatorDotRadius * 2);

    ui->mediaPreviewWidget->setAutoScaleUp(true);
    ui->mediaPreviewWidget->setAutoScaleDown(true);
    ui->layoutPreviewWidget->setAutoScaleUp(true);
    ui->layoutPreviewWidget->setAutoScaleDown(true);

    d->setMediaPreviewWidget(ui->mediaPreviewWidget);
    d->setLayoutPreviewWidget(ui->layoutPreviewWidget);

    const QSize frameSize(ui->mediaFrame->frameWidth(), ui->mediaFrame->frameWidth());
    ui->mediaFrame->setFixedSize(kPreviewSize + frameSize * 2);
    ui->layoutFrame->setFixedSize(kPreviewSize + frameSize * 2);

    ui->mediaExportSettingsWidget->setMaximumHeight(ui->mediaFrame->maximumHeight());
    ui->layoutExportSettingsWidget->setMaximumHeight(ui->layoutFrame->maximumHeight());

    autoResizePagesToContents(ui->tabWidget, QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed),
        true, [this]() { updateTabWidgetSize(); });

    d->createOverlays(ui->mediaPreviewWidget);

    setupSettingsButtons();

    auto exportButton = ui->buttonBox->addButton(QDialogButtonBox::StandardButton::Ok);
    exportButton->setText(tr("Export"));
    setAccentStyle(exportButton);

    connect(d.data(), &Private::validated, this, &ExportSettingsDialog::updateMessageBars);
    connect(d.data(), &Private::validated, this, &ExportSettingsDialog::updateWidgetsState);

    connect(this, &QDialog::accepted, d.get(), &Private::saveSettings);

    ui->rapidReviewSettingsPage->setSourcePeriodLengthMs(timePeriod.durationMs);

    ui->mediaFilenamePanel->setAllowedExtensions(d->allowedFileExtensions(ExportMode::media));
    ui->layoutFilenamePanel->setAllowedExtensions(d->allowedFileExtensions(ExportMode::layout));

    connect(ui->exportMediaSettingsPage, &ExportMediaSettingsWidget::dataEdited,
        d.get(), [this](const ExportMediaSettingsWidget::Data& data) { d->dispatch(Reducer::setApplyFilters, data.applyFilters); });
    connect(ui->exportLayoutSettingsPage, &ExportLayoutSettingsWidget::dataEdited,
        d.get(), [this](const ExportLayoutSettingsWidget::Data& data) { d->dispatch(Reducer::setLayoutReadOnly, data.readOnly); });
    connect(m_passwordWidget, &ExportPasswordWidget::dataEdited,
        d.get(), [this](const ExportPasswordWidget::Data& data) { d->dispatch(Reducer::setLayoutEncryption, data.cryptVideo, data.password); });

    connect(ui->mediaFilenamePanel, &FilenamePanel::filenameChanged,
        d.get(), d->dispatchTo(Reducer::setMediaFilename));
    connect(ui->layoutFilenamePanel, &FilenamePanel::filenameChanged,
        d.get(), d->dispatchTo(Reducer::setLayoutFilename));

    connect(ui->timestampSettingsPage, &TimestampOverlaySettingsWidget::dataChanged,
        d.get(), d->dispatchTo(Reducer::setTimestampOverlaySettings));
    connect(ui->timestampSettingsPage, &TimestampOverlaySettingsWidget::formatChanged,
        d.get(), d->dispatchTo(Reducer::setFormatTimestampOverlay));

    connect(ui->imageSettingsPage, &ImageOverlaySettingsWidget::dataChanged,
        d.get(), d->dispatchTo(Reducer::setImageOverlaySettings));

    connect(ui->textSettingsPage, &TextOverlaySettingsWidget::dataChanged,
        d.get(), d->dispatchTo(Reducer::setTextOverlaySettings));

    connect(ui->bookmarkSettingsPage, &BookmarkOverlaySettingsWidget::dataChanged,
        d.get(), d->dispatchTo(Reducer::setBookmarkOverlaySettings));

    connect(ui->infoSettingsPage, &InfoOverlaySettingsWidget::dataChanged, this,
        [this](ExportInfoOverlayPersistentSettings data)
        {
            d->dispatch(Reducer::setInfoOverlaySettings, d->getInfoTextData(data));
        });

    ui->bookmarkButton->setVisible(bookmark.isValid());

    const auto updateRapidReviewData =
        [this](int absoluteSpeed, qint64 frameStepMs)
        {
            if (ui->speedButton->state() == SelectableTextButton::State::deactivated)
            {
                d->dispatch(Reducer::hideRapidReview);
                return;
            }

            d->dispatch(Reducer::selectRapidReview);

            if (frameStepMs) //< 0 would mean rapid review is off due to impossibility.
            {
                d->dispatch(Reducer::setSpeed, absoluteSpeed);
            }
        };

    connect(ui->rapidReviewSettingsPage, &RapidReviewSettingsWidget::speedChanged,
        this, updateRapidReviewData);

    connect(ui->timestampSettingsPage, &TimestampOverlaySettingsWidget::deleteClicked,
        ui->timestampButton, &SelectableTextButton::deactivate);

    connect(ui->imageSettingsPage, &ImageOverlaySettingsWidget::deleteClicked,
        ui->imageButton, &SelectableTextButton::deactivate);

    connect(ui->textSettingsPage, &TextOverlaySettingsWidget::deleteClicked,
        ui->textButton, &SelectableTextButton::deactivate);

    connect(ui->infoSettingsPage, &InfoOverlaySettingsWidget::deleteClicked,
        ui->infoButton, &SelectableTextButton::deactivate);

    connect(ui->bookmarkSettingsPage, &BookmarkOverlaySettingsWidget::deleteClicked,
        ui->bookmarkButton, &SelectableTextButton::deactivate);

    connect(ui->rapidReviewSettingsPage, &RapidReviewSettingsWidget::deleteClicked,
        ui->speedButton, d->dispatchTo(Reducer::hideRapidReview));

    connect(ui->tabWidget, &QTabWidget::tabBarClicked, this,
        [this](int index)
        {
            const auto cameraTabIndex = ui->tabWidget->indexOf(ui->cameraTab);

            const auto isCameraMode = index == cameraTabIndex
                && ui->tabWidget->isTabEnabled(cameraTabIndex);

            d->dispatch(Reducer::setMode, isCameraMode ? ExportMode::media : ExportMode::layout);
        });

    d->dispatch(Reducer::setTimePeriod, timePeriod);
    d->dispatch(Reducer::setTimestampFont, ui->mediaPreviewWidget->font());
    d->subscribe([this](const State&){ renderState(); });
}

int ExportSettingsDialog::exec()
{
    d->dispatch(Reducer::loadSettings,
        appContext()->localSettings(),
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));

    // Updates media resource related settings like camera name.
    const auto infoData = d->state().exportMediaPersistentSettings.infoOverlay;
    d->dispatch(Reducer::setInfoOverlaySettings, d->getInfoTextData(infoData));

    return base_type::exec();
}

void ExportSettingsDialog::setupSettingsButtons()
{
    static const auto kPagePropertyName = "_qn_ExportSettingsPage";
    static const auto kOverlayPropertyName = "_qn_ExportSettingsOverlay";

    ui->cameraExportSettingsButton->setText(tr("Export Settings"));
    ui->cameraExportSettingsButton->setIcon(qnSkin->icon(
        "text_buttons/settings_20.svg",
        kIconSubstitutionsSettings,
        kCheckedIconSubstitutionsSettings));
    ui->cameraExportSettingsButton->setState(SelectableTextButton::State::selected);
    ui->cameraExportSettingsButton->setProperty(kPagePropertyName,
        QVariant::fromValue(ui->exportMediaSettingsPage));

    ui->layoutExportSettingsButton->setText(tr("Export Settings"));
    ui->layoutExportSettingsButton->setIcon(qnSkin->icon(
        "text_buttons/settings_20.svg",
        kIconSubstitutionsSettings,
        kCheckedIconSubstitutionsSettings));
    ui->layoutExportSettingsButton->setState(SelectableTextButton::State::selected);

    ui->timestampButton->setDeactivatable(true);
    ui->timestampButton->setDeactivatedText(tr("Add Timestamp"));
    ui->timestampButton->setDeactivationToolTip(tr("Delete Timestamp"));
    ui->timestampButton->setText(tr("Timestamp"));
    ui->timestampButton->setDeactivatedIcon(qnSkin->icon(
        "text_buttons/clock_20.svg",
        kDisabledIconSubstitutions));
    ui->timestampButton->setIcon(qnSkin->icon(
        "text_buttons/clock_20.svg",
        kIconSubstitutionsSettings,
        kCheckedIconSubstitutionsSettings));
    ui->timestampButton->setProperty(
        kPagePropertyName, QVariant::fromValue(ui->timestampSettingsPage));
    ui->timestampButton->setProperty(kOverlayPropertyName,
        QVariant::fromValue(ExportOverlayType::timestamp));

    ui->imageButton->setDeactivatable(true);
    ui->imageButton->setDeactivatedText(tr("Add Image"));
    ui->imageButton->setDeactivationToolTip(tr("Delete Image"));
    ui->imageButton->setText(tr("Image"));
    ui->imageButton->setDeactivatedIcon(qnSkin->icon(
        "text_buttons/image_20.svg",
        kDisabledIconSubstitutions));
    ui->imageButton->setIcon(qnSkin->icon(
        "text_buttons/image_20.svg",
        kIconSubstitutionsSettings,
        kCheckedIconSubstitutionsSettings));
    ui->imageButton->setProperty(kPagePropertyName, QVariant::fromValue(ui->imageSettingsPage));
    ui->imageButton->setProperty(kOverlayPropertyName,
        QVariant::fromValue(ExportOverlayType::image));

    ui->textButton->setDeactivatable(true);
    ui->textButton->setDeactivatedText(tr("Add Text"));
    ui->textButton->setDeactivationToolTip(tr("Delete Text"));
    ui->textButton->setText(tr("Text"));
    ui->textButton->setDeactivatedIcon(qnSkin->icon(
        "text_buttons/event_log_20.svg",
        kDisabledIconSubstitutions));
    ui->textButton->setIcon(qnSkin->icon(
        "text_buttons/event_log_20.svg",
        kIconSubstitutionsSettings,
        kCheckedIconSubstitutionsSettings));
    ui->textButton->setProperty(kPagePropertyName,
        QVariant::fromValue(ui->textSettingsPage));
    ui->textButton->setProperty(kOverlayPropertyName,
        QVariant::fromValue(ExportOverlayType::text));

    ui->infoButton->setDeactivatable(true);
    ui->infoButton->setDeactivatedText(tr("Add Info"));
    ui->infoButton->setDeactivationToolTip(tr("Delete Info"));
    ui->infoButton->setText(tr("Info"));
    ui->infoButton->setDeactivatedIcon(qnSkin->icon(
        "text_buttons/info_20x20.svg",
        kDisabledIconSubstitutions));
    ui->infoButton->setIcon(qnSkin->icon(
        "text_buttons/info_20x20.svg",
        kIconSubstitutionsSettings,
        kCheckedIconSubstitutionsSettings));
    ui->infoButton->setProperty(kPagePropertyName, QVariant::fromValue(ui->infoSettingsPage));
    ui->infoButton->setProperty(kOverlayPropertyName,
        QVariant::fromValue(ExportOverlayType::info));

    ui->speedButton->setDeactivatable(true);
    ui->speedButton->setDeactivatedText(tr("Rapid Review"));
    ui->speedButton->setDeactivationToolTip(tr("Reset Speed"));
    ui->speedButton->setDeactivatedIcon(qnSkin->icon(
        "text_buttons/rapid_review_20.svg",
        kDisabledIconSubstitutions));
    ui->speedButton->setIcon(qnSkin->icon(
        "text_buttons/rapid_review_20.svg",
        kIconSubstitutionsSettings,
        kCheckedIconSubstitutionsSettings));
    ui->speedButton->setProperty(kPagePropertyName,
        QVariant::fromValue(ui->rapidReviewSettingsPage));
    connect(ui->speedButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state) {
            switch (state)
            {
                case SelectableTextButton::State::selected:
                    d->dispatch(Reducer::selectRapidReview);
                    d->dispatch(Reducer::setSpeed,
                        d->state().exportMediaPersistentSettings.rapidReview.speed);
                    break;
                case SelectableTextButton::State::deactivated:
                    d->dispatch(Reducer::hideRapidReview);
                    break;
                default:
                    break;
            }
        });
    connect(ui->cameraExportSettingsButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::selected)
            {
                d->dispatch(Reducer::clearSelection);
            }
        });

    ui->bookmarkButton->setDeactivatable(true);
    ui->bookmarkButton->setDeactivatedText(tr("Add Bookmark Info"));
    ui->bookmarkButton->setDeactivationToolTip(tr("Delete Bookmark Info"));
    ui->bookmarkButton->setText(tr("Bookmark Info"));
    ui->bookmarkButton->setDeactivatedIcon(qnSkin->icon(
        "text_buttons/bookmark_20.svg",
        kDisabledIconSubstitutions));
    ui->bookmarkButton->setIcon(qnSkin->icon(
        "text_buttons/bookmark_20.svg",
        kIconSubstitutionsSettings,
        kCheckedIconSubstitutionsSettings));
    ui->bookmarkButton->setProperty(
        kPagePropertyName, QVariant::fromValue(ui->bookmarkSettingsPage));
    ui->bookmarkButton->setProperty(kOverlayPropertyName,
        QVariant::fromValue(ExportOverlayType::bookmark));

    connect(d.get(), &Private::overlaySelected, this, d->dispatchTo(Reducer::selectOverlay));

    auto group = new SelectableTextButtonGroup(this);
    group->add(ui->cameraExportSettingsButton);
    group->add(ui->bookmarkButton);
    group->add(ui->timestampButton);
    group->add(ui->imageButton);
    group->add(ui->textButton);
    group->add(ui->infoButton);
    group->add(ui->speedButton);
    group->setSelectionFallback(ui->cameraExportSettingsButton);

    connect(group, &SelectableTextButtonGroup::selectionChanged, this,
        [this](SelectableTextButton* /*old*/, SelectableTextButton* selected)
        {
            if (!selected)
                return;

            const auto page = selected->property(kPagePropertyName).value<QWidget*>();
            ui->mediaExportSettingsWidget->setCurrentWidget(page);
        });

    connect(group, &SelectableTextButtonGroup::buttonStateChanged, this,
        [this](SelectableTextButton* button)
        {
            NX_ASSERT(button);
            const auto overlayTypeVariant = button->property(kOverlayPropertyName);
            const auto overlayType = overlayTypeVariant.canConvert<ExportOverlayType>()
                ? overlayTypeVariant.value<ExportOverlayType>()
                : ExportOverlayType::none;

            switch (button->state())
            {
                case SelectableTextButton::State::deactivated:
                    d->dispatch(Reducer::hideOverlay, overlayType);
                    break;

                case SelectableTextButton::State::selected:
                    d->dispatch(Reducer::selectOverlay, overlayType);
                    break;

                default:
                    break;
            }
        });

    installEventHandler(ui->mediaFrame, QEvent::MouseButtonPress,
        ui->cameraExportSettingsButton, &QPushButton::click);

    installEventHandler(ui->mediaPreviewWidget, QEvent::MouseButtonPress,
        ui->cameraExportSettingsButton, &QPushButton::click);
}

ExportSettingsDialog::~ExportSettingsDialog()
{
    d->subscribe(nullptr);
    d->disconnect(this);
}

bool ExportSettingsDialog::tryClose(bool /*force*/)
{
    return close();
}

void ExportSettingsDialog::forcedUpdate()
{
    renderState();
}

SelectableTextButton* ExportSettingsDialog::buttonForOverlayType(ExportOverlayType type)
{
    switch (type)
    {
        case ExportOverlayType::timestamp:
            return ui->timestampButton;
        case ExportOverlayType::image:
            return ui->imageButton;
        case ExportOverlayType::text:
            return ui->textButton;
        case ExportOverlayType::info:
            return ui->infoButton;
        case ExportOverlayType::bookmark:
            return ui->bookmarkButton;
        default:
            return static_cast<SelectableTextButton*>(nullptr);
    }
};

ExportMode ExportSettingsDialog::mode() const
{
    return d->state().mode;
}

ExportMediaSettings ExportSettingsDialog::exportMediaSettings() const
{
    return d->state().getExportMediaSettings();
}

QnMediaResourcePtr ExportSettingsDialog::mediaResource() const
{
    return d->mediaResource();
}

LayoutResourcePtr ExportSettingsDialog::layout() const
{
    return d->layout();
}

ExportLayoutSettings ExportSettingsDialog::exportLayoutSettings() const
{
    return d->state().getExportLayoutSettings();
}

void ExportSettingsDialog::initSettingsWidgets()
{
    const auto& mediaPersistentSettings = d->state().exportMediaPersistentSettings;

    ui->exportLayoutSettingsPage->setData({ d->state().exportLayoutPersistentSettings.readOnly });

    // No settings for exportMediaSettingsPage - it will be set up in updateWidgetsState().

    if(mediaPersistentSettings.rapidReview.enabled)
    {
        ui->speedButton->setState(d->state().rapidReviewSelected
            ? SelectableTextButton::State::selected
            : SelectableTextButton::State::unselected);
        int speed = d->state().exportMediaPersistentSettings.rapidReview.speed;
        ui->rapidReviewSettingsPage->setSpeed(speed);
        ui->speedButton->setText(lit("%1 %3 %2x").
            arg(tr("Rapid Review")).
            arg(speed).
            arg(QChar(L'\x2013'))); //< N-dash
    }
    else
    {
        ui->speedButton->setState(SelectableTextButton::State::deactivated);
    }

    ui->timestampSettingsPage->setData(mediaPersistentSettings.timestampOverlay);
    ui->bookmarkSettingsPage->setData(mediaPersistentSettings.bookmarkOverlay);
    ui->imageSettingsPage->setData(mediaPersistentSettings.imageOverlay);
    ui->textSettingsPage->setData(mediaPersistentSettings.textOverlay);
    ui->infoSettingsPage->setData(mediaPersistentSettings.infoOverlay);

    ui->mediaFilenamePanel->setFilename(d->selectedFileName(ExportMode::media));
    ui->layoutFilenamePanel->setFilename(d->selectedFileName(ExportMode::layout));
}

void ExportSettingsDialog::updateTabWidgetSize()
{
    ui->tabWidget->setFixedSize(ui->tabWidget->minimumSizeHint());
}

void ExportSettingsDialog::updateMessageBars(ExportMode mode, const BarDescs& barDescriptions)
{
    updateMessageBarsInternal(mode, barDescriptions);
    updateTabWidgetSize();
}

void ExportSettingsDialog::updateMessageBarsInternal(
    ExportMode mode, const BarDescs& barDescriptions)
{
    if (mode != ExportMode::media && mode != ExportMode::layout)
        return;

    auto alertBlock = mode == ExportMode::media ? ui->mediaMessageBarBlock : ui->messageBarBlock;
    alertBlock->setMessageBars(barDescriptions);
}

void ExportSettingsDialog::updateWidgetsState()
{
    // Gather all data to apply to UI.
    const auto& settings = d->state().exportMediaPersistentSettings;
    bool transcodingLocked = settings.areFiltersForced();
    bool transcodingChecked = settings.applyFilters;
    auto mode = d->state().mode;
    bool overlayOptionsAvailable = mode == ExportMode::media && settings.canExportOverlays();

    auto exportButton = ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok);
    exportButton->setEnabled(mode == ExportMode::layout || d->hasCameraData());

    // No transcoding if no video.
    if (mode == ExportMode::media && !d->hasVideo())
    {
        transcodingLocked = true;
        transcodingChecked = false;
    }

    // Force transcoding for watermark.
    if (d->state().getExportMediaSettings().transcodingSettings.watermark.visible())
    {
        transcodingLocked = true;
        transcodingChecked = true;
    }

    // Applying data to UI.
    // All UI events should be locked here.
    ui->exportMediaSettingsPage->setData({transcodingChecked, !transcodingLocked});
    m_passwordWidget->setVisible(mode == ExportMode::layout
        || nx::core::layout::isLayoutExtension(ui->mediaFilenamePanel->filename().completeFileName()));

    ui->transcodingButtonsWidget->setVisible(overlayOptionsAvailable);

    if (settings.canExportOverlays())
        ui->timestampSettingsPage->setFormatEnabled(d->mediaSupportsUtc());

    // Update button state for used overlays.
    static const std::array<ExportOverlayType, 5> overlayTypes {
        ExportOverlayType::timestamp,
        ExportOverlayType::image,
        ExportOverlayType::text,
        ExportOverlayType::info,
        ExportOverlayType::bookmark
    };
    for (auto overlayType : overlayTypes)
    {
        SelectableTextButton* button = buttonForOverlayType(overlayType);
        if (!button)
            continue;

        if (d->state().exportMediaPersistentSettings.usedOverlays.contains(overlayType))
        {
            const bool overlayIsSelected = overlayType == d->state().selectedOverlayType;
            button->setState(overlayIsSelected
                ? SelectableTextButton::State::selected
                : SelectableTextButton::State::unselected);
        }
        else
        {
            button->setState(SelectableTextButton::State::deactivated);
        }
    }
}

void ExportSettingsDialog::setMediaParams(
    const QnMediaResourcePtr& mediaResource,
    const nx::vms::common::LayoutItemData& itemData)
{
    d->dispatch(Reducer::enableTab, ExportMode::media);

    const QTimeZone resourceTimeZone = timeZone(mediaResource);
    const QTimeZone timestampTimeZone = displayTimeZone(mediaResource);

    d->dispatch(Reducer::setTimestampTimeZone, timestampTimeZone);

    const auto currentSettings = d->state().getExportMediaSettings();
    const auto startTimeMs = currentSettings.period.startTimeMs;

    QString timePart;
    const auto resource = mediaResource->toResourcePtr();
    if (resource->hasFlags(Qn::utc))
    {
        auto dateTime = QDateTime::fromMSecsSinceEpoch(startTimeMs, timestampTimeZone);
        timePart = nx::vms::time::toString(dateTime, nx::vms::time::Format::filename_date);
    }
    else
    {
        timePart = nx::vms::time::toString(startTimeMs, nx::vms::time::Format::filename_time);
    }

    Filename baseFileName = currentSettings.fileName;
    QString namePart = resource->getName();

    static constexpr int kMaxNamePartLength = 200;
    if (namePart.length() > kMaxNamePartLength)
        namePart = namePart.left(kMaxNamePartLength) + '~';

    baseFileName.name = nx::utils::replaceNonFileNameCharacters(namePart + '_' + timePart, '_');

    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();

    d->setMediaResource(mediaResource);

    nx::core::transcoding::Settings settings;
    settings.aspectRatio = mediaResource->customAspectRatio();
    settings.enhancement = itemData.contrastParams;
    settings.dewarping = itemData.dewarpingParams;
    settings.zoomWindow = itemData.zoomRect;
    settings.rotation = itemData.rotation;
    d->dispatch(Reducer::setMediaResourceSettings, mediaResource->hasVideo(), settings);

    ui->mediaFilenamePanel->setFilename(suggestedFileName(baseFileName));

    // Disabling additional filters if we have no video.
    if (!d->state().exportMediaPersistentSettings.hasVideo)
    {
        ui->timestampButton->setEnabled(false);
        ui->imageButton->setEnabled(false);
        ui->textButton->setEnabled(false);
        ui->infoButton->setEnabled(false);
        ui->speedButton->setEnabled(false);
        ui->exportMediaSettingsPage->setData({false, false});
    }
}

void ExportSettingsDialog::setBookmarks(const QnCameraBookmarkList& bookmarks)
{
    d->dispatch(Reducer::setBookmarks, bookmarks);
}

void ExportSettingsDialog::setLayout(const LayoutResourcePtr& layout)
{
    d->dispatch(Reducer::enableTab, ExportMode::layout);
    const auto palette = ui->layoutPreviewWidget->palette();
    d->setLayout(layout, palette);

    auto baseFullName = nx::utils::replaceNonFileNameCharacters(layout->getName(), ' ');
    if (qnRuntime->isAcsMode() || baseFullName.isEmpty())
        baseFullName = tr("exported");

    Filename baseFileName = d->state().getExportLayoutSettings().fileName;
    baseFileName.name = Filename::parse(baseFullName).name;
    ui->layoutFilenamePanel->setFilename(suggestedFileName(baseFileName));
}

void ExportSettingsDialog::renderTabState()
{
    const auto& state = d->state();
    struct TabParams {
        QWidget* widget;
        bool available;
        QString reason;
    };
    // Should match the UI file content
    const std::array<TabParams, 2> tabs {{
        { ui->cameraTab, state.mediaAvailable, state.mediaDisableReason },
        { ui->layoutTab, state.layoutAvailable, state.layoutDisableReason }
    }};

    for (size_t i = 0; i < tabs.size(); i++)
    {
        auto tab = tabs[i];
        int tabIndex = ui->tabWidget->indexOf(tab.widget);

        if (!tab.available)
        {
            if (tabIndex != -1)
            {
                // Re-parent instead of ui->tabWidget->removeTab(tabIndex)
                // because we may add it back depending on the state.
                tab.widget->setParent(this);
            }
            continue;
        }

        if (tabIndex == -1)
        {
            const auto title = tab.widget->property(kTabTitlePropertyName).toString();
            ui->tabWidget->insertTab(i, tab.widget, title);
            tabIndex = ui->tabWidget->indexOf(tab.widget);
        }
        const bool isEnabled = tab.reason.isNull();
        ui->tabWidget->setTabEnabled(tabIndex, isEnabled);
        if (!isEnabled)
            ui->tabWidget->setTabToolTip(tabIndex, tab.reason);
    }

    const auto selectedTab = state.mode == ExportMode::media ? ui->cameraTab : ui->layoutTab;
    if (ui->tabWidget->currentWidget() != selectedTab)
        ui->tabWidget->setCurrentWidget(selectedTab);
}

void ExportSettingsDialog::renderState()
{
    renderTabState();

    initSettingsWidgets();

    // Place password input into the appropriate page.
    if(d->state().mode == ExportMode::media)
        ui->exportMediaSettingsPage->passwordPlaceholder()->addWidget(m_passwordWidget);
    else
        ui->exportLayoutSettingsPage->passwordPlaceholder()->addWidget(m_passwordWidget);

    updateWidgetsState();

    d->renderState();

    const auto size = d->state().fullFrameSize;
    if (size.isValid())
    {
        setMaxOverlayWidth(ui->bookmarkSettingsPage, size.width());
        setMaxOverlayWidth(ui->imageSettingsPage, size.width());
        setMaxOverlayWidth(ui->textSettingsPage, size.width());
        setMaxOverlayWidth(ui->infoSettingsPage, size.width());

        setMaxOverlayHeight(ui->infoSettingsPage, size.height());
    }
}

void ExportSettingsDialog::disableTab(ExportMode mode, const QString& reason)
{
    d->dispatch(Reducer::disableTab, mode, reason);
}

void ExportSettingsDialog::accept()
{
    auto filenamePanel = d->state().mode == ExportMode::media
        ? ui->mediaFilenamePanel
        : ui->layoutFilenamePanel;

    if (!filenamePanel->validate())
        return;

    if (nx::core::layout::isLayoutExtension(filenamePanel->filename().completeFileName())
        && !m_passwordWidget->validate())
    {
        return;
    }

    if (isFileNameValid && !isFileNameValid(filenamePanel->filename(), false))
        return;

    base_type::accept();
}

Filename ExportSettingsDialog::suggestedFileName(const Filename& baseName) const
{
    if (!isFileNameValid)
        return baseName;

    Filename suggested = baseName;
    for (int i = 1; i < std::numeric_limits<int>::max(); ++i)
    {
        if (isFileNameValid(suggested, true))
            return suggested;

        suggested.name = nx::format("%1 (%2)", baseName.name, i);
    }

    NX_ASSERT(false, "Failed to generate suggested filename");
    return baseName;
}

} // namespace nx::vms::client::desktop
