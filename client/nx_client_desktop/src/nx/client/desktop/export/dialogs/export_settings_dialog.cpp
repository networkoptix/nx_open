#include "export_settings_dialog.h"
#include "private/export_settings_dialog_p.h"
#include "ui_export_settings_dialog.h"

#include <limits>

#include <camera/single_thumbnail_loader.h>
#include <client/client_runtime_settings.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <ui/common/palette.h>
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
#include <nx/client/desktop/utils/layout_thumbnail_loader.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/app_info.h>

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
    QnMediaResourceWidget* widget,
    bool allowLayoutExport,
    const QnTimePeriod& timePeriod,
    FileNameValidator isFileNameValid,
    QWidget* parent)
    :
    ExportSettingsDialog(timePeriod, QnCameraBookmark(), isFileNameValid, parent)
{
    setMediaParams(widget->resource(), widget->item()->data(), widget->context());
    if (!allowLayoutExport)
        hideTab(Mode::Layout);

    const auto layout = widget->item()->layout()->resource();
    d->setLayout(layout);
    ui->layoutPreviewWidget->setImageProvider(d->layoutImageProvider());

    const auto palette = ui->layoutPreviewWidget->palette();
    d->layoutImageProvider()->setItemBackgroundColor(palette.color(QPalette::Window));
    d->layoutImageProvider()->setFontColor(palette.color(QPalette::WindowText));

    auto baseName = nx::utils::replaceNonFileNameCharacters(layout->getName(), L' ');
    if (qnRuntime->isActiveXMode() || baseName.isEmpty())
        baseName = tr("exported");
    Filename baseFileName = d->exportLayoutSettings().filename;
    baseFileName.name = baseName;
    ui->layoutFilenamePanel->setFilename(suggestedFileName(baseFileName));
}

ExportSettingsDialog::ExportSettingsDialog(
    const QnMediaResourcePtr& mediaResource,
    QnWorkbenchContext* context,
    const QnTimePeriod& timePeriod,
    FileNameValidator isFileNameValid,
    QWidget* parent)
    :
    ExportSettingsDialog(timePeriod, QnCameraBookmark(), isFileNameValid, parent)
{
    setMediaParams(mediaResource, QnLayoutItemData(), context);
    hideTab(Mode::Layout);
}

ExportSettingsDialog::ExportSettingsDialog(
    QnMediaResourceWidget* widget,
    const QnCameraBookmark& bookmark,
    FileNameValidator isFileNameValid,
    QWidget* parent)
    :
    ExportSettingsDialog({bookmark.startTimeMs, bookmark.durationMs}, bookmark, isFileNameValid, parent)
{
    setMediaParams(widget->resource(), widget->item()->data(), widget->context());
    hideTab(Mode::Layout);
}

ExportSettingsDialog::ExportSettingsDialog(
    const QnMediaResourcePtr& mediaResource,
    QnWorkbenchContext* context,
    const QnCameraBookmark& bookmark,
    FileNameValidator isFileNameValid,
    QWidget* parent)
    :
    ExportSettingsDialog({bookmark.startTimeMs, bookmark.durationMs}, bookmark, isFileNameValid, parent)
{
    setMediaParams(mediaResource, QnLayoutItemData(), context);
    hideTab(Mode::Layout);
}

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
    ui->setupUi(this);

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

    ui->mediaFrame->setFixedSize(kPreviewSize
        + QSize(ui->mediaFrame->frameWidth(), ui->mediaFrame->frameWidth()) * 2);
    ui->layoutFrame->setFixedSize(kPreviewSize
        + QSize(ui->mediaFrame->frameWidth(), ui->mediaFrame->frameWidth()) * 2);

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

    ui->bookmarkButton->setVisible(bookmark.isValid());
    if (!bookmark.isValid()) // Just in case...
        ui->bookmarkButton->setState(ui::SelectableTextButton::State::deactivated);

    ui->speedButton->setState(d->storedRapidReviewSettings().enabled
        ? ui::SelectableTextButton::State::unselected
        : ui::SelectableTextButton::State::deactivated);

    connect(ui->exportMediaSettingsPage, &ExportMediaSettingsWidget::dataChanged,
        d, &Private::setApplyFilters);

    connect(ui->exportLayoutSettingsPage, &ExportLayoutSettingsWidget::dataChanged,
        d, &Private::setLayoutReadOnly);

    connect(ui->layoutFilenamePanel, &FilenamePanel::filenameChanged, d, &Private::setLayoutFilename);
    connect(ui->mediaFilenamePanel, &FilenamePanel::filenameChanged, this,
        [this](const Filename& fileName)
        {
            d->setMediaFilename(fileName);
            ui->transcodingButtonsWidget->setHidden(
                FileExtensionUtils::isExecutable(fileName.extension));
        });

    connect(ui->timestampSettingsPage, &TimestampOverlaySettingsWidget::dataChanged,
        d, &Private::setTimestampOverlaySettings);

    connect(ui->imageSettingsPage, &ImageOverlaySettingsWidget::dataChanged,
        d, &Private::setImageOverlaySettings);

    connect(ui->textSettingsPage, &TextOverlaySettingsWidget::dataChanged,
        d, &Private::setTextOverlaySettings);

    connect(ui->bookmarkSettingsPage, &BookmarkOverlaySettingsWidget::dataChanged,
        d, &Private::setBookmarkOverlaySettings);

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
        [this](bool transcodingIsAllowed)
        {
            ui->exportMediaSettingsPage->setTranscodingAllowed(transcodingIsAllowed);
            if (transcodingIsAllowed)
            {
                ui->exportMediaSettingsPage->setApplyFilters(
                    d->exportMediaPersistentSettings().applyFilters);
            }
            else
            {
                ui->cameraExportSettingsButton->click();
            }
        });

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
        ui->cameraExportSettingsButton->click(); //< Set current page to settings.
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
    ui->exportLayoutSettingsPage->setLayoutReadOnly(d->exportLayoutPersistentSettings().readOnly);
    ui->exportMediaSettingsPage->setApplyFilters(d->exportMediaPersistentSettings().applyFilters);
    ui->timestampSettingsPage->setData(d->exportMediaPersistentSettings().timestampOverlay);
    ui->timestampSettingsPage->setFormatEnabled(d->mediaSupportsUtc());
    ui->bookmarkSettingsPage->setData(d->exportMediaPersistentSettings().bookmarkOverlay);
    ui->imageSettingsPage->setData(d->exportMediaPersistentSettings().imageOverlay);
    ui->textSettingsPage->setData(d->exportMediaPersistentSettings().textOverlay);
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
    const auto currentMode = ui->tabWidget->currentWidget() == ui->cameraTab
        ? Mode::Media
        : Mode::Layout;

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

void ExportSettingsDialog::setMediaParams(
    const QnMediaResourcePtr& mediaResource,
    const QnLayoutItemData& itemData,
    QnWorkbenchContext* context)
{
    d->setMediaResource(mediaResource);
    ui->mediaPreviewWidget->setImageProvider(d->mediaImageProvider());

    const auto timeWatcher = context->instance<QnWorkbenchServerTimeWatcher>();
    d->setServerTimeZoneOffsetMs(timeWatcher->utcOffset(mediaResource, Qn::InvalidUtcOffset));

    const auto timestampOffsetMs = timeWatcher->displayOffset(mediaResource);
    d->setTimestampOffsetMs(timestampOffsetMs);

    updateSettingsWidgets();

    const auto resource = mediaResource->toResourcePtr();
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    const auto customAr = mediaResource->customAspectRatio();

    nx::core::transcoding::Settings available;
    available.rotation = resource->hasProperty(QnMediaResource::rotationKey())
        ? resource->getProperty(QnMediaResource::rotationKey()).toInt()
        : 0;
    available.aspectRatio = qFuzzyIsNull(customAr)
        ? QnAspectRatio()
        : QnAspectRatio::closestStandardRatio(customAr);
    available.enhancement = itemData.contrastParams;
    available.dewarping = itemData.dewarpingParams;
    available.zoomWindow = itemData.zoomRect;
    d->setAvailableTranscodingSettings(available);

    const auto currentSettings = d->exportMediaSettings();
    const auto namePart = resource->getName();
    const auto timePart = resource->hasFlags(Qn::utc)
        ? QDateTime::fromMSecsSinceEpoch(currentSettings.timePeriod.startTimeMs
            + timestampOffsetMs).toString(lit("yyyy_MMM_dd_hh_mm_ss"))
        : QTime(0, 0, 0, 0).addMSecs(currentSettings.timePeriod.startTimeMs)
            .toString(lit("hh_mm_ss"));

    Filename baseFileName = currentSettings.fileName;
    baseFileName.name = nx::utils::replaceNonFileNameCharacters(
        namePart + L'_' + timePart, L'_');

    ui->mediaFilenamePanel->setFilename(suggestedFileName(baseFileName));
}

void ExportSettingsDialog::hideTab(Mode mode)
{
    NX_ASSERT(ui->tabWidget->count() > 1);
    ui->tabWidget->removeTab(mode == Mode::Media
        ? ui->tabWidget->indexOf(ui->cameraTab)
        : ui->tabWidget->indexOf(ui->layoutTab));

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
