#include "export_settings_dialog.h"
#include "private/export_settings_dialog_p.h"
#include "ui_export_settings_dialog.h"

#include <camera/single_thumbnail_loader.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>

#include <ui/common/palette.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/busy_indicator.h>
#include <ui/widgets/common/alert_bar.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/event_processors.h>
#include <utils/math/math.h>
#include <nx/client/desktop/ui/common/selectable_text_button_group.h>
#include <nx/client/desktop/utils/layout_thumbnail_loader.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static const QSize kPreviewSize(512, 288);
static constexpr int kBusyIndicatorDotRadius = 8;
static constexpr int kNoDataDefaultFontSize = 18;

} // namespace

ExportSettingsDialog::ExportSettingsDialog(
    QnMediaResourceWidget* widget,
    const QnTimePeriod& timePeriod,
    QWidget* parent)
    :
    ExportSettingsDialog(timePeriod, false /*isBookmark*/, parent)
{
    setMediaResource(widget->resource());

    const auto resource = widget->resource()->toResourcePtr();
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    const auto itemData = widget->item()->data();

    nx::core::transcoding::Settings available;
    available.rotation = resource->hasProperty(QnMediaResource::rotationKey())
        ? resource->getProperty(QnMediaResource::rotationKey()).toInt()
        : 0;
    available.aspectRatio = camera ? camera->aspectRatio() : QnAspectRatio();
    available.enhancement = itemData.contrastParams;
    available.dewarping = itemData.dewarpingParams;
    available.zoomWindow = itemData.zoomRect;
    d->setAvailableTranscodingSettings(available);

    // TODO: #vkutin #GDM Put this layout part elsewhere.

    const auto layout = widget->item()->layout()->resource();
    d->setLayout(layout);
    ui->layoutPreviewWidget->setImageProvider(d->layoutImageProvider());

    const auto palette = ui->layoutPreviewWidget->palette();
    d->layoutImageProvider()->setItemBackgroundColor(palette.color(QPalette::Window));
    d->layoutImageProvider()->setFontColor(palette.color(QPalette::WindowText));
}

ExportSettingsDialog::ExportSettingsDialog(
    const QnMediaResourcePtr& media,
    const QnTimePeriod& timePeriod,
    QWidget* parent)
    :
    ExportSettingsDialog(timePeriod, true /*isBookmark*/, parent)
{
    ui->layoutTab->setVisible(false);
    setMediaResource(media);
}

ExportSettingsDialog::ExportSettingsDialog(
    const QnTimePeriod& timePeriod,
    bool isBookmark,
    QWidget* parent)
    :
    base_type(parent),
    d(new Private(isBookmark, kPreviewSize)),
    ui(new Ui::ExportSettingsDialog)
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

    autoResizePagesToContents(ui->tabWidget,
        QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed), true);

    d->createOverlays(ui->mediaPreviewWidget);

    setupSettingsButtons();

    auto exportButton = ui->buttonBox->addButton(tr("Export"), QDialogButtonBox::AcceptRole);
    setAccentStyle(exportButton);

    connect(d.data(), &Private::validated, this, &ExportSettingsDialog::updateAlerts);

    d->loadSettings();
    connect(this, &QDialog::accepted, d, &Private::saveSettings);

    d->setTimePeriod(timePeriod);

    if (timePeriod.durationMs < RapidReviewSettingsWidget::minimalSourcePeriodLength())
    {
        ui->speedButton->setState(ui::SelectableTextButton::State::deactivated);
        ui->speedButton->setDisabled(true); //TODO: #vkutin #gdm Also show alert.
    }
    else
    {
        ui->rapidReviewSettingsPage->setSourcePeriodLengthMs(timePeriod.durationMs);
    }

    ui->mediaFilenamePanel->setAllowedExtensions(d->allowedFileExtensions(Mode::Media));
    ui->layoutFilenamePanel->setAllowedExtensions(d->allowedFileExtensions(Mode::Layout));

    updateSettingsWidgets();

    ui->bookmarkButton->setVisible(isBookmark);
    if (!isBookmark) // Just in case...
        ui->bookmarkButton->setState(ui::SelectableTextButton::State::deactivated);

    connect(ui->exportMediaSettingsPage, &ExportMediaSettingsWidget::dataChanged,
        d, &Private::setApplyFilters);

    connect(ui->mediaFilenamePanel, &FilenamePanel::filenameChanged, d, &Private::setMediaFilename);
    connect(ui->layoutFilenamePanel, &FilenamePanel::filenameChanged, d, &Private::setLayoutFilename);

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
            d->setRapidReviewFrameStep(frameStepMs);
            ui->speedButton->setText(lit("%1 %3 %2x").arg(tr("Speed")).arg(absoluteSpeed).
                arg(QChar(L'\x2013'))); //< N-dash
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
    updateMode();

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
        qVariantFromValue(settings::ExportOverlayType::timestamp));

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
        qVariantFromValue(settings::ExportOverlayType::image));

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
        qVariantFromValue(settings::ExportOverlayType::text));

    ui->speedButton->setDeactivatable(true);
    ui->speedButton->setDeactivatedText(tr("Speed Up"));
    ui->speedButton->setDeactivationToolTip(tr("Reset Speed"));
    ui->speedButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/rapid_review.png")));
    ui->speedButton->setIcon(qnSkin->icon(
        lit("buttons/rapid_review_hovered.png"),
        lit("buttons/rapid_review_selected.png")));
    ui->speedButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->rapidReviewSettingsPage));

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
        qVariantFromValue(settings::ExportOverlayType::bookmark));

    const auto buttonForType =
        [this](settings::ExportOverlayType type)
        {
            switch (type)
            {
                case settings::ExportOverlayType::timestamp:
                    return ui->timestampButton;
                case settings::ExportOverlayType::image:
                    return ui->imageButton;
                case settings::ExportOverlayType::text:
                    return ui->textButton;
                case settings::ExportOverlayType::bookmark:
                    return ui->bookmarkButton;
                default:
                    return static_cast<ui::SelectableTextButton*>(nullptr);
            }
        };

    connect(d, &Private::overlaySelected, this,
        [buttonForType](settings::ExportOverlayType type)
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
            const auto overlayType = overlayTypeVariant.canConvert<settings::ExportOverlayType>()
                ? overlayTypeVariant.value<settings::ExportOverlayType>()
                : settings::ExportOverlayType::none;

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
    ui->timestampSettingsPage->setData(d->exportMediaSettings().timestampOverlay);
    ui->bookmarkSettingsPage->setData(d->exportMediaSettings().bookmarkOverlay);
    ui->imageSettingsPage->setData(d->exportMediaSettings().imageOverlay);
    ui->textSettingsPage->setData(d->exportMediaSettings().textOverlay);
    ui->mediaFilenamePanel->setFilename(d->exportMediaSettings().fileName);
    ui->layoutFilenamePanel->setFilename(d->exportLayoutSettings().filename);
}

void ExportSettingsDialog::updateMode()
{
    const auto currentMode = ui->tabWidget->currentWidget() == ui->cameraTab
        ? Mode::Media
        : Mode::Layout;

    d->setMode(currentMode);
}

void ExportSettingsDialog::updateAlerts(Mode mode, const QStringList& texts)
{
    QLayout* layout = (mode == Mode::Media)
        ? ui->mediaAlertsLayout
        : ui->layoutAlertsLayout;

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
            layout->addWidget(new QnAlertBar());
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

void ExportSettingsDialog::setMediaResource(const QnMediaResourcePtr& media)
{
    d->setMediaResource(media);
    ui->mediaPreviewWidget->setImageProvider(d->mediaImageProvider());

    ui->bookmarkSettingsPage->setMaxOverlayWidth(d->fullFrameSize().width());
    ui->imageSettingsPage->setMaxOverlayWidth(d->fullFrameSize().width());
    ui->textSettingsPage->setMaxOverlayWidth(d->fullFrameSize().width());

    updateSettingsWidgets();
}

void ExportSettingsDialog::accept()
{
    if (d->mode() == Mode::Media && !ui->mediaFilenamePanel->validate())
        return;

    if (d->mode() == Mode::Layout && !ui->layoutFilenamePanel->validate())
        return;

    base_type::accept();
}

} // namespace desktop
} // namespace client
} // namespace nx
