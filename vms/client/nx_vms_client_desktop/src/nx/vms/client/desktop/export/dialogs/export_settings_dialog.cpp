#include "export_settings_dialog.h"
#include "private/export_settings_dialog_p.h"
#include "ui_export_settings_dialog.h"

#include <limits>

#include <client/client_runtime_settings.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <translation/datetime_formatter.h>
#include <ui/common/palette.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>
#include <utils/math/math.h>

#include <nx/client/core/watchers/server_time_watcher.h>
#include <nx/core/layout/layout_file_info.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/common/widgets/message_bar.h>
#include <nx/vms/client/desktop/common/widgets/selectable_text_button_group.h>
#include <nx/vms/client/desktop/export/widgets/export_password_widget.h>
#include <nx/vms/client/desktop/image_providers/layout_thumbnail_loader.h>
#include <nx/vms/client/desktop/ini.h>

namespace nx::vms::client::desktop {

namespace {

static const QSize kPreviewSize(512, 288);
static constexpr int kBusyIndicatorDotRadius = 8;
static constexpr int kNoDataDefaultFontSize = 18;

template<class Widget>
void setMaxOverlayWidth(Widget* settingsWidget, int width)
{
    QSignalBlocker blocker(settingsWidget);
    settingsWidget->setMaxOverlayWidth(width);
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
    d(new Private(bookmark, kPreviewSize, watermark)),
    ui(new ::Ui::ExportSettingsDialog),
    isFileNameValid(isFileNameValid),
    m_passwordWidget(new ExportPasswordWidget(this))
{
    ui->setupUi(this);
    setHelpTopic(this, Qn::Exporting_Help);

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

    auto exportButton = ui->buttonBox->addButton(tr("Export"), QDialogButtonBox::AcceptRole);
    setAccentStyle(exportButton);

    connect(d.data(), &Private::validated, this, &ExportSettingsDialog::updateAlerts);

    d->loadSettings();
    connect(this, &QDialog::accepted, d, &Private::saveSettings);

    ui->tabWidget->setCurrentWidget(d->mode() == Mode::Media ? ui->cameraTab : ui->layoutTab);

    d->setTimePeriod(timePeriod);

    ui->rapidReviewSettingsPage->setSourcePeriodLengthMs(timePeriod.durationMs);

    ui->mediaFilenamePanel->setAllowedExtensions(d->allowedFileExtensions(Mode::Media));
    ui->layoutFilenamePanel->setAllowedExtensions(d->allowedFileExtensions(Mode::Layout));

    initSettingsWidgets();

    connect(ui->exportMediaSettingsPage, &ExportMediaSettingsWidget::dataEdited,
        d, [this](const ExportMediaSettingsWidget::Data& data) { d->setApplyFilters(data.applyFilters); });
    connect(ui->exportLayoutSettingsPage, &ExportLayoutSettingsWidget::dataEdited,
        d, [this](const ExportLayoutSettingsWidget::Data& data) { d->setLayoutReadOnly(data.readOnly); });
    connect(m_passwordWidget, &ExportPasswordWidget::dataEdited,
        d, [this](const ExportPasswordWidget::Data& data) { d->setLayoutEncryption(data.cryptVideo, data.password); });

    connect(ui->mediaFilenamePanel, &FilenamePanel::filenameChanged,
        d, &Private::setMediaFilename);
    connect(ui->layoutFilenamePanel, &FilenamePanel::filenameChanged,
        d, &Private::setLayoutFilename);

    connect(ui->timestampSettingsPage, &TimestampOverlaySettingsWidget::dataChanged,
        d, &Private::setTimestampOverlaySettings);

    connect(ui->imageSettingsPage, &ImageOverlaySettingsWidget::dataChanged,
        d, &Private::setImageOverlaySettings);

    connect(ui->textSettingsPage, &TextOverlaySettingsWidget::dataChanged,
        d, &Private::setTextOverlaySettings);

    connect(ui->bookmarkSettingsPage, &BookmarkOverlaySettingsWidget::dataChanged,
        d, &Private::setBookmarkOverlaySettings);

    ui->bookmarkButton->setVisible(bookmark.isValid());
    if (!bookmark.isValid()) // Just in case...
        ui->bookmarkButton->setState(SelectableTextButton::State::deactivated);

    ui->speedButton->setState(d->storedRapidReviewSettings().enabled
        ? SelectableTextButton::State::unselected
        : SelectableTextButton::State::deactivated);

    const auto updateRapidReviewData =
        [this](int absoluteSpeed, qint64 frameStepMs)
        {
            ui->speedButton->setText(lit("%1 %3 %2x").arg(tr("Rapid Review")).arg(absoluteSpeed).
                arg(QChar(L'\x2013'))); //< N-dash

            if (ui->speedButton->state() == SelectableTextButton::State::deactivated)
                return;

            d->setRapidReviewFrameStep(frameStepMs);

            if (frameStepMs) //< 0 would mean rapid review is off due to impossibility.
            {
                d->setStoredRapidReviewSettings({
                    d->storedRapidReviewSettings().enabled, absoluteSpeed});
            }
        };

    connect(ui->rapidReviewSettingsPage, &RapidReviewSettingsWidget::speedChanged,
        this, updateRapidReviewData);

    updateRapidReviewData(ui->rapidReviewSettingsPage->speed(),
        ui->rapidReviewSettingsPage->frameStepMs());

    connect(ui->timestampSettingsPage, &TimestampOverlaySettingsWidget::deleteClicked,
        ui->timestampButton, &SelectableTextButton::deactivate);

    connect(ui->imageSettingsPage, &ImageOverlaySettingsWidget::deleteClicked,
        ui->imageButton, &SelectableTextButton::deactivate);

    connect(ui->textSettingsPage, &TextOverlaySettingsWidget::deleteClicked,
        ui->textButton, &SelectableTextButton::deactivate);

    connect(ui->bookmarkSettingsPage, &BookmarkOverlaySettingsWidget::deleteClicked,
        ui->bookmarkButton, &SelectableTextButton::deactivate);

    connect(ui->rapidReviewSettingsPage, &RapidReviewSettingsWidget::deleteClicked,
        ui->speedButton, &SelectableTextButton::deactivate);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &ExportSettingsDialog::updateMode);

    connect(d, &Private::transcodingModeChanged, this,
        &ExportSettingsDialog::updateWidgetsState);

    connect(d, &Private::frameSizeChanged, this,
        [this](const QSize& size)
        {
            setMaxOverlayWidth(ui->bookmarkSettingsPage, size.width());
            setMaxOverlayWidth(ui->imageSettingsPage, size.width());
            setMaxOverlayWidth(ui->textSettingsPage, size.width());
        });

    if (ui->bookmarkButton->state() != SelectableTextButton::State::deactivated)
        ui->bookmarkButton->click(); //< Set current page to bookmark info.
    else
        ui->cameraExportSettingsButton->click(); //< Set current page to settings

    updateMode(); //< Will also reparent m_passwordWidget.
}

void ExportSettingsDialog::setupSettingsButtons()
{
    static const auto kPagePropertyName = "_qn_ExportSettingsPage";
    static const auto kOverlayPropertyName = "_qn_ExportSettingsOverlay";

    ui->cameraExportSettingsButton->setText(tr("Export Settings"));
    ui->cameraExportSettingsButton->setIcon(qnSkin->icon(
        lit("text_buttons/settings_hovered.png"),
        lit("text_buttons/settings_selected.png")));
    ui->cameraExportSettingsButton->setState(SelectableTextButton::State::selected);
    ui->cameraExportSettingsButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->exportMediaSettingsPage));

    ui->layoutExportSettingsButton->setText(tr("Export Settings"));
    ui->layoutExportSettingsButton->setIcon(qnSkin->icon(
        lit("text_buttons/settings_hovered.png"),
        lit("text_buttons/settings_selected.png")));
    ui->layoutExportSettingsButton->setState(SelectableTextButton::State::selected);

    ui->timestampButton->setDeactivatable(true);
    ui->timestampButton->setDeactivatedText(tr("Add Timestamp"));
    ui->timestampButton->setDeactivationToolTip(tr("Delete Timestamp"));
    ui->timestampButton->setText(tr("Timestamp"));
    ui->timestampButton->setDeactivatedIcon(qnSkin->icon(lit("text_buttons/timestamp.png")));
    ui->timestampButton->setIcon(qnSkin->icon(
        lit("text_buttons/timestamp_hovered.png"),
        lit("text_buttons/timestamp_selected.png")));
    ui->timestampButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->timestampSettingsPage));
    ui->timestampButton->setProperty(kOverlayPropertyName,
        qVariantFromValue(ExportOverlayType::timestamp));

    ui->imageButton->setDeactivatable(true);
    ui->imageButton->setDeactivatedText(tr("Add Image"));
    ui->imageButton->setDeactivationToolTip(tr("Delete Image"));
    ui->imageButton->setText(tr("Image"));
    ui->imageButton->setDeactivatedIcon(qnSkin->icon(lit("text_buttons/image.png")));
    ui->imageButton->setIcon(qnSkin->icon(
        lit("text_buttons/image_hovered.png"),
        lit("text_buttons/image_selected.png")));
    ui->imageButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->imageSettingsPage));
    ui->imageButton->setProperty(kOverlayPropertyName,
        qVariantFromValue(ExportOverlayType::image));

    ui->textButton->setDeactivatable(true);
    ui->textButton->setDeactivatedText(tr("Add Text"));
    ui->textButton->setDeactivationToolTip(tr("Delete Text"));
    ui->textButton->setText(tr("Text"));
    ui->textButton->setDeactivatedIcon(qnSkin->icon(lit("text_buttons/text.png")));
    ui->textButton->setIcon(qnSkin->icon(
        lit("text_buttons/text_hovered.png"),
        lit("text_buttons/text_selected.png")));
    ui->textButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->textSettingsPage));
    ui->textButton->setProperty(kOverlayPropertyName,
        qVariantFromValue(ExportOverlayType::text));

    ui->speedButton->setDeactivatable(true);
    ui->speedButton->setDeactivatedText(tr("Rapid Review"));
    ui->speedButton->setDeactivationToolTip(tr("Reset Speed"));
    ui->speedButton->setDeactivatedIcon(qnSkin->icon(lit("text_buttons/rapid_review.png")));
    ui->speedButton->setIcon(qnSkin->icon(
        lit("text_buttons/rapid_review_hovered.png"),
        lit("text_buttons/rapid_review_selected.png")));
    ui->speedButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->rapidReviewSettingsPage));
    connect(ui->speedButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            const bool enabled = state != SelectableTextButton::State::deactivated;
            d->setRapidReviewFrameStep(enabled ? ui->rapidReviewSettingsPage->frameStepMs() : 0);
            d->setStoredRapidReviewSettings({enabled, d->storedRapidReviewSettings().speed});
            ui->transcodingButtonsWidget->layout()->activate();
        });

    ui->bookmarkButton->setDeactivatable(true);
    ui->bookmarkButton->setDeactivatedText(tr("Add Bookmark Info"));
    ui->bookmarkButton->setDeactivationToolTip(tr("Delete Bookmark Info"));
    ui->bookmarkButton->setText(tr("Bookmark Info"));
    ui->bookmarkButton->setDeactivatedIcon(qnSkin->icon(lit("text_buttons/bookmark.png")));
    ui->bookmarkButton->setIcon(qnSkin->icon(
        lit("text_buttons/bookmark_hovered.png"),
        lit("text_buttons/bookmark_selected.png")));
    ui->bookmarkButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->bookmarkSettingsPage));
    ui->bookmarkButton->setProperty(kOverlayPropertyName,
        qVariantFromValue(ExportOverlayType::bookmark));

    connect(d, &Private::overlaySelected, this,
        [this](ExportOverlayType type)
        {
            auto button = buttonForOverlayType(type);
            if (button && button->state() != SelectableTextButton::State::selected)
                button->click();
        });

    auto group = new SelectableTextButtonGroup(this);
    group->add(ui->cameraExportSettingsButton);
    group->add(ui->bookmarkButton);
    group->add(ui->timestampButton);
    group->add(ui->imageButton);
    group->add(ui->textButton);
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
                    d->hideOverlay(overlayType);
                    break;

                case SelectableTextButton::State::selected:
                    d->selectOverlay(overlayType);
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
    d->disconnect(this);
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
        case ExportOverlayType::bookmark:
            return ui->bookmarkButton;
        default:
            return static_cast<SelectableTextButton*>(nullptr);
    }
};

ExportSettingsDialog::Mode ExportSettingsDialog::mode() const
{
    return d->mode();
}

ExportMediaSettings ExportSettingsDialog::exportMediaSettings() const
{
    return d->exportMediaSettings();
}

ExportLayoutSettings ExportSettingsDialog::exportLayoutSettings() const
{
    return d->exportLayoutSettings();
}

void ExportSettingsDialog::initSettingsWidgets()
{
    const auto& mediaPersistentSettings = d->exportMediaPersistentSettings();

    ui->exportLayoutSettingsPage->setData({ d->exportLayoutPersistentSettings().readOnly });

    // No settings for exportMediaSettingsPage - it will be set up in updateWidgetsState().

    if(mediaPersistentSettings.rapidReview.enabled)
    {
        int speed = d->storedRapidReviewSettings().speed;
        ui->rapidReviewSettingsPage->setSpeed(speed);
    }

    ui->timestampSettingsPage->setData(mediaPersistentSettings.timestampOverlay);
    ui->bookmarkSettingsPage->setData(mediaPersistentSettings.bookmarkOverlay);
    ui->imageSettingsPage->setData(mediaPersistentSettings.imageOverlay);
    ui->textSettingsPage->setData(mediaPersistentSettings.textOverlay);

    ui->mediaFilenamePanel->setFilename(d->selectedFileName(Mode::Media));
    ui->layoutFilenamePanel->setFilename(d->selectedFileName(Mode::Layout));
}

void ExportSettingsDialog::updateTabWidgetSize()
{
    ui->tabWidget->setFixedSize(ui->tabWidget->minimumSizeHint());
}

void ExportSettingsDialog::updateMode()
{
    const auto cameraTabIndex = ui->tabWidget->indexOf(ui->cameraTab);

    const auto isCameraMode = ui->tabWidget->currentIndex() == cameraTabIndex
        && ui->tabWidget->isTabEnabled(cameraTabIndex);

    // Place password input into the appropriate page.
    if(isCameraMode)
        ui->exportMediaSettingsPage->passwordPlaceholder()->addWidget(m_passwordWidget);
    else
        ui->exportLayoutSettingsPage->passwordPlaceholder()->addWidget(m_passwordWidget);

    const auto currentMode = isCameraMode ? Mode::Media : Mode::Layout;
    d->setMode(currentMode);

    updateWidgetsState();
}

void ExportSettingsDialog::updateAlerts(Mode mode, const QStringList& weakAlerts,
    const QStringList& severeAlerts)
{
    switch (mode)
    {
        case Mode::Media:
            updateAlertsInternal(ui->weakMediaAlertsLayout, weakAlerts, false);
            updateAlertsInternal(ui->severeMediaAlertsLayout, severeAlerts, true);
            break;

        case Mode::Layout:
            updateAlertsInternal(ui->weakLayoutAlertsLayout, weakAlerts, false);
            updateAlertsInternal(ui->severeLayoutAlertsLayout, severeAlerts, true);
            break;
    }

    updateTabWidgetSize();
}

void ExportSettingsDialog::updateAlertsInternal(QLayout* layout,
    const QStringList& texts, bool severe)
{
    const auto newCount = texts.count();
    const auto oldCount = layout->count();

    const auto setAlertText =
        [layout](int index, const QString& text)
        {
            auto item = layout->itemAt(index);
            // Notice: notifications are added at the runtime. It is possible to have no
            // widget so far, especially when dialog is only initialized.
            if (auto bar = item ? qobject_cast<MessageBar*>(item->widget()) : nullptr)
                bar->setText(text);
        };

    if (newCount > oldCount)
    {
        // Add new alert bars.
        for (int i = oldCount; i < newCount; ++i)
            layout->addWidget(severe ? new AlertBar() : new MessageBar());
    }
    else
    {
        // Clear unused alerts.
        for (int i = newCount; i < oldCount; ++i)
            setAlertText(i, QString());
    }

    // Set alert texts.
    for (int i = 0; i < newCount; ++i)
        setAlertText(i, texts[i]);
}

void ExportSettingsDialog::updateWidgetsState()
{
    // Gather all data to apply to UI.
    const auto& settings = d->exportMediaPersistentSettings();
    bool transcodingLocked = settings.areFiltersForced();
    bool transcodingChecked = settings.applyFilters;
    auto mode = d->mode();
    bool overlayOptionsAvailable = mode == Mode::Media && settings.canExportOverlays();

    // No transcoding if no video.
    if (mode == Mode::Media && !d->hasVideo())
    {
        transcodingLocked = true;
        transcodingChecked = false;
    }

    // Force transcoding for watermark.
    if (d->exportMediaSettings().transcodingSettings.watermark.visible())
    {
        transcodingLocked = true;
        transcodingChecked = true;
    }

    // Applying data to UI.
    // All UI events should be locked here.
    ui->exportMediaSettingsPage->setData({transcodingChecked, !transcodingLocked});
    m_passwordWidget->setVisible(mode == Mode::Layout
        || nx::core::layout::isLayoutExtension(ui->mediaFilenamePanel->filename().completeFileName()));

    if (overlayOptionsAvailable)
        ui->cameraExportSettingsButton->click();

    // Yep, we need exactly this condition.
    if (overlayOptionsAvailable == ui->transcodingButtonsWidget->isHidden())
        ui->transcodingButtonsWidget->setHidden(!overlayOptionsAvailable);

    if (settings.canExportOverlays())
        ui->timestampSettingsPage->setFormatEnabled(d->mediaSupportsUtc());

    // Update button state for used overlays.
    for (auto overlay: settings.usedOverlays)
    {
        SelectableTextButton* button = buttonForOverlayType(overlay);
        if (!button)
            continue;
        if (button->state() == SelectableTextButton::State::deactivated)
        {
            button->setState(SelectableTextButton::State::unselected);
        }
    }
}

void ExportSettingsDialog::setMediaParams(
    const QnMediaResourcePtr& mediaResource,
    const QnLayoutItemData& itemData,
    QnWorkbenchContext* context)
{
    const auto timeWatcher = context->instance<core::ServerTimeWatcher>();
    d->setServerTimeZoneOffsetMs(timeWatcher->utcOffset(mediaResource, Qn::InvalidUtcOffset));

    const auto timestampOffsetMs = timeWatcher->displayOffset(mediaResource);
    d->setTimestampOffsetMs(timestampOffsetMs);

    const auto resource = mediaResource->toResourcePtr();
    const auto currentSettings = d->exportMediaSettings();
    const auto startTimeMs = currentSettings.period.startTimeMs;

    QString timePart;
    if (resource->hasFlags(Qn::utc))
    {
        timePart = datetime::toString(startTimeMs + timestampOffsetMs,
            datetime::Format::filename_date);
    }
    else
    {
        timePart = datetime::toString(startTimeMs, datetime::Format::filename_time);
    }

    Filename baseFileName = currentSettings.fileName;
    QString namePart = resource->getName();
    baseFileName.name = nx::utils::replaceNonFileNameCharacters(namePart + L'_' + timePart, L'_');

    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    const auto customAr = mediaResource->customAspectRatio();

    nx::core::transcoding::Settings settings;
    settings.aspectRatio = mediaResource->customAspectRatio();
    settings.enhancement = itemData.contrastParams;
    settings.dewarping = itemData.dewarpingParams;
    settings.zoomWindow = itemData.zoomRect;
    settings.rotation = itemData.rotation;

    d->setMediaResource(mediaResource, settings);

    // That also emits ExportSettingsDialog::Private::setMediaFilename(const Filename& filename)
    //      and then emits ExportSettingsDialog::Private::transcodingAllowedChanged
    ui->mediaFilenamePanel->setFilename(suggestedFileName(baseFileName));

    // Disabling additional filters if we have no video.
    if (!mediaResource->hasVideo())
    {
        ui->timestampButton->setEnabled(false);
        ui->imageButton->setEnabled(false);
        ui->textButton->setEnabled(false);
        ui->speedButton->setEnabled(false);
        ui->exportMediaSettingsPage->setData({false, false});
    }
}

void ExportSettingsDialog::setBookmarks(const QnCameraBookmarkList& bookmarks)
{
    d->setBookmarks(bookmarks);
}

void ExportSettingsDialog::setLayout(const QnLayoutResourcePtr& layout)
{
    const auto palette = ui->layoutPreviewWidget->palette();
    d->setLayout(layout, palette);

    auto baseFullName = nx::utils::replaceNonFileNameCharacters(layout->getName(), L' ');
    if (qnRuntime->isAcsMode() || baseFullName.isEmpty())
        baseFullName = tr("exported");

    Filename baseFileName = d->exportLayoutSettings().fileName;
    baseFileName.name = Filename::parse(baseFullName).name;
    ui->layoutFilenamePanel->setFilename(suggestedFileName(baseFileName));
}

void ExportSettingsDialog::disableTab(Mode mode, const QString& reason)
{
    NX_ASSERT(ui->tabWidget->count() > 1);

    QWidget* tab = (mode == Mode::Media) ? ui->cameraTab : ui->layoutTab;
    int tabIndex = ui->tabWidget->indexOf(tab);
    ui->tabWidget->setTabEnabled(tabIndex, false);
    ui->tabWidget->setTabToolTip(tabIndex, reason);
    updateMode();
}

void ExportSettingsDialog::hideTab(Mode mode)
{
    NX_ASSERT(ui->tabWidget->count() > 1);
    QWidget* tabToRemove = (mode == Mode::Media ? ui->cameraTab : ui->layoutTab);
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(tabToRemove));
    updateMode();
}

void ExportSettingsDialog::accept()
{
    auto filenamePanel = d->mode() == Mode::Media
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

        suggested.name = lit("%1 (%2)").arg(baseName.name).arg(i);
    }

    NX_ASSERT(false, "Failed to generate suggested filename");
    return baseName;
}

} // namespace nx::vms::client::desktop

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::vms::client::desktop::ExportSettingsDialog, Mode)
