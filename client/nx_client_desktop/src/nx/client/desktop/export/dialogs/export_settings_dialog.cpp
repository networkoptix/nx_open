#include "export_settings_dialog.h"
#include "private/export_settings_dialog_p.h"
#include "ui_export_settings_dialog.h"

#include <camera/single_thumbnail_loader.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/busy_indicator.h>
#include <ui/widgets/common/alert_bar.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/event_processors.h>
#include <nx/client/desktop/ui/common/selectable_text_button_group.h>
#include <nx/client/desktop/utils/layout_thumbnail_loader.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

namespace {

static const QSize kPreviewSize(512, 288);
static constexpr int kBusyIndicatorDotRadius = 8;
static constexpr int kNoDataDefaultFontSize = 18;

void addAlert(QLayout* layout, const QString& text)
{
    NX_EXPECT(!text.isEmpty());
    if (text.isEmpty())
        return;

    auto alert = new QnAlertBar();
    alert->setText(text);
    layout->addWidget(alert);
}

} // namespace

ExportSettingsDialog::ExportSettingsDialog(
    QnMediaResourceWidget* widget,
    const QnTimePeriod& timePeriod,
    QWidget* parent)
    :
    ExportSettingsDialog(timePeriod, parent)
{
    setMediaResource(widget->resource());

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
    ExportSettingsDialog(timePeriod, parent)
{
    ui->layoutTab->setVisible(false);
    setMediaResource(media);
}

ExportSettingsDialog::ExportSettingsDialog(const QnTimePeriod& timePeriod, QWidget* parent) :
    base_type(parent),
    d(new Private(kPreviewSize)),
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
    autoResizePagesToContents(ui->alertsStackedWidget,
        QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed), true);

    connect(ui->tabWidget, &QTabWidget::currentChanged,
        ui->alertsStackedWidget, &QStackedWidget::setCurrentIndex);

    d->createOverlays(ui->mediaPreviewWidget);

    setupSettingsButtons();

    auto exportButton = ui->buttonBox->addButton(tr("Export"), QDialogButtonBox::AcceptRole);
    setAccentStyle(exportButton);

    connect(d, &Private::statusChanged, this,
        [this, exportButton](Private::ErrorCode value)
        {
            exportButton->setEnabled(Private::isExportAllowed(value));
        });

    d->loadSettings();

    d->setTimePeriod(timePeriod);

    if (timePeriod.durationMs < RapidReviewSettingsWidget::minimalSourcePeriodLength())
        ui->speedButton->setHidden(true);
    else
        ui->rapidReviewSettingsPage->setSourcePeriodLengthMs(timePeriod.durationMs);

    updateSettingsWidgets();

    // TODO: #GDM bound to tab change
    QStringList filters{
        lit("Matroska (*.mkv)"),
        lit("MP4 (*.mp4)"),
        lit("AVI (*.avi)") };

    ui->filenamePanel->setAllowedExtensions(filters);
    connect(ui->filenamePanel, &FilenamePanel::filenameChanged, d, &Private::setFilename);

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
        ui->timestampButton, &SelectableTextButton::deactivate);

    connect(ui->imageSettingsPage, &ImageOverlaySettingsWidget::deleteClicked,
        ui->imageButton, &SelectableTextButton::deactivate);

    connect(ui->textSettingsPage, &TextOverlaySettingsWidget::deleteClicked,
        ui->textButton, &SelectableTextButton::deactivate);

    connect(ui->bookmarkSettingsPage, &BookmarkOverlaySettingsWidget::deleteClicked,
        ui->bookmarkButton, &SelectableTextButton::deactivate);

    connect(ui->rapidReviewSettingsPage, &RapidReviewSettingsWidget::deleteClicked,
        ui->speedButton, &SelectableTextButton::deactivate);
}

void ExportSettingsDialog::setupSettingsButtons()
{
    static const auto kPagePropertyName = "_qn_ExportSettingsPage";
    static const auto kOverlayPropertyName = "_qn_ExportSettingsOverlay";

    ui->cameraExportSettingsButton->setText(tr("Export Settings"));
    ui->cameraExportSettingsButton->setIcon(qnSkin->icon(
        lit("buttons/settings_hovered.png"),
        lit("buttons/settings_selected.png")));
    ui->cameraExportSettingsButton->setState(SelectableTextButton::State::selected);
    ui->cameraExportSettingsButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->exportMediaSettingsPage));

    ui->layoutExportSettingsButton->setText(tr("Export Settings"));
    ui->layoutExportSettingsButton->setIcon(qnSkin->icon(
        lit("buttons/settings_hovered.png"),
        lit("buttons/settings_selected.png")));
    ui->layoutExportSettingsButton->setState(SelectableTextButton::State::selected);

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
        qVariantFromValue(d->overlay(Private::OverlayType::timestamp)));
    connect(d->overlay(Private::OverlayType::timestamp), &ExportOverlayWidget::pressed,
        ui->timestampButton, &SelectableTextButton::click);

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
        qVariantFromValue(d->overlay(Private::OverlayType::image)));
    connect(d->overlay(Private::OverlayType::image), &ExportOverlayWidget::pressed,
        ui->imageButton, &SelectableTextButton::click);

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
        qVariantFromValue(d->overlay(Private::OverlayType::text)));
    connect(d->overlay(Private::OverlayType::text), &ExportOverlayWidget::pressed,
        ui->textButton, &SelectableTextButton::click);

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
        qVariantFromValue(d->overlay(Private::OverlayType::bookmark)));
    connect(d->overlay(Private::OverlayType::bookmark), &ExportOverlayWidget::pressed,
        ui->bookmarkButton, &SelectableTextButton::click);

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
            NX_EXPECT(button);
            const auto overlay = button->property(kOverlayPropertyName).value<ExportOverlayWidget*>();
            if (overlay)
            {
                const bool selected = button->state() == SelectableTextButton::State::selected;
                overlay->setHidden(button->state() == SelectableTextButton::State::deactivated);
                overlay->setBorderVisible(selected);
                if (selected)
                    overlay->raise();
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
    ui->timestampSettingsPage->setData(d->timestampOverlaySettings());
    ui->bookmarkSettingsPage->setData(d->bookmarkOverlaySettings());
    ui->imageSettingsPage->setData(d->imageOverlaySettings());
    ui->textSettingsPage->setData(d->textOverlaySettings());
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

void ExportSettingsDialog::addSingleCameraAlert(const QString& text)
{
    addAlert(ui->mediaAlertsLayout, text);
}

void ExportSettingsDialog::addMultiVideoAlert(const QString& text)
{
    addAlert(ui->layoutAlertsLayout, text);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
