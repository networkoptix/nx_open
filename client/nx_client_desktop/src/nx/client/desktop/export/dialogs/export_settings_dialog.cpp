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
#include <ui/common/palette.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/busy_indicator.h>
#include <ui/widgets/common/alert_bar.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <utils/common/event_processors.h>
#include <utils/math/math.h>
#include <nx/client/desktop/ui/common/selectable_text_button_group.h>
#include <nx/client/desktop/image_providers/layout_thumbnail_loader.h>
#include <nx/fusion/model_functions.h>

namespace nx {
namespace client {
namespace desktop {

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
    QWidget* parent)
    :
    base_type(parent),
    d(new Private(bookmark, kPreviewSize)),
    ui(new Ui::ExportSettingsDialog),
    isFileNameValid(isFileNameValid)
{

    qDebug() << "ExportSettingsDialog::ExportSettingsDialog(...) start";
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

    updateSettingsWidgets();

    connect(ui->exportMediaSettingsPage, &ExportMediaSettingsWidget::dataChanged,
        d, &Private::setApplyFilters);
    connect(ui->exportLayoutSettingsPage, &ExportLayoutSettingsWidget::dataChanged,
        d, &Private::setLayoutReadOnly);

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
        ui->bookmarkButton->setState(ui::SelectableTextButton::State::deactivated);

    ui->speedButton->setState(d->storedRapidReviewSettings().enabled
        ? ui::SelectableTextButton::State::unselected
        : ui::SelectableTextButton::State::deactivated);

    const auto updateRapidReviewData =
        [this](int absoluteSpeed, qint64 frameStepMs)
        {
            ui->speedButton->setText(lit("%1 %3 %2x").arg(tr("Rapid Review")).arg(absoluteSpeed).
                arg(QChar(L'\x2013'))); //< N-dash

            if (ui->speedButton->state() == ui::SelectableTextButton::State::deactivated)
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
        ui->timestampButton, &ui::SelectableTextButton::deactivate);

    connect(ui->imageSettingsPage, &ImageOverlaySettingsWidget::deleteClicked,
        ui->imageButton, &ui::SelectableTextButton::deactivate);

    connect(ui->textSettingsPage, &TextOverlaySettingsWidget::deleteClicked,
        ui->textButton, &ui::SelectableTextButton::deactivate);

    connect(ui->bookmarkSettingsPage, &BookmarkOverlaySettingsWidget::deleteClicked,
        ui->bookmarkButton, &ui::SelectableTextButton::deactivate);

    connect(ui->rapidReviewSettingsPage, &RapidReviewSettingsWidget::deleteClicked,
        ui->speedButton, &ui::SelectableTextButton::deactivate);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &ExportSettingsDialog::updateMode);

    connect(d, &Private::transcodingAllowedChanged, this,
        &ExportSettingsDialog::updateTranscodingWidgets);
    updateTranscodingWidgets(d->isTranscodingRequested());

    connect(d, &Private::frameSizeChanged, this,
        [this](const QSize& size)
        {
            setMaxOverlayWidth(ui->bookmarkSettingsPage, size.width());
            setMaxOverlayWidth(ui->imageSettingsPage, size.width());
            setMaxOverlayWidth(ui->textSettingsPage, size.width());
            updateSettingsWidgets();
        });

    if (ui->bookmarkButton->state() != ui::SelectableTextButton::State::deactivated)
        ui->bookmarkButton->click(); //< Set current page to bookmark info.
    else
        ui->cameraExportSettingsButton->click(); //< Set current page to settings

    qDebug() << "ExportSettingsDialog::ExportSettingsDialog(...) exit";
}

void ExportSettingsDialog::setupSettingsButtons()
{
    static const auto kPagePropertyName = "_qn_ExportSettingsPage";
    static const auto kOverlayPropertyName = "_qn_ExportSettingsOverlay";

    ui->cameraExportSettingsButton->setText(tr("Export Settings"));
    ui->cameraExportSettingsButton->setIcon(qnSkin->icon(
        lit("buttons/settings_hovered.png"),
        lit("buttons/settings_selected.png")));
    ui->cameraExportSettingsButton->setState(ui::SelectableTextButton::State::selected);
    ui->cameraExportSettingsButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->exportMediaSettingsPage));

    ui->layoutExportSettingsButton->setText(tr("Export Settings"));
    ui->layoutExportSettingsButton->setIcon(qnSkin->icon(
        lit("buttons/settings_hovered.png"),
        lit("buttons/settings_selected.png")));
    ui->layoutExportSettingsButton->setState(ui::SelectableTextButton::State::selected);

    ui->timestampButton->setDeactivatable(true);
    ui->timestampButton->setDeactivatedText(tr("Add Timestamp"));
    ui->timestampButton->setDeactivationToolTip(tr("Delete Timestamp"));
    ui->timestampButton->setText(tr("Timestamp"));
    ui->timestampButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/timestamp.png")));
    ui->timestampButton->setIcon(qnSkin->icon(
        lit("buttons/timestamp_hovered.png"),
        lit("buttons/timestamp_selected.png")));
    ui->timestampButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->timestampSettingsPage));
    ui->timestampButton->setProperty(kOverlayPropertyName,
        qVariantFromValue(ExportOverlayType::timestamp));

    ui->imageButton->setDeactivatable(true);
    ui->imageButton->setDeactivatedText(tr("Add Image"));
    ui->imageButton->setDeactivationToolTip(tr("Delete Image"));
    ui->imageButton->setText(tr("Image"));
    ui->imageButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/image.png")));
    ui->imageButton->setIcon(qnSkin->icon(
        lit("buttons/image_hovered.png"),
        lit("buttons/image_selected.png")));
    ui->imageButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->imageSettingsPage));
    ui->imageButton->setProperty(kOverlayPropertyName,
        qVariantFromValue(ExportOverlayType::image));

    ui->textButton->setDeactivatable(true);
    ui->textButton->setDeactivatedText(tr("Add Text"));
    ui->textButton->setDeactivationToolTip(tr("Delete Text"));
    ui->textButton->setText(tr("Text"));
    ui->textButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/text.png")));
    ui->textButton->setIcon(qnSkin->icon(
        lit("buttons/text_hovered.png"),
        lit("buttons/text_selected.png")));
    ui->textButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->textSettingsPage));
    ui->textButton->setProperty(kOverlayPropertyName,
        qVariantFromValue(ExportOverlayType::text));

    ui->speedButton->setDeactivatable(true);
    ui->speedButton->setDeactivatedText(tr("Rapid Review"));
    ui->speedButton->setDeactivationToolTip(tr("Reset Speed"));
    ui->speedButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/rapid_review.png")));
    ui->speedButton->setIcon(qnSkin->icon(
        lit("buttons/rapid_review_hovered.png"),
        lit("buttons/rapid_review_selected.png")));
    ui->speedButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->rapidReviewSettingsPage));
    connect(ui->speedButton, &ui::SelectableTextButton::stateChanged, this,
        [this](ui::SelectableTextButton::State state)
        {
            const bool enabled = state != ui::SelectableTextButton::State::deactivated;
            d->setRapidReviewFrameStep(enabled ? ui->rapidReviewSettingsPage->frameStepMs() : 0);
            d->setStoredRapidReviewSettings({enabled, d->storedRapidReviewSettings().speed});
            ui->transcodingButtonsWidget->layout()->activate();
        });

    ui->bookmarkButton->setDeactivatable(true);
    ui->bookmarkButton->setDeactivatedText(tr("Add Bookmark Info"));
    ui->bookmarkButton->setDeactivationToolTip(tr("Delete Bookmark Info"));
    ui->bookmarkButton->setText(tr("Bookmark Info"));
    ui->bookmarkButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/bookmark.png")));
    ui->bookmarkButton->setIcon(qnSkin->icon(
        lit("buttons/bookmark_hovered.png"),
        lit("buttons/bookmark_selected.png")));
    ui->bookmarkButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->bookmarkSettingsPage));
    ui->bookmarkButton->setProperty(kOverlayPropertyName,
        qVariantFromValue(ExportOverlayType::bookmark));

    const auto buttonForType =
        [this](ExportOverlayType type)
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
                    return static_cast<ui::SelectableTextButton*>(nullptr);
            }
        };

    connect(d, &Private::overlaySelected, this,
        [buttonForType](ExportOverlayType type)
        {
            auto button = buttonForType(type);
            if (button && button->state() != ui::SelectableTextButton::State::selected)
                button->click();
        });

    auto group = new ui::SelectableTextButtonGroup(this);
    group->add(ui->cameraExportSettingsButton);
    group->add(ui->bookmarkButton);
    group->add(ui->timestampButton);
    group->add(ui->imageButton);
    group->add(ui->textButton);
    group->add(ui->speedButton);
    group->setSelectionFallback(ui->cameraExportSettingsButton);

    connect(group, &ui::SelectableTextButtonGroup::selectionChanged, this,
        [this](ui::SelectableTextButton* /*old*/, ui::SelectableTextButton* selected)
        {
            if (!selected)
                return;

            const auto page = selected->property(kPagePropertyName).value<QWidget*>();
            ui->mediaExportSettingsWidget->setCurrentWidget(page);
        });

    connect(group, &ui::SelectableTextButtonGroup::buttonStateChanged, this,
        [this](ui::SelectableTextButton* button)
        {
            NX_EXPECT(button);
            const auto overlayTypeVariant = button->property(kOverlayPropertyName);
            const auto overlayType = overlayTypeVariant.canConvert<ExportOverlayType>()
                ? overlayTypeVariant.value<ExportOverlayType>()
                : ExportOverlayType::none;

            switch (button->state())
            {
                case ui::SelectableTextButton::State::deactivated:
                    d->hideOverlay(overlayType);
                    break;

                case ui::SelectableTextButton::State::selected:
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

void ExportSettingsDialog::updateSettingsWidgets()
{
    const auto& mediaPersistentSettings = d->exportMediaPersistentSettings();

    ui->exportLayoutSettingsPage->setLayoutReadOnly(d->exportLayoutPersistentSettings().readOnly);
    ui->exportMediaSettingsPage->setApplyFilters(mediaPersistentSettings.applyFilters);
    ui->timestampSettingsPage->setData(mediaPersistentSettings.timestampOverlay);
    ui->timestampSettingsPage->setFormatEnabled(d->mediaSupportsUtc());
    ui->bookmarkSettingsPage->setData(mediaPersistentSettings.bookmarkOverlay);
    ui->imageSettingsPage->setData(mediaPersistentSettings.imageOverlay);
    ui->textSettingsPage->setData(mediaPersistentSettings.textOverlay);

    ui->rapidReviewSettingsPage->setSpeed(d->storedRapidReviewSettings().speed);
    ui->mediaFilenamePanel->setFilename(d->selectedFileName(Mode::Media));
    ui->layoutFilenamePanel->setFilename(d->selectedFileName(Mode::Layout));
}

void ExportSettingsDialog::updateTabWidgetSize()
{
    // Stupid QTabWidget sizeHint workarounds.
    auto size = ui->tabWidget->minimumSizeHint();
    if (ui->tabWidget->tabBar()->isHidden())
        size.setHeight(size.height() - ui->tabWidget->tabBar()->sizeHint().height());
    ui->tabWidget->setFixedSize(size);
}

void ExportSettingsDialog::updateMode()
{
    const auto cameraTabIndex = ui->tabWidget->indexOf(ui->cameraTab);

    const auto isCameraMode = ui->tabWidget->currentIndex() == cameraTabIndex
        && ui->tabWidget->isTabEnabled(cameraTabIndex);

    const auto currentMode = isCameraMode ? Mode::Media : Mode::Layout;
    d->setMode(currentMode);
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
    // TODO: #vkutin #gdm Properly separate in the future.
    using QnMessageBar = QnPromoBar;

    const auto newCount = texts.count();
    const auto oldCount = layout->count();

    const auto setAlertText =
        [layout](int index, const QString& text)
        {
            auto item = layout->itemAt(index);
            auto bar = item ? qobject_cast<QnAlertBar*>(item->widget()) : nullptr;
            NX_EXPECT(bar);
            if (bar)
                bar->setText(text);
        };

    if (newCount > oldCount)
    {
        // Add new alert bars.
        for (int i = oldCount; i < newCount; ++i)
            layout->addWidget(severe ? new QnAlertBar() : new QnMessageBar());
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

void ExportSettingsDialog::updateTranscodingWidgets(bool unused)
{
    // Gather all data to apply to UI
    bool transcodingLocked = d->exportMediaPersistentSettings().isTranscodingForced();
    bool transcodingChecked = d->exportMediaPersistentSettings().applyFilters;
    bool transcodingIsHidden = d->mode() == Mode::Layout;

    if (d->mode() == Mode::Media && d->isMediaEmpty())
    {
        transcodingLocked = true;
        transcodingChecked = false;
    }

    // Applying data to UI
    ui->exportMediaSettingsPage->setTranscodingAllowed(!transcodingLocked);
    ui->exportMediaSettingsPage->setApplyFilters(transcodingChecked);

    if (transcodingChecked && !transcodingIsHidden)
        ui->cameraExportSettingsButton->click();

    ui->transcodingButtonsWidget->setHidden(transcodingIsHidden);
}

void ExportSettingsDialog::setMediaParams(
    const QnMediaResourcePtr& mediaResource,
    const QnLayoutItemData& itemData,
    QnWorkbenchContext* context)
{
    const auto timeWatcher = context->instance<QnWorkbenchServerTimeWatcher>();
    d->setServerTimeZoneOffsetMs(timeWatcher->utcOffset(mediaResource, Qn::InvalidUtcOffset));

    const auto timestampOffsetMs = timeWatcher->displayOffset(mediaResource);
    d->setTimestampOffsetMs(timestampOffsetMs);

    updateSettingsWidgets();

    const auto resource = mediaResource->toResourcePtr();
    const auto currentSettings = d->exportMediaSettings();
    const auto startTimeMs = currentSettings.timePeriod.startTimeMs;

    QString timePart;
    if (resource->hasFlags(Qn::utc))
    {
        QDateTime time = QDateTime::fromMSecsSinceEpoch(startTimeMs + timestampOffsetMs);
        timePart = time.toString(lit("yyyy_MMM_dd_hh_mm_ss"));
    }
    else
    {
        QTime time = QTime(0, 0, 0, 0).addMSecs(startTimeMs);
        timePart = time.toString(lit("hh_mm_ss"));
    }

    Filename baseFileName = currentSettings.fileName;
    QString namePart = resource->getName();
    baseFileName.name = nx::utils::replaceNonFileNameCharacters(namePart + L'_' + timePart, L'_');
    // Changing filename causes signal that rebuilds filter chain

    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    const auto customAr = mediaResource->customAspectRatio();

    nx::core::transcoding::Settings settings;
    settings.aspectRatio = qFuzzyIsNull(customAr)
        ? QnAspectRatio()
        : QnAspectRatio::closestStandardRatio(customAr);
    settings.enhancement = itemData.contrastParams;
    settings.dewarping = itemData.dewarpingParams;
    settings.zoomWindow = itemData.zoomRect;

    d->setMediaResource(mediaResource, settings);

    // That also emits ExportSettingsDialog::Private::setMediaFilename(const Filename& filename)
    //      and then emits ExportSettingsDialog::Private::transcodingAllowedChanged
    ui->mediaFilenamePanel->setFilename(suggestedFileName(baseFileName));

    // Disabling additional filters if we have no video
    // TODO: this case is mapped to d->isMediaEmpty()
    if (!mediaResource->hasVideo())
    {
        ui->timestampButton->setEnabled(false);
        ui->imageButton->setEnabled(false);
        ui->textButton->setEnabled(false);
        ui->speedButton->setEnabled(false);
        ui->exportMediaSettingsPage->setApplyFilters(true);
        ui->exportMediaSettingsPage->setTranscodingAllowed(false);
    }
}

void ExportSettingsDialog::setLayout(const QnLayoutResourcePtr& layout)
{
    const auto palette = ui->layoutPreviewWidget->palette();
    d->setLayout(layout, palette);
    //ui->layoutPreviewWidget->setImageProvider(d->layoutImageProvider());

    auto baseName = nx::utils::replaceNonFileNameCharacters(layout->getName(), L' ');
    if (qnRuntime->isActiveXMode() || baseName.isEmpty())
        baseName = tr("exported");
    Filename baseFileName = d->exportLayoutSettings().filename;
    baseFileName.name = baseName;
    ui->layoutFilenamePanel->setFilename(suggestedFileName(baseFileName));
}

void ExportSettingsDialog::disableTab(Mode mode, const QString& reason)
{
    NX_EXPECT(ui->tabWidget->count() > 1);

    QWidget* tab = (mode == Mode::Media) ? ui->cameraTab : ui->layoutTab;
    int tabIndex = ui->tabWidget->indexOf(tab);
    ui->tabWidget->setTabEnabled(tabIndex, false);
    ui->tabWidget->setTabToolTip(tabIndex, reason);
    updateMode();
}

void ExportSettingsDialog::hideTab(Mode mode)
{
    NX_EXPECT(ui->tabWidget->count() > 1);
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

    NX_ASSERT(false, Q_FUNC_INFO, "Failed to generate suggested filename");
    return baseName;
}

} // namespace desktop
} // namespace client
} // namespace nx

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::client::desktop::ExportSettingsDialog, Mode)
